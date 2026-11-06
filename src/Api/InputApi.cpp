#include "Api/HandleSequence.h"
#include "Api/InputApi.h"
#include "Api/ModApi.h"
#include "BML/InputHook.h"
#include "Hooks/InputAccess.h"
#include <climits>

namespace BML::Api {
int InputApi::ReadKeyboard(ModContext &ctx, uint32_t flags, BML_InputKeyboardState *out) {
    if (!out || out->StructSize < BML_INPUT_KEYBOARD_STATE_1_0_SIZE || (flags & ~BML_INPUT_RAW))
        return BML_ERROR_INVALID_PARAMETER;
    auto *input = ctx.GetInputManager();
    if (!input)
        return BML_ERROR_UNAVAILABLE;
    const bool attached = input->IsKeyboardAttached() != 0,
               repetition = input->IsKeyboardRepetitionEnabled() != 0;
    const auto *bytes = input->oGetKeyboardState();
    const int count = input->oGetNumberOfKeyInBuffer();
    bool same = m_Raw && m_Raw->Revision == InputHookAccess::Revision(*input) &&
                m_Raw->Attached == attached && m_Raw->Repetition == repetition && count >= 0 &&
                m_Raw->Events.size() == static_cast<size_t>(count) &&
                (!bytes || std::memcmp(m_Raw->Bytes.data(), bytes, 256) == 0);
    for (int i = 0; same && i < count; ++i) {
        CKDWORD key = 0, stamp = 0;
        const int state = input->oGetKeyFromBuffer(i, key, &stamp);
        const auto &e = m_Raw->Events[i];
        same = e.Key == key && e.TimestampMs == stamp && e.State == static_cast<uint32_t>(state);
    }
    if (!same) {
        auto next = m_Raw && m_Raw.use_count() == 1 ? m_Raw : std::make_shared<Keyboard>();
        next->Revision = InputHookAccess::Revision(*input);
        next->Attached = attached;
        next->Repetition = repetition;
        if (bytes)
            std::memcpy(next->Bytes.data(), bytes, 256);
        else
            next->Bytes.fill(0);
        for (uint32_t key = 0; key < 256; ++key) {
            CKDWORD downStamp = 0, toggleStamp = 0;
            auto &k = next->Keys[key];
            k = {};
            k.Key = key;
            if (!attached) {
                k.Up = 1;
                continue;
            }
            k.Down = input->oIsKeyDown(key, &downStamp) ? 1u : 0u;
            k.Up = input->oIsKeyUp(key) ? 1u : 0u;
            k.Toggled = input->oIsKeyToggled(key, &toggleStamp) ? 1u : 0u;
            k.Pressed = input->oIsKeyPressed(key) ? 1u : 0u;
            k.Released = input->oIsKeyReleased(key) ? 1u : 0u;
            k.DownTimestampMs = downStamp;
            k.ToggledTimestampMs = toggleStamp;
        }
        if (count < 0)
            return BML_ERROR_FAIL;
        next->Events.clear();
        next->Events.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            CKDWORD key = 0, stamp = 0;
            const int state = input->oGetKeyFromBuffer(i, key, &stamp);
            next->Events.push_back({key, static_cast<uint32_t>(state), stamp});
        }
        m_Raw = std::move(next);
    }
    std::shared_ptr<Keyboard> result = m_Raw;
    if (!(flags & BML_INPUT_RAW) && input->IsBlocked(CK_INPUT_DEVICE_KEYBOARD)) {
        if (!m_Filtered || m_Filtered->Attached != attached ||
            m_Filtered->Repetition != repetition) {
            auto next = std::make_shared<Keyboard>();
            next->Revision = InputHookAccess::Revision(*input);
            next->Attached = attached;
            next->Repetition = repetition;
            for (uint32_t key = 0; key < 256; ++key) {
                next->Keys[key].Key = key;
                next->Keys[key].Up = 1;
            }
            m_Filtered = std::move(next);
        }
        result = m_Filtered;
    }
    ctx.GetModLoader().InvocationGate().HoldBorrow(result);
    const BML_InputKeyboardState value = {
        sizeof(*out),         result->Attached ? 1u : 0u, result->Repetition ? 1u : 0u,
        result->Keys.data(),  result->Keys.size(),        result->Events.data(),
        result->Events.size()};
    WriteOutput(out, value, BML_INPUT_KEYBOARD_STATE_1_0_SIZE);
    return BML_OK;
}

