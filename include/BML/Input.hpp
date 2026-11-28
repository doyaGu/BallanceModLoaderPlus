#ifndef BML_INPUT_HPP
#define BML_INPUT_HPP

#include "BML/Input.h"
#include "BML/Mod.hpp"

namespace BML::Input {
struct KeyboardState {
    bool Attached = false;
    bool RepetitionEnabled = false;
    std::span<const BML_InputKeyState> Keys;
    std::span<const BML_InputKeyEvent> Events;

    bool IsKeyDown(uint32_t key) const noexcept { return key < Keys.size() && Keys[key].Down; }

    bool IsKeyUp(uint32_t key) const noexcept { return key >= Keys.size() || Keys[key].Up; }

    bool IsKeyPressed(uint32_t key) const noexcept {
        return key < Keys.size() && Keys[key].Pressed;
    }

    bool IsKeyReleased(uint32_t key) const noexcept {
        return key < Keys.size() && Keys[key].Released;
    }

    bool IsKeyToggled(uint32_t key) const noexcept {
        return key < Keys.size() && Keys[key].Toggled;
    }
};
class View;

class Block {
public:
    Block() noexcept = default;

    ~Block() noexcept { Dispose(); }

    Block(const Block &) = delete;
    Block &operator=(const Block &) = delete;

    Block(Block &&other) noexcept { Move(other); }

    Block &operator=(Block &&other) noexcept {
        if (this != &other) {
            Dispose();
            Move(other);
        }
        return *this;
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_Handle && m_Context.IsValid(); }

    [[nodiscard]] Result<void> Release() noexcept {
        if (!m_Handle)
            return BML::Detail::ToResult(BML_OK);
        const int code = m_Api->ReleaseBlock(m_Context.Handle(), m_Handle);
        // This wrapper can only own handles returned by a successful Acquire.
        // An invalidated owned token has already been removed by owner cleanup.
        if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
            m_Handle = 0;
            return BML::Detail::ToResult(BML_OK);
        }
        return BML::Detail::ToResult(code);
    }

private:
    friend class View;

    Block(Mod::Context ctx, const BML_InputInterface *api, BML_InputBlockHandle handle) noexcept
        : m_Context(ctx), m_Api(api), m_Handle(handle) {}

    void Dispose() noexcept {
        if (!Release()) {
            (void)m_Context.LogError("Input block release failed; retained until owner cleanup");
            m_Handle = 0;
        }
    }

    void Move(Block &other) noexcept {
        m_Context = other.m_Context;
        m_Api = other.m_Api;
        m_Handle = std::exchange(other.m_Handle, 0);
    }

    Mod::Context m_Context;
    const BML_InputInterface *m_Api = nullptr;
    BML_InputBlockHandle m_Handle = 0;
};

