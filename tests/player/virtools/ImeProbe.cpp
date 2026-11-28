#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commctrl.h>
#include <imm.h>
#include <msctf.h>
#include <wrl/client.h>

#include <BML/Engine.hpp>
#include <BML/Gameplay.h>
#include <BML/ImGui.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace {
using Clock = std::chrono::steady_clock;
using Microsoft::WRL::ComPtr;

class ImeProbe final : public BML::Mod::Instance {
public:
    void OnPostStartMenu(const BML::Mod::Context &) override {
        stage = 1;
        when = Clock::now();
    }

    void OnProcess(const BML::Mod::Context &ctx) override {
        if (stage == 0)
            return;
        ImGui::Begin("Virtools Unicode input acceptance");
        if (stage == 1)
            ImGui::SetKeyboardFocusHere();
        ImGui::InputText("Input", buffer, sizeof buffer);
        ImGui::End();
        auto now = Clock::now();
        if (now - when < std::chrono::milliseconds(350))
            return;
        when = now;
        if (stage == 1) {
            window = static_cast<HWND>(BML::Engine::BorrowContext(ctx).Take()->GetMainWindow());
            if (::GetForegroundWindow() != window || !(::GetFocus() == window || ::IsChild(window, ::GetFocus()))) {
                Finish(ctx, "Win32 Pinyin smoke could not acquire the Player foreground");
                return;
            }
            auto co = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            uninitCom = SUCCEEDED(co);
            const HRESULT manager = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
            if (FAILED(manager)) {
                Finish(ctx, "Win32 Pinyin smoke could not open TSF profiles");
                return;
            }
            profileBefore = profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &previous) == S_OK;
            constexpr GUID pinyinService = {0x81d4e9c9, 0x1d3b, 0x41bc, {0x9e,0x6c,0x4b,0x40,0xbf,0x79,0xe3,0x5e}};
            constexpr GUID pinyinProfile = {0xfa550b04, 0x5ad7, 0x411f, {0xa5,0xac,0xca,0x03,0x8e,0xc5,0x15,0xd7}};
            const HRESULT activated = profiles->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0804,
                pinyinService, pinyinProfile, nullptr, TF_IPPMF_FORPROCESS);
            (void)ctx.LogInfo("Win32 Pinyin activation HRESULT=" + std::to_string(static_cast<long>(activated)));
            if (activated != S_OK) {
                Finish(ctx, "Win32 Pinyin smoke could not activate the profile in this process");
                return;
            }
            profileChanged = true;
            if (!SetWindowSubclass(window, Observe, 1, reinterpret_cast<DWORD_PTR>(this))) {
                Finish(ctx, "Win32 Pinyin smoke could not observe the Player window");
                return;
            }
        } else if (stage == 2 || stage == 4) {
            if (stage == 4 && (commit != 0 || end != 1 || buffer[0] != '\0')) {
                Finish(ctx, "Win32 Pinyin cancellation changed the input");
                return;
            }
            for (WORD key : {0x4e, 0x49, 0x48, 0x41, 0x4f}) {
                if (!Press(key)) {
                    Finish(ctx, "Win32 Pinyin test lost Player foreground");
                    return;
                }
            }
        } else if (stage == 3) {
            (void)ctx.LogInfo("IME preedit messages=" + std::to_string(preedit) + " buffer=" + buffer);
            if (preedit == 0 || commit != 0 || buffer[0] != '\0' || !Press(VK_ESCAPE)) {
                Finish(ctx, "Win32 Pinyin preedit could not be cancelled");
                return;
            }
        } else if (stage == 5) {
            if (!Press(VK_SPACE)) {
                Finish(ctx, "Win32 Pinyin test lost Player foreground");
                return;
            }
        } else {
            (void)ctx.LogInfo("IME messages: preedit=" + std::to_string(preedit) + " commit=" +
                              std::to_string(commit) + " end=" + std::to_string(end) + " text=" + buffer);
            const std::string_view text(buffer);
            Finish(ctx, preedit >= 2 && end >= 2 && commit == 1 && text == "你好"
                              ? "Win32 Pinyin composition, cancellation and commit PASS"
                              : "Win32 Pinyin composition smoke did not complete");
            return;
        }
        ++stage;
    }

    void OnUnload(const BML::Mod::Context &) override {
        if (window)
            RemoveWindowSubclass(window, Observe, 1);
        if (profileBefore && profileChanged && profiles) {
            profiles->ActivateProfile(previous.dwProfileType, previous.langid,
                previous.clsid, previous.guidProfile,
                previous.dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT ? previous.hkl : nullptr,
                TF_IPPMF_FORPROCESS);
        }
        profiles.Reset();
        if (uninitCom)
            ::CoUninitialize();
    }

private:
    static LRESULT CALLBACK Observe(HWND hwnd, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
        auto *self = reinterpret_cast<ImeProbe *>(data);
        if (message == WM_IME_COMPOSITION) {
            if (l & GCS_COMPSTR) ++self->preedit;
            if (l & GCS_RESULTSTR) ++self->commit;
        } else if (message == WM_IME_ENDCOMPOSITION) {
            ++self->end;
        }
        return DefSubclassProc(hwnd, message, w, l);
    }

    bool Press(WORD key) const {
        if (::GetForegroundWindow() != window)
            return false;
        INPUT pair[2]{};
        pair[0].type = pair[1].type = INPUT_KEYBOARD;
        pair[0].ki.wVk = pair[1].ki.wVk = key;
        pair[1].ki.dwFlags = KEYEVENTF_KEYUP;
        return SendInput(2, pair, sizeof(INPUT)) == 2;
    }

    void Finish(const BML::Mod::Context &ctx, const char *result) {
        stage = 0;
        (void)ctx.LogInfo(result);
        (void)BML::Gameplay::ExitGame(ctx);
    }

    Clock::time_point when{};
    HWND window = nullptr;
    char buffer[128]{};
    unsigned stage = 0, preedit = 0, commit = 0, end = 0;
    ComPtr<ITfInputProcessorProfileMgr> profiles;
    TF_INPUTPROCESSORPROFILE previous{};
    bool profileChanged = false, profileBefore = false, uninitCom = false;
};
}

BML_DEFINE_MOD(ImeProbe, {
    .Id = "acceptance.virtools.ime",
    .Name = "Virtools IME acceptance",
    .Version = "1.0.0",
    .Author = "BML",
    .MinimumLoaderVersion = BML_VERSION,
})
