#include <BML/BML.h>
#include <BML/Command.hpp>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include "PlayerProbe.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
constexpr std::array<int, 3> Categories{DATA_PATH_IDX, BITMAP_PATH_IDX, SOUND_PATH_IDX};

class ScriptZipSearchPathsTest final : public IMod {
public:
    explicit ScriptZipSearchPathsTest(IBML *bml) : IMod(bml) {}
    const char *GetID() override { return "ScriptZipSearchPathsTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Script ZIP Search Paths Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Loads packaged data, bitmap and sound resources across reload"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        GetLogger()->Info("Native ZIP resource: phase=load readable=%s", OwnResource() ? "true" : "false");
        m_Rollback = std::getenv("BML_TEST_ZIP_ROLLBACK") != nullptr;
        BML::Command::Definition command;
        command.Name = "zip_paths_phase";
        command.Hidden = true;
        const int status = m_Phase.Register(command, [this](const BML::Command::Invocation &call) {
            if (call.Argument(0) == "candidate") {
                m_CandidateReadable = ReadResources("zip-after.dat", "zip-after");
                GetLogger()->Info("ZIP search paths: candidate_readable=%s", m_CandidateReadable ? "true" : "false");
            } else if (call.Argument(0) == "rollback") {
                m_RolledBack = true;
            }
            return BML_OK;
        }, {}, GetID());
        if (status != BML_OK)
            BML::PlayerTest::ProbeReport::Fail("phase-command-unavailable");
    }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        if (!ProbeReport::Started() || ProbeReport::Reported())
            return;
        if (!m_Requested) {
            m_PreviousRoot = Root("bml.zip.paths");
            m_Counts = PathCounts();
            const wchar_t *replacement = _wgetenv(L"BML_TEST_ZIP_REPLACEMENT");
            const wchar_t *loader = BML_GetLoaderPathW(BML_DIR_LOADER);
            if (!replacement || !loader || !ReadResources("zip-before.dat", "zip-before")) {
                ProbeReport::Fail("initial-resources-unavailable");
                return;
            }
            std::error_code error;
            std::filesystem::copy_file(replacement,
                std::filesystem::path(loader) / "Mods" / "ScriptResourcesTest.zip",
                std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                ProbeReport::Fail("cannot-update-script-package");
                return;
            }
            m_Requested = true;
            m_RequestedAt = std::chrono::steady_clock::now();
            m_BML->ExecuteCommand("script reload bml.zip.paths");
            return;
        }
        const std::wstring root = Root("bml.zip.paths");
        if (m_Rollback ? m_RolledBack : root != m_PreviousRoot) {
            const bool resources = m_Rollback
                ? root == m_PreviousRoot && ReadResources("zip-before.dat", "zip-before") && !AnyResource("zip-after.dat")
                : ReadResources("zip-after.dat", "zip-after") && !AnyResource("zip-before.dat");
            const bool stable = PathCounts() == m_Counts;
            GetLogger()->Info("ZIP search paths: mode=%s resources=%s stable_count=%s",
                m_Rollback ? "rollback" : "reload", resources ? "true" : "false", stable ? "true" : "false");
            if (resources && stable && m_CandidateReadable)
                ProbeReport::Pass("search-paths-correct");
            else
                ProbeReport::Fail("search-paths-incorrect");
        } else if (std::chrono::steady_clock::now() - m_RequestedAt > std::chrono::seconds(10)) {
            ProbeReport::Fail("reload-timeout");
        }
    }

    void OnUnload() override {
        GetLogger()->Info("Native ZIP resource: phase=unload readable=%s", OwnResource() ? "true" : "false");
        if (m_Phase.Unregister() != BML_OK)
            GetLogger()->Error("Could not unregister the ZIP path test command");
    }

private:
    std::wstring Root(const char *id) const {
        wchar_t *root = BML_GetModRootW(id);
        const std::wstring value = root ? root : L"";
        BML_FreeWString(root);
        return value;
    }
    bool OwnResource() const {
        std::ifstream file(std::filesystem::path(Root("ScriptZipSearchPathsTest")) / "Resources" / "probe.txt");
        std::string value;
        return static_cast<bool>(file >> value) && value == "zip-resource";
    }
    bool ReadResources(const char *name, const char *expected) const {
        for (int category : Categories) {
            XString file = name;
            if (m_BML->GetPathManager()->ResolveFileName(file, category) != CK_OK)
                return false;
            std::ifstream resource(file.CStr());
            std::string value;
            if (!(resource >> value) || value != expected)
                return false;
        }
        return true;
    }
    bool AnyResource(const char *name) const {
        for (int category : Categories) {
            XString file = name;
            if (m_BML->GetPathManager()->ResolveFileName(file, category) == CK_OK)
                return true;
        }
        return false;
    }
    std::array<int, 3> PathCounts() const {
        return {m_BML->GetPathManager()->GetPathCount(DATA_PATH_IDX),
                m_BML->GetPathManager()->GetPathCount(BITMAP_PATH_IDX),
                m_BML->GetPathManager()->GetPathCount(SOUND_PATH_IDX)};
    }
    BML::Command::Registration m_Phase;
    bool m_Rollback = false, m_Requested = false, m_RolledBack = false, m_CandidateReadable = false;
    std::wstring m_PreviousRoot;
    std::array<int, 3> m_Counts{};
    std::chrono::steady_clock::time_point m_RequestedAt;
};
} // namespace
BML_PLAYER_PROBE_EXPORTS()
BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new ScriptZipSearchPathsTest(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
