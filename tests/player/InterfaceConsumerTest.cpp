#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ModInterface.hpp>

#include "InterfaceProviderTestApi.h"
#include "PlayerProbe.h"

#include <cstdint>

namespace {

const BML_PlayerProviderInterface kSpoofedInterface = {
    BML_IFACE_HEADER(BML_PlayerProviderInterface, "test.player.consumer.spoof", BML_PLAYER_PROVIDER_INTERFACE_MAJOR,
                     BML_PLAYER_PROVIDER_INTERFACE_MINOR),
    nullptr,
};

BML_DECLARE_INTERFACE_TRAITS(WrongMajorTraits, BML_PlayerProviderInterface, BML_PLAYER_PROVIDER_INTERFACE_ID,
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR + 1, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(MissingTraits, BML_PlayerProviderInterface, "test.player.provider.missing",
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);

class InterfaceConsumerTest final : public IMod {
public:
    explicit InterfaceConsumerTest(IBML *bml)
        : IMod(bml), m_Provider(*this, "InterfaceProviderTest", BMLVersion(1, 0, 0)) {
        AddDependency("BML");
    }

    const char *GetID() override { return "InterfaceConsumerTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Interface Consumer Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Consumes a native provider interface in Ballance Player"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        using BML::PlayerTest::ProbeReport;
        ProbeReport::Reset();

        const int findCode = m_Provider.Open();
        int value = 0;
        const bool callable = findCode == BML_OK && m_Provider && m_Provider->ReadValue(35, &value) == BML_OK &&
                              value == 42;

        BML::Interfaces::Reference<WrongMajorTraits> wrongMajor;
        const int wrongMajorCode = wrongMajor.Open();

        BML::Interfaces::Reference<MissingTraits> missing;
        const int missingCode = missing.Open();

        BML::Interfaces::Reference<BML_PlayerProviderExplicitTraits> explicitInterface;
        const int explicitCode = explicitInterface.Open();

        const int unregisterDenied =
            BML_UnregisterInterface(nullptr, BML_PLAYER_PROVIDER_INTERFACE_ID, BML_PLAYER_PROVIDER_INTERFACE_MAJOR);
        const int spoofedUnregisterDenied = BML_UnregisterInterface(
            "InterfaceProviderTest", BML_PLAYER_PROVIDER_INTERFACE_ID, BML_PLAYER_PROVIDER_INTERFACE_MAJOR);
        const int spoofedRegisterDenied = BML_RegisterInterface("InterfaceProviderTest", &kSpoofedInterface);

        const bool passed =
            m_Provider.DependencyStatus() == BML_OK && callable && wrongMajorCode == BML_ERROR_VERSION_MISMATCH &&
            !wrongMajor && missingCode == BML_ERROR_NOT_FOUND && !missing && explicitCode == BML_ERROR_NOT_FOUND &&
            !explicitInterface && unregisterDenied == BML_ERROR_ACCESS_DENIED &&
            spoofedUnregisterDenied == BML_ERROR_ACCESS_DENIED && spoofedRegisterDenied == BML_ERROR_ACCESS_DENIED;

        GetLogger()->Info("Interface consumer load: status=%s callable=%s value=%d "
                          "version_mismatch=%s missing=%s explicit_missing=%s "
                          "unregister_denied=%s spoof_denied=%s",
                          passed ? "pass" : "fail", callable ? "true" : "false", value,
                          wrongMajorCode == BML_ERROR_VERSION_MISMATCH && !wrongMajor ? "true" : "false",
                          missingCode == BML_ERROR_NOT_FOUND && !missing ? "true" : "false",
                          explicitCode == BML_ERROR_NOT_FOUND && !explicitInterface ? "true" : "false",
                          unregisterDenied == BML_ERROR_ACCESS_DENIED ? "true" : "false",
                          spoofedUnregisterDenied == BML_ERROR_ACCESS_DENIED &&
                                  spoofedRegisterDenied == BML_ERROR_ACCESS_DENIED
                              ? "true"
                              : "false");
        if (passed)
            ProbeReport::Pass("completed");
        else
            ProbeReport::Fail("interface-binding");
    }

    void OnUnload() override {
        BML::Interfaces::Reference<BML_PlayerProviderCleanupTraits> cleanup;
        const int cleanupCode = cleanup.Open();
        int value = 0;
        const bool callable = m_Provider && m_Provider->ReadValue(35, &value) == BML_OK && value == 42;
        const bool passed = callable && cleanupCode == BML_OK && cleanup;
        m_Provider.Reset();
        GetLogger()->Info("Interface consumer unload: status=%s provider_present=%s "
                          "cleanup_present=%s value=%d",
                          passed ? "pass" : "fail", callable ? "true" : "false",
                          cleanupCode == BML_OK && cleanup ? "true" : "false", value);
    }

private:
    BML::Interfaces::RequiredInterface<BML_PlayerProviderTraits> m_Provider;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new InterfaceConsumerTest(bml); }

BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
