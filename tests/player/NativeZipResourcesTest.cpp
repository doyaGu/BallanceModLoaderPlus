#include <BML/BML.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerProbe.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class NativeZipResourcesTest final : public IMod {
public:
    explicit NativeZipResourcesTest(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "NativeZipResourcesTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Native ZIP Resources Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Reads a resource shipped beside this Mod"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        const bool readable = ReadResource();
        GetLogger()->Info("Native ZIP resource: phase=load readable=%s", readable ? "true" : "false");
        m_Reload = std::getenv("BML_TEST_NATIVE_ZIP_RELOAD") != nullptr;
        if (readable && !m_Reload)
            BML::PlayerTest::ProbeReport::Pass("resource-readable");
        else if (!readable)
            BML::PlayerTest::ProbeReport::Fail("resource-unreadable");
    }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        if (!m_Reload || !ProbeReport::Started() || ProbeReport::Reported())
            return;
        if (!m_Requested) {
            wchar_t *root = BML_GetModRootW("bml.zip.smoke");
            m_PreviousRoot = root ? root : L"";
            BML_FreeWString(root);
            m_Requested = true;
            m_RequestedAt = std::chrono::steady_clock::now();
            m_BML->ExecuteCommand("script reload bml.zip.smoke");
            return;
        }
        wchar_t *root = BML_GetModRootW("bml.zip.smoke");
        const bool changed = root && !m_PreviousRoot.empty() && root != m_PreviousRoot;
        BML_FreeWString(root);
        if (changed) {
            const bool readable = ReadResource();
            GetLogger()->Info("Native ZIP reload: round=%d readable=%s", ++m_Rounds, readable ? "true" : "false");
            if (!readable)
                ProbeReport::Fail("resource-lost-after-script-reload");
            else if (m_Rounds == 2)
                ProbeReport::Pass("resource-readable-after-two-reloads");
            else
                m_Requested = false;
        } else if (std::chrono::steady_clock::now() - m_RequestedAt > std::chrono::seconds(10)) {
            ProbeReport::Fail("script-reload-timeout");
        }
    }

    void OnUnload() override {
        GetLogger()->Info("Native ZIP resource: phase=unload readable=%s", ReadResource() ? "true" : "false");
    }

private:
    bool m_Reload = false;
    bool m_Requested = false;
    int m_Rounds = 0;
    std::wstring m_PreviousRoot;
    std::chrono::steady_clock::time_point m_RequestedAt;

    bool ReadResource() const {
        wchar_t *root = BML_GetModRootW("NativeZipResourcesTest");
        if (!root)
            return false;
        const auto directory = std::filesystem::path(root);
        BML_FreeWString(root);
        XString data = "zip-native-only.dat";
        if (m_BML->GetPathManager()->ResolveFileName(data, DATA_PATH_IDX) != CK_OK)
            return false;
        std::error_code error;
        if (!std::filesystem::equivalent(data.CStr(),
                directory / "3D Entities" / "PH" / "zip-native-only.dat", error))
            return false;
        std::ifstream resource(directory / "Resources" / "probe.txt");
        std::string value;
        return static_cast<bool>(resource >> value) && value == "zip-resource";
    }
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new NativeZipResourcesTest(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