int InputApi::AcquireBlock(ModContext &ctx, IMod &owner, uint32_t mask, BML_InputBlockHandle *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    if (!mask || (mask & ~BML_INPUT_BLOCK_ALL))
        return BML_ERROR_INVALID_PARAMETER;
    if (!AcceptsModWork(ctx, owner))
        return BML_ERROR_FROZEN;
    auto *input = ctx.GetInputManager();
    if (!input)
        return BML_ERROR_UNAVAILABLE;
    for (uint32_t d = 0; d < 3; ++d)
        if ((mask & (1u << d)) && input->IsBlocked(static_cast<CK_INPUT_DEVICE>(d)) == INT_MAX)
            return BML_ERROR_BUSY;
    static std::atomic<uint64_t> next{1};
    const auto h = BML::Api::NextHandle(next);
    if (!h)
        return BML_ERROR_FAIL;
    auto [entry, inserted] = m_Blocks.emplace(h, Block{&owner, 0});
    if (!inserted)
        return BML_ERROR_FAIL;
    try {
        entry->second.Token = input->AcquireBlock(mask);
    } catch (...) {
        m_Blocks.erase(entry);
        throw;
    }
    if (!entry->second.Token) {
        m_Blocks.erase(entry);
        return BML_ERROR_UNAVAILABLE;
    }
    *out = h;
    return BML_OK;
}

int InputApi::ReleaseBlock(ModContext &ctx, IMod &owner, BML_InputBlockHandle h) {
    const auto it = m_Blocks.find(h);
    if (it == m_Blocks.end())
        return BML_ERROR_INVALID_HANDLE;
    if (it->second.Owner != &owner)
        return BML_ERROR_ACCESS_DENIED;
    auto *input = ctx.GetInputManager();
    if (!input)
        return BML_ERROR_UNAVAILABLE;
    input->ReleaseBlock(it->second.Token);
    m_Blocks.erase(it);
    return BML_OK;
}

void InputApi::CleanupOwner(ModContext &ctx, IMod &owner) noexcept {
    for (auto it = m_Blocks.begin(); it != m_Blocks.end();) {
        if (it->second.Owner != &owner) {
            ++it;
            continue;
        }
        if (auto *input = ctx.GetInputManager())
            input->ReleaseBlock(it->second.Token);
        it = m_Blocks.erase(it);
    }
}

namespace {
template <class Body> int Serve(BML_ModContextHandle h, Body &&body) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        auto *input = ctx.GetInputManager();
        return input ? body(ctx, owner, *input) : BML_ERROR_UNAVAILABLE;
    });
}

int BML_CDECL ReadKeyboard(BML_ModContextHandle h, uint32_t flags, BML_InputKeyboardState *out) {
    return WithModContext(h, [&](ModContext &ctx, IMod &) {
        return ctx.GetInputApi().ReadKeyboard(ctx, flags, out);
    });
}

int BML_CDECL ReadMouse(BML_ModContextHandle h, uint32_t flags, BML_InputMouseState *out) {
    if (!out || out->StructSize < BML_INPUT_MOUSE_STATE_1_0_SIZE || (flags & ~BML_INPUT_RAW))
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        BML_InputMouseState value{};
        value.StructSize = sizeof(value);
        value.Attached = input.IsMouseAttached() ? 1u : 0u;
        Vx2DVector last{};
        input.GetLastMousePosition(last);
        value.LastPosition = {last.x, last.y};
        if (flags & BML_INPUT_RAW)
            InputHookAccess::ReadRawMouse(input, value);
        else if (value.Attached) {
            Vx2DVector screen{}, window{};
            VxVector relative{};
            input.GetMousePosition(screen, TRUE);
            input.GetMousePosition(window, FALSE);
            input.GetMouseRelativePosition(relative);
            value.ScreenPosition = {screen.x, screen.y};
            value.WindowPosition = {window.x, window.y};
            value.RelativePosition = {relative.x, relative.y, relative.z};
            for (uint32_t i = 0; i < 4; ++i) {
                const auto b = static_cast<CK_MOUSEBUTTON>(i);
                if (input.IsMouseButtonDown(b))
                    value.DownMask |= 1u << i;
                if (input.IsMouseClicked(b))
                    value.ClickedMask |= 1u << i;
                if (input.IsMouseToggled(b))
                    value.ToggledMask |= 1u << i;
            }
        }
        WriteOutput(out, value, BML_INPUT_MOUSE_STATE_1_0_SIZE);
        return BML_OK;
    });
}

