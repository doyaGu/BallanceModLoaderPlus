/**
 * @file InputMod.cpp
 * @brief BML_Input Module Entry Point
 *
 * This module:
 * - Hooks CKInputManager to intercept input
 * - Publishes input events via IMC (keyboard, mouse)
 * - Provides token-based input capture service for device blocking
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_input_control.h"

#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

#include "InputHook.h"

#include "CKContext.h"
#include "CKInputManager.h"

struct BML_InputCaptureToken_T {
    uint64_t id{0};
};

namespace {
class InputCaptureCoordinator {
public:
    void SetMainThread(std::thread::id thread_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_MainThread = thread_id;
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Captures.clear();
        m_NextTokenId = 1;
        ApplyStateLocked(0u, -1);
    }

    BML_Result Acquire(const BML_InputCaptureDesc *desc, BML_InputCaptureToken *out_token) {
        if (!desc || desc->struct_size < sizeof(BML_InputCaptureDesc) || !out_token) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        auto *token = new (std::nothrow) BML_InputCaptureToken_T{};
        if (!token) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            token->id = m_NextTokenId++;
            m_Captures.emplace(token->id, CaptureRecord{
                token->id,
                m_NextSequence++,
                desc->flags,
                desc->cursor_visible,
                desc->priority
            });
            RecomputeLocked();
        }

        *out_token = reinterpret_cast<BML_InputCaptureToken>(token);
        return BML_RESULT_OK;
    }

    BML_Result Update(BML_InputCaptureToken token, const BML_InputCaptureDesc *desc) {
        if (!token || !desc || desc->struct_size < sizeof(BML_InputCaptureDesc)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        auto *token_impl = reinterpret_cast<BML_InputCaptureToken_T *>(token);
        auto it = m_Captures.find(token_impl->id);
        if (it == m_Captures.end()) {
            return BML_RESULT_INVALID_HANDLE;
        }

        it->second.flags = desc->flags;
        it->second.cursor_visible = desc->cursor_visible;
        it->second.priority = desc->priority;
        it->second.sequence = m_NextSequence++;
        RecomputeLocked();
        return BML_RESULT_OK;
    }

    BML_Result Release(BML_InputCaptureToken token) {
        if (!token) {
            return BML_RESULT_INVALID_HANDLE;
        }
        BML_Result thread_result = CheckThread();
        if (thread_result != BML_RESULT_OK) {
            return thread_result;
        }

        auto *token_impl = reinterpret_cast<BML_InputCaptureToken_T *>(token);
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Captures.find(token_impl->id);
        if (it == m_Captures.end()) {
            return BML_RESULT_INVALID_HANDLE;
        }

        m_Captures.erase(it);
        RecomputeLocked();
        delete token_impl;
        return BML_RESULT_OK;
    }

    BML_Result Query(BML_InputCaptureState *out_state) const {
        if (!out_state || out_state->struct_size < sizeof(BML_InputCaptureState)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        out_state->effective_flags = m_AppliedFlags;
        out_state->cursor_visible = m_AppliedCursorVisible;
        return BML_RESULT_OK;
    }

private:
    struct CaptureRecord {
        uint64_t id{0};
        uint64_t sequence{0};
        uint32_t flags{0};
        int cursor_visible{-1};
        int32_t priority{0};
    };

    BML_Result CheckThread() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_MainThread == std::thread::id{} || std::this_thread::get_id() == m_MainThread) {
            return BML_RESULT_OK;
        }
        return BML_RESULT_WRONG_THREAD;
    }

    void RecomputeLocked() {
        uint32_t effective_flags = 0u;
        int cursor_visible = m_AppliedCursorVisible;
        bool have_cursor_owner = false;
        int32_t best_priority = 0;
        uint64_t best_sequence = 0;

        for (const auto &[id, capture] : m_Captures) {
            (void) id;
            effective_flags |= capture.flags;
            if (capture.cursor_visible < 0) {
                continue;
            }

            if (!have_cursor_owner ||
                capture.priority > best_priority ||
                (capture.priority == best_priority && capture.sequence > best_sequence)) {
                have_cursor_owner = true;
                best_priority = capture.priority;
                best_sequence = capture.sequence;
                cursor_visible = capture.cursor_visible;
            }
        }

        if (!have_cursor_owner) {
            cursor_visible = -1;
        }

        ApplyStateLocked(effective_flags, cursor_visible);
    }

    void ApplyStateLocked(uint32_t effective_flags, int cursor_visible) {
        const bool wants_keyboard = (effective_flags & BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD) != 0;
        const bool wants_mouse = (effective_flags & BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE) != 0;
        const bool has_cursor_override = cursor_visible >= 0;

        const bool keyboard_applied = (m_AppliedFlags & BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD) != 0;
        const bool mouse_applied = (m_AppliedFlags & BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE) != 0;

        if (wants_keyboard != keyboard_applied) {
            if (wants_keyboard) {
                BML_Input::BlockDevice(BML_Input::INPUT_DEVICE_KEYBOARD);
            } else {
                BML_Input::UnblockDevice(BML_Input::INPUT_DEVICE_KEYBOARD);
            }
        }

        if (wants_mouse != mouse_applied) {
            if (wants_mouse) {
                BML_Input::BlockDevice(BML_Input::INPUT_DEVICE_MOUSE);
            } else {
                BML_Input::UnblockDevice(BML_Input::INPUT_DEVICE_MOUSE);
            }
        }

        if (has_cursor_override && cursor_visible != m_AppliedCursorVisible) {
            BML_Input::ShowCursor(cursor_visible ? TRUE : FALSE);
        } else if (!has_cursor_override && m_AppliedCursorVisible >= 0) {
            BML_Input::ShowCursor(FALSE);
        }

        m_AppliedFlags = effective_flags;
        m_AppliedCursorVisible = cursor_visible;
    }

    mutable std::mutex m_Mutex;
    std::thread::id m_MainThread;
    uint64_t m_NextTokenId{1};
    uint64_t m_NextSequence{1};
    std::unordered_map<uint64_t, CaptureRecord> m_Captures;
    uint32_t m_AppliedFlags{0u};
    int m_AppliedCursorVisible{-1};
};

InputCaptureCoordinator &GetCaptureCoordinator() {
    static InputCaptureCoordinator coordinator;
    return coordinator;
}

BML_Result Input_AcquireCapture(const BML_InputCaptureDesc *desc, BML_InputCaptureToken *out_token) {
    return GetCaptureCoordinator().Acquire(desc, out_token);
}

BML_Result Input_UpdateCapture(BML_InputCaptureToken token, const BML_InputCaptureDesc *desc) {
    return GetCaptureCoordinator().Update(token, desc);
}

BML_Result Input_ReleaseCapture(BML_InputCaptureToken token) {
    return GetCaptureCoordinator().Release(token);
}

BML_Result Input_QueryEffectiveState(BML_InputCaptureState *out_state) {
    return GetCaptureCoordinator().Query(out_state);
}

const BML_InputCaptureInterface kInputCaptureService = {
    BML_IFACE_HEADER(BML_InputCaptureInterface, BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0),
    Input_AcquireCapture,
    Input_UpdateCapture,
    Input_ReleaseCapture,
    Input_QueryEffectiveState
};
} // namespace

class InputMod : public bml::HookModule {
    bml::PublishedInterface m_Published;

    const char *HookLogCategory() const override { return "BML_Input"; }

    bool InitHook(CKContext *ctx) override {
        auto *im = static_cast<CKInputManager *>(
            ctx->GetManagerByGuid(INPUT_MANAGER_GUID));
        if (!im) {
            Services().Log().Warn("CKInputManager not available yet - retrying next Engine/Init");
            return false;
        }
        return BML_Input::InitInputHook(im, Services());
    }

    void ShutdownHook() override {
        BML_Input::ShutdownInputHook();
    }

    BML_Result OnModuleAttach() override {
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [](const bml::imc::Message &) {
            BML_Input::ProcessInput();
        });

        GetCaptureCoordinator().SetMainThread(std::this_thread::get_id());

        m_Published = Publish(BML_INPUT_CAPTURE_INTERFACE_ID,
                              &kInputCaptureService,
                              1,
                              0,
                              0,
                              BML_INTERFACE_FLAG_TOKENIZED | BML_INTERFACE_FLAG_IMMUTABLE);
        if (!m_Published) {
            Services().Log().Error("Failed to register input capture service");
            return BML_RESULT_FAIL;
        }

        return BML_RESULT_OK;
    }

    void OnModuleDetach() override {
        (void)m_Published.Reset();
        GetCaptureCoordinator().Reset();
    }
};

BML_DEFINE_MODULE(InputMod)
