#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "bml_console.h"
#include "bml_game_topics.h"
#include "bml_input.h"
#include "bml_input_capture.hpp"
#include "bml_input_control.h"
#include "bml_interface.hpp"
#include "bml_topics.h"

#include "RecorderEngine.h"
#include "PathUtils.h"

namespace fs = std::filesystem;

namespace {

std::string GetRecordingsDir() {
    fs::path dir = utils::GetRuntimeLayout().runtime_directory / L"Recordings";
    return dir.string();
}

std::string GenerateFilename() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "recording_%04d%02d%02d_%02d%02d%02d.bmlr",
                  st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond);
    return buf;
}

EventRecord MakeKeyDownEvent(const BML_KeyDownEvent &e) {
    EventRecord r{};
    r.type = EVENT_KEY_DOWN;
    r.key_down.key_code = e.key_code;
    r.key_down.scan_code = e.scan_code;
    r.key_down.timestamp = e.timestamp;
    r.key_down.repeat = e.repeat ? 1 : 0;
    return r;
}

EventRecord MakeKeyUpEvent(const BML_KeyUpEvent &e) {
    EventRecord r{};
    r.type = EVENT_KEY_UP;
    r.key_up.key_code = e.key_code;
    r.key_up.scan_code = e.scan_code;
    r.key_up.timestamp = e.timestamp;
    return r;
}

EventRecord MakeMouseBtnEvent(const BML_MouseButtonEvent &e) {
    EventRecord r{};
    r.type = EVENT_MOUSE_BTN;
    r.mouse_btn.button = e.button;
    r.mouse_btn.down = e.down ? 1 : 0;
    r.mouse_btn.timestamp = e.timestamp;
    return r;
}

EventRecord MakeMouseMoveEvent(const BML_MouseMoveEvent &e) {
    EventRecord r{};
    r.type = EVENT_MOUSE_MOVE;
    r.mouse_move.x = e.x;
    r.mouse_move.y = e.y;
    r.mouse_move.rel_x = e.rel_x;
    r.mouse_move.rel_y = e.rel_y;
    r.mouse_move.absolute = e.absolute ? 1 : 0;
    r.mouse_move.timestamp = e.timestamp;
    return r;
}

} // namespace

class InputRecorderMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    bml::InputCaptureGuard m_InputCapture;
    bml::InterfaceLease<BML_ConsoleCommandRegistry> m_ConsoleRegistry;
    RecorderEngine m_Engine;
    std::string m_RecordingsDir;

    void InjectEvents(const std::vector<EventRecord> &events) {
        for (const auto &event : events) {
            switch (event.type) {
            case EVENT_KEY_DOWN: {
                BML_KeyDownEvent e{};
                e.key_code = event.key_down.key_code;
                e.scan_code = event.key_down.scan_code;
                e.timestamp = event.key_down.timestamp;
                e.repeat = event.key_down.repeat != 0;
                bml::imc::publish(Handle(), BML_TOPIC_INPUT_KEY_DOWN, e);
                break;
            }
            case EVENT_KEY_UP: {
                BML_KeyUpEvent e{};
                e.key_code = event.key_up.key_code;
                e.scan_code = event.key_up.scan_code;
                e.timestamp = event.key_up.timestamp;
                bml::imc::publish(Handle(), BML_TOPIC_INPUT_KEY_UP, e);
                break;
            }
            case EVENT_MOUSE_BTN: {
                BML_MouseButtonEvent e{};
                e.button = event.mouse_btn.button;
                e.down = event.mouse_btn.down != 0;
                e.timestamp = event.mouse_btn.timestamp;
                bml::imc::publish(Handle(), BML_TOPIC_INPUT_MOUSE_BUTTON, e);
                break;
            }
            case EVENT_MOUSE_MOVE: {
                BML_MouseMoveEvent e{};
                e.x = event.mouse_move.x;
                e.y = event.mouse_move.y;
                e.rel_x = event.mouse_move.rel_x;
                e.rel_y = event.mouse_move.rel_y;
                e.absolute = event.mouse_move.absolute != 0;
                e.timestamp = event.mouse_move.timestamp;
                bml::imc::publish(Handle(), BML_TOPIC_INPUT_MOUSE_MOVE, e);
                break;
            }
            }
        }
    }

    void AcquireInputCapture() {
        if (!m_InputCapture.IsHeld() && m_InputCaptureService) {
            m_InputCapture.Acquire(
                BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD | BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE);
        }
    }

    void ReleaseInputCapture() {
        m_InputCapture.Release();
    }

    void ConsolePrint(const char *msg, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_NONE) {
        if (m_ConsoleRegistry) {
            m_ConsoleRegistry->Print(msg, flags);
        }
    }

    // -- Console command callbacks (static, with user_data = this) --

    static BML_Result ExecuteRecord(const BML_ConsoleCommandInvocation *inv, void *ud) {
        auto *self = static_cast<InputRecorderMod *>(ud);
        if (!inv || inv->argc < 1) return BML_RESULT_INVALID_ARGUMENT;

        const char *sub = inv->argv_utf8[0];

        if (std::strcmp(sub, "start") == 0) {
            if (self->m_Engine.State() != RecorderState::Idle) {
                self->ConsolePrint("Already recording or playing back.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
                return BML_RESULT_OK;
            }

            std::string filename;
            if (inv->argc >= 2) {
                filename = inv->argv_utf8[1];
                if (!filename.ends_with(".bmlr"))
                    filename += ".bmlr";
            } else {
                filename = GenerateFilename();
            }

            fs::create_directories(self->m_RecordingsDir);
            std::string fullPath = (fs::path(self->m_RecordingsDir) / filename).string();

            if (self->m_Engine.StartRecording(fullPath, 0)) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "Recording started: %s", filename.c_str());
                self->ConsolePrint(buf);
            } else {
                self->ConsolePrint("Failed to start recording.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
            }
        } else if (std::strcmp(sub, "stop") == 0) {
            if (self->m_Engine.State() != RecorderState::Recording) {
                self->ConsolePrint("Not recording.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
                return BML_RESULT_OK;
            }
            uint32_t frames = self->m_Engine.FrameCount();
            self->m_Engine.StopRecording();
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Recording stopped. %u frames saved.", frames);
            self->ConsolePrint(buf);
        } else if (std::strcmp(sub, "status") == 0) {
            char buf[256];
            switch (self->m_Engine.State()) {
            case RecorderState::Recording:
                std::snprintf(buf, sizeof(buf), "Recording: frame %u", self->m_Engine.FrameCount());
                break;
            case RecorderState::Playing:
                std::snprintf(buf, sizeof(buf), "Playing: frame %u", self->m_Engine.FrameCount());
                break;
            case RecorderState::Paused:
                std::snprintf(buf, sizeof(buf), "Paused: frame %u", self->m_Engine.FrameCount());
                break;
            default:
                std::snprintf(buf, sizeof(buf), "Idle.");
                break;
            }
            self->ConsolePrint(buf);
        } else {
            self->ConsolePrint("Usage: record <start [filename]|stop|status>", BML_CONSOLE_OUTPUT_FLAG_ERROR);
        }

        return BML_RESULT_OK;
    }

    static void CompleteRecord(const BML_ConsoleCompletionContext *ctx,
                               PFN_BML_ConsoleCompletionSink sink,
                               void *sinkUd, void * /*cmdUd*/) {
        if (!ctx || !sink) return;
        if (ctx->token_index == 0) {
            sink("start", sinkUd);
            sink("stop", sinkUd);
            sink("status", sinkUd);
        }
    }

    static BML_Result ExecutePlayback(const BML_ConsoleCommandInvocation *inv, void *ud) {
        auto *self = static_cast<InputRecorderMod *>(ud);
        if (!inv || inv->argc < 1) return BML_RESULT_INVALID_ARGUMENT;

        const char *sub = inv->argv_utf8[0];

        if (std::strcmp(sub, "start") == 0) {
            if (self->m_Engine.State() != RecorderState::Idle) {
                self->ConsolePrint("Already recording or playing back.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
                return BML_RESULT_OK;
            }
            if (inv->argc < 2) {
                self->ConsolePrint("Usage: playback start <filename>", BML_CONSOLE_OUTPUT_FLAG_ERROR);
                return BML_RESULT_OK;
            }

            std::string filename = inv->argv_utf8[1];
            if (!filename.ends_with(".bmlr"))
                filename += ".bmlr";

            std::string fullPath = (fs::path(self->m_RecordingsDir) / filename).string();
            if (self->m_Engine.StartPlayback(fullPath)) {
                self->AcquireInputCapture();
                char buf[256];
                std::snprintf(buf, sizeof(buf), "Playback started: %s", filename.c_str());
                self->ConsolePrint(buf);
            } else {
                self->ConsolePrint("Failed to start playback. File not found or invalid.",
                                   BML_CONSOLE_OUTPUT_FLAG_ERROR);
            }
        } else if (std::strcmp(sub, "stop") == 0) {
            if (self->m_Engine.State() != RecorderState::Playing &&
                self->m_Engine.State() != RecorderState::Paused) {
                self->ConsolePrint("Not playing back.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
                return BML_RESULT_OK;
            }
            self->m_Engine.StopPlayback();
            self->ReleaseInputCapture();
            self->ConsolePrint("Playback stopped.");
        } else if (std::strcmp(sub, "pause") == 0) {
            if (self->m_Engine.PausePlayback()) {
                self->ConsolePrint("Playback paused.");
            } else {
                self->ConsolePrint("Not playing.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
            }
        } else if (std::strcmp(sub, "resume") == 0) {
            if (self->m_Engine.ResumePlayback()) {
                self->ConsolePrint("Playback resumed.");
            } else {
                self->ConsolePrint("Not paused.", BML_CONSOLE_OUTPUT_FLAG_ERROR);
            }
        } else if (std::strcmp(sub, "status") == 0) {
            char buf[256];
            switch (self->m_Engine.State()) {
            case RecorderState::Playing:
                std::snprintf(buf, sizeof(buf), "Playing: frame %u", self->m_Engine.FrameCount());
                break;
            case RecorderState::Paused:
                std::snprintf(buf, sizeof(buf), "Paused: frame %u", self->m_Engine.FrameCount());
                break;
            default:
                std::snprintf(buf, sizeof(buf), "Idle.");
                break;
            }
            self->ConsolePrint(buf);
        } else {
            self->ConsolePrint("Usage: playback <start <file>|stop|pause|resume|status>",
                               BML_CONSOLE_OUTPUT_FLAG_ERROR);
        }

        return BML_RESULT_OK;
    }

    static void CompletePlayback(const BML_ConsoleCompletionContext *ctx,
                                 PFN_BML_ConsoleCompletionSink sink,
                                 void *sinkUd, void *cmdUd) {
        if (!ctx || !sink) return;
        if (ctx->token_index == 0) {
            sink("start", sinkUd);
            sink("stop", sinkUd);
            sink("pause", sinkUd);
            sink("resume", sinkUd);
            sink("status", sinkUd);
        } else if (ctx->token_index == 1 && ctx->argc >= 1 &&
                   std::strcmp(ctx->argv_utf8[0], "start") == 0) {
            // For "playback start <file>", list .bmlr files
            auto *self = static_cast<InputRecorderMod *>(cmdUd);
            if (!self) return;
            std::error_code ec;
            for (const auto &entry : fs::directory_iterator(self->m_RecordingsDir, ec)) {
                if (entry.is_regular_file() && entry.path().extension() == ".bmlr") {
                    sink(entry.path().filename().string().c_str(), sinkUd);
                }
            }
        }
    }

public:
    BML_Result OnAttach() override {
        m_Subs = Services().CreateSubscriptions();
        m_RecordingsDir = GetRecordingsDir();

        // Acquire optional interfaces
        m_InputCaptureService = Services().Acquire<BML_InputCaptureInterface>();
        if (m_InputCaptureService) {
            m_InputCapture.SetService(m_InputCaptureService.Get());
        }

        m_ConsoleRegistry = Services().Acquire<BML_ConsoleCommandRegistry>();
        if (m_ConsoleRegistry) {
            BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
            desc.name_utf8 = "record";
            desc.description_utf8 = "Record input events (record <start [file]|stop|status>)";
            desc.execute = ExecuteRecord;
            desc.complete = CompleteRecord;
            desc.user_data = this;
            m_ConsoleRegistry->RegisterCommand(&desc);

            desc = BML_CONSOLE_COMMAND_DESC_INIT;
            desc.name_utf8 = "playback";
            desc.description_utf8 = "Play back recorded input (playback <start <file>|stop|pause|resume|status>)";
            desc.execute = ExecutePlayback;
            desc.complete = CompletePlayback;
            desc.user_data = this;
            m_ConsoleRegistry->RegisterCommand(&desc);
        }

        // Subscribe to input events for recording
        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            const auto *e = msg.As<BML_KeyDownEvent>();
            if (e) m_Engine.AccumulateEvent(MakeKeyDownEvent(*e));
        });
        m_Subs.Add(BML_TOPIC_INPUT_KEY_UP, [this](const bml::imc::Message &msg) {
            const auto *e = msg.As<BML_KeyUpEvent>();
            if (e) m_Engine.AccumulateEvent(MakeKeyUpEvent(*e));
        });
        m_Subs.Add(BML_TOPIC_INPUT_MOUSE_BUTTON, [this](const bml::imc::Message &msg) {
            const auto *e = msg.As<BML_MouseButtonEvent>();
            if (e) m_Engine.AccumulateEvent(MakeMouseBtnEvent(*e));
        });
        m_Subs.Add(BML_TOPIC_INPUT_MOUSE_MOVE, [this](const bml::imc::Message &msg) {
            const auto *e = msg.As<BML_MouseMoveEvent>();
            if (e) m_Engine.AccumulateEvent(MakeMouseMoveEvent(*e));
        });

        // Frame tick for recording flush and playback injection
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &msg) {
            const float *dt = msg.As<float>();
            float deltaTime = dt ? *dt : (1.0f / 60.0f);

            std::vector<EventRecord> playbackEvents;
            bool continuing = m_Engine.OnPostProcess(deltaTime, &playbackEvents);

            if (!playbackEvents.empty()) {
                InjectEvents(playbackEvents);
            }

            if (!continuing && m_Engine.State() == RecorderState::Idle) {
                ReleaseInputCapture();
                ConsolePrint("Playback finished.");
            }
        });

        // Auto-stop on level exit
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_PRE_EXIT, [this](const bml::imc::Message &) {
            if (m_Engine.State() != RecorderState::Idle) {
                m_Engine.OnLevelEnd();
                ReleaseInputCapture();
                ConsolePrint("Recording/playback auto-stopped (level exit).");
            }
        });

        constexpr size_t kExpectedSubscriptions = 6;  // 4 input + postprocess + level-exit
        if (m_Subs.Count() < kExpectedSubscriptions) {
            Services().Log().Error("Failed to subscribe to required topics");
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("Input Recorder module initialized");
        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        m_Engine.OnLevelEnd();
        ReleaseInputCapture();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        if (m_ConsoleRegistry) {
            m_ConsoleRegistry->UnregisterCommand("record");
            m_ConsoleRegistry->UnregisterCommand("playback");
            m_ConsoleRegistry.Reset();
        }

        m_InputCapture.Release();
        m_InputCaptureService.Reset();
    }
};

BML_DEFINE_MODULE(InputRecorderMod)