int BML_CDECL ReadJoystick(BML_ModContextHandle h, uint32_t index, uint32_t flags,
                           BML_InputJoystickState *out) {
    if (!out || out->StructSize < BML_INPUT_JOYSTICK_STATE_1_0_SIZE || index >= CKMAX_JOY ||
        (flags & ~BML_INPUT_RAW))
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        BML_InputJoystickState value{};
        value.StructSize = sizeof(value);
        value.Attached = input.IsJoystickAttached(index) ? 1u : 0u;
        if (flags & BML_INPUT_RAW)
            InputHookAccess::ReadRawJoystick(input, index, value);
        else if (value.Attached) {
            VxVector position{}, rotation{};
            Vx2DVector sliders{};
            input.GetJoystickPosition(index, &position);
            input.GetJoystickRotation(index, &rotation);
            input.GetJoystickSliders(index, &sliders);
            input.GetJoystickPointOfViewAngle(index, &value.PointOfViewAngle);
            value.Position = {position.x, position.y, position.z};
            value.Rotation = {rotation.x, rotation.y, rotation.z};
            value.Sliders = {sliders.x, sliders.y};
            value.Buttons = input.GetJoystickButtonsState(index);
        }
        WriteOutput(out, value, BML_INPUT_JOYSTICK_STATE_1_0_SIZE);
        return BML_OK;
    });
}

int BML_CDECL KeyName(BML_ModContextHandle h, uint32_t key, char *buffer, size_t capacity,
                      size_t *required) {
    if (key >= 256)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        char name[256]{};
        input.GetKeyName(key, name);
        return CopyText(name, buffer, capacity, required);
    });
}

int BML_CDECL KeyFromName(BML_ModContextHandle h, const char *name, uint32_t *out) {
    if (!name || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        *out = input.GetKeyFromName(const_cast<char *>(name));
        return *out && *out < 256 ? BML_OK : BML_ERROR_NOT_FOUND;
    });
}

int BML_CDECL Repetition(BML_ModContextHandle h, uint32_t enable) {
    if (enable > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        input.EnableKeyboardRepetition(enable);
        return BML_OK;
    });
}

int BML_CDECL Acquire(BML_ModContextHandle h, uint32_t mask, BML_InputBlockHandle *out) {
    if (out)
        *out = 0;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetInputApi().AcquireBlock(ctx, owner, mask, out);
    });
}

int BML_CDECL Release(BML_ModContextHandle h, BML_InputBlockHandle block) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return ctx.GetInputApi().ReleaseBlock(ctx, owner, block);
    });
}

int BML_CDECL BlockCount(BML_ModContextHandle h, uint32_t device, uint32_t *out) {
    if (device >= 3 || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        *out = input.IsBlocked(static_cast<CK_INPUT_DEVICE>(device));
        return BML_OK;
    });
}

int BML_CDECL Pause(BML_ModContextHandle h, uint32_t pause) {
    if (pause > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        input.Pause(pause);
        return BML_OK;
    });
}

int BML_CDECL Cursor(BML_ModContextHandle h, BML_InputCursorState *out) {
    if (!out || out->StructSize < BML_INPUT_CURSOR_STATE_1_0_SIZE)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        const BML_InputCursorState value = {sizeof(*out), input.GetCursorVisibility() ? 1u : 0u,
                                            static_cast<uint32_t>(input.GetSystemCursor())};
        WriteOutput(out, value, BML_INPUT_CURSOR_STATE_1_0_SIZE);
        return BML_OK;
    });
}

int BML_CDECL ShowCursor(BML_ModContextHandle h, uint32_t show) {
    if (show > 1)
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        input.ShowCursor(show);
        return BML_OK;
    });
}

int BML_CDECL SystemCursor(BML_ModContextHandle h, uint32_t shape) {
    if (shape < static_cast<uint32_t>(VXCURSOR_NORMALSELECT) ||
        shape > static_cast<uint32_t>(VXCURSOR_LINKSELECT))
        return BML_ERROR_INVALID_PARAMETER;
    return Serve(h, [&](ModContext &, IMod &, InputHook &input) {
        input.SetSystemCursor(static_cast<VXCURSOR_POINTER>(shape));
        return BML_OK;
    });
}

const BML_InputInterface table{BML_IFACE_HEADER(BML_InputInterface, BML_INPUT_INTERFACE_ID, 1, 0),
                               ReadKeyboard,
                               ReadMouse,
                               ReadJoystick,
                               KeyName,
                               KeyFromName,
                               Repetition,
                               Acquire,
                               Release,
                               BlockCount,
                               Pause,
                               Cursor,
                               ShowCursor,
                               SystemCursor};
} // namespace

const BML_InputInterface &InputInterface() {
    return table;
}
} // namespace BML::Api
