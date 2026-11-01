#include <BML/BML.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerProbe.h"

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
        if (readable)
            BML::PlayerTest::ProbeReport::Pass("resource-readable");
        else
            BML::PlayerTest::ProbeReport::Fail("resource-unreadable");
    }

    void OnUnload() override {
        GetLogger()->Info("Native ZIP resource: phase=unload readable=%s", ReadResource() ? "true" : "false");
    }

private:
    bool ReadResource() const {
        wchar_t *root = BML_GetModRootW("NativeZipResourcesTest");
        if (!root)
            return false;
        const auto path = std::filesystem::path(root) / "Resources" / "probe.txt";
        BML_FreeWString(root);
        std::ifstream resource(path);
        std::string value;
        return static_cast<bool>(resource >> value) && value == "zip-resource";
    }
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new NativeZipResourcesTest(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