class View {
public:
    explicit View(const Mod::Context &context) noexcept
        : m_Context(context),
          m_Api(BML::Detail::Bind<BML_InputInterface>(
              BML_INPUT_INTERFACE_ID, 1, BML_INPUT_INTERFACE_1_0_SIZE,
              +[](const BML_InputInterface *table) {
                  return BML_IFACE_HAS(table, BML_InputInterface, ReadKeyboard) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ReadMouse) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ReadJoystick) &&
                         BML_IFACE_HAS(table, BML_InputInterface, GetKeyName) &&
                         BML_IFACE_HAS(table, BML_InputInterface, GetKeyFromName) &&
                         BML_IFACE_HAS(table, BML_InputInterface, EnableKeyboardRepetition) &&
                         BML_IFACE_HAS(table, BML_InputInterface, AcquireBlock) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ReleaseBlock) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ReadBlockCount) &&
                         BML_IFACE_HAS(table, BML_InputInterface, Pause) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ReadCursor) &&
                         BML_IFACE_HAS(table, BML_InputInterface, ShowCursor) &&
                         BML_IFACE_HAS(table, BML_InputInterface, SetSystemCursor);
              })) {}
    [[nodiscard]] Result<KeyboardState> ReadKeyboard(uint32_t flags = 0) const try {
        BML_InputKeyboardState value{};
        value.StructSize = sizeof(value);
        const int code =
            m_Api ? m_Api->ReadKeyboard(m_Context.Handle(), flags, &value) : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<KeyboardState>::Success({value.Attached != 0,
                                                                value.RepetitionEnabled != 0,
                                                                {value.Keys, value.KeyCount},
                                                                {value.Events, value.EventCount}})
                              : Result<KeyboardState>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<KeyboardState>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<KeyboardState>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<BML_InputMouseState> ReadMouse(uint32_t flags = 0) const try {
        BML_InputMouseState value{};
        value.StructSize = sizeof(value);
        const int code =
            m_Api ? m_Api->ReadMouse(m_Context.Handle(), flags, &value) : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<BML_InputMouseState>::Success(value)
                              : Result<BML_InputMouseState>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<BML_InputMouseState>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<BML_InputMouseState>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<BML_InputJoystickState> ReadJoystick(uint32_t index,
                                                              uint32_t flags = 0) const try {
        BML_InputJoystickState value{};
        value.StructSize = sizeof(value);
        const int code = m_Api ? m_Api->ReadJoystick(m_Context.Handle(), index, flags, &value)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<BML_InputJoystickState>::Success(value)
                              : Result<BML_InputJoystickState>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<BML_InputJoystickState>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<BML_InputJoystickState>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<std::string> GetKeyName(uint32_t key) const try {
        return BML::Detail::ReadBuffer<std::string>([&](char *p, size_t n, size_t *required) {
            return m_Api ? m_Api->GetKeyName(m_Context.Handle(), key, p, n, required)
                         : BML_ERROR_UNAVAILABLE;
        });
    } catch (const std::bad_alloc &) {
        return Result<std::string>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<std::string>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<uint32_t> GetKeyFromName(std::string_view name) const try {
        if (!BML::Detail::IsText(name))
            return Result<uint32_t>::Failure(BML_ERROR_INVALID_PARAMETER);
        const std::string text(name);
        uint32_t key = 0;
        const int code = m_Api ? m_Api->GetKeyFromName(m_Context.Handle(), text.c_str(), &key)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<uint32_t>::Success(key) : Result<uint32_t>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<uint32_t>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<uint32_t>::Failure(BML_ERROR_FAIL);
    }

    [[nodiscard]] Result<void> EnableKeyboardRepetition(bool enable = true) const noexcept {
        return BML::Detail::ToResult(
            m_Api ? m_Api->EnableKeyboardRepetition(m_Context.Handle(), enable ? 1u : 0u)
                  : BML_ERROR_UNAVAILABLE);
    }
    [[nodiscard]] Result<Block> AcquireBlock(uint32_t mask) const try {
        BML_InputBlockHandle block = 0;
        const int code =
            m_Api ? m_Api->AcquireBlock(m_Context.Handle(), mask, &block) : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<Block>::Success(Block(m_Context, m_Api, block))
                              : Result<Block>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<Block>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Block>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<uint32_t> ReadBlockCount(uint32_t device) const try {
        uint32_t count = 0;
        const int code = m_Api ? m_Api->ReadBlockCount(m_Context.Handle(), device, &count)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<uint32_t>::Success(count) : Result<uint32_t>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<uint32_t>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<uint32_t>::Failure(BML_ERROR_FAIL);
    }

    [[nodiscard]] Result<void> Pause(bool pause) const noexcept {
        return BML::Detail::ToResult(m_Api ? m_Api->Pause(m_Context.Handle(), pause ? 1u : 0u)
                                           : BML_ERROR_UNAVAILABLE);
    }
    [[nodiscard]] Result<BML_InputCursorState> ReadCursor() const try {
        BML_InputCursorState value{};
        value.StructSize = sizeof(value);
        const int code =
            m_Api ? m_Api->ReadCursor(m_Context.Handle(), &value) : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<BML_InputCursorState>::Success(value)
                              : Result<BML_InputCursorState>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<BML_InputCursorState>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<BML_InputCursorState>::Failure(BML_ERROR_FAIL);
    }

    [[nodiscard]] Result<void> ShowCursor(bool visible) const noexcept {
        return BML::Detail::ToResult(m_Api
                                         ? m_Api->ShowCursor(m_Context.Handle(), visible ? 1u : 0u)
                                         : BML_ERROR_UNAVAILABLE);
    }

    [[nodiscard]] Result<void> SetSystemCursor(uint32_t shape) const noexcept {
        return BML::Detail::ToResult(m_Api ? m_Api->SetSystemCursor(m_Context.Handle(), shape)
                                           : BML_ERROR_UNAVAILABLE);
    }

private:
    Mod::Context m_Context;
    const BML_InputInterface *m_Api;
};
} // namespace BML::Input

inline BML::Input::View BML::Mod::Context::Input() const noexcept {
    return BML::Input::View(*this);
}
#endif
