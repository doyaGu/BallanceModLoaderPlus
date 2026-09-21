#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Interface.hpp>
#include <BML/Runtime.h>

#include "InterfaceProviderTestApi.h"

#include <new>
#include <thread>

namespace {

int BML_CDECL ReadValue(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr auto kPrimaryInterface = BML::Interfaces::MakeInterface<BML_PlayerProviderTraits>(
    BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);
constexpr auto kCleanupInterface = BML::Interfaces::MakeInterface<BML_PlayerProviderCleanupTraits>(
    BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);
constexpr auto kExplicitInterface = BML::Interfaces::MakeInterface<BML_PlayerProviderExplicitTraits>(
    BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);

BML_DECLARE_INTERFACE_TRAITS(WrongOwnerTraits, BML_PlayerProviderInterface, "test.player.provider.wrong-owner",
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(ThreadTraits, BML_PlayerProviderInterface, "test.player.provider.thread",
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(StackTraits, BML_PlayerProviderInterface, "test.player.provider.stack",
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(HeapTraits, BML_PlayerProviderInterface, "test.player.provider.heap",
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(BuiltinCollisionTraits, BML_PlayerProviderInterface, BML_RUNTIME_INTERFACE_ID,
                             BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);

constexpr auto kWrongOwnerInterface =
    BML::Interfaces::MakeInterface<WrongOwnerTraits>(BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);
constexpr auto kThreadInterface =
    BML::Interfaces::MakeInterface<ThreadTraits>(BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);
constexpr auto kBuiltinCollision =
    BML::Interfaces::MakeInterface<BuiltinCollisionTraits>(BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);

// Publishes two interfaces that deliberately take different teardown paths:
// OnUnload explicitly removes the primary one, while the loader must remove
// the cleanup interface before it calls BMLExit and releases this DLL.
class InterfaceProviderTest final : public IMod {
public:
    explicit InterfaceProviderTest(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    ~InterfaceProviderTest() override {
        BML::Interfaces::Reference<BML_PlayerProviderTraits> primary;
        BML::Interfaces::Reference<BML_PlayerProviderCleanupTraits> cleanup;
        const int primaryCode = primary.Open();
        const int cleanupCode = cleanup.Open();
        const bool passed = primaryCode == BML_ERROR_NOT_FOUND && cleanupCode == BML_ERROR_NOT_FOUND && !primary &&
                            !cleanup;
        GetLogger()->Info("Interface provider destroy: status=%s primary_removed=%s "
                          "cleanup_removed=%s primary_code=%d cleanup_code=%d",
                          passed ? "pass" : "fail", primaryCode == BML_ERROR_NOT_FOUND && !primary ? "true" : "false",
                          cleanupCode == BML_ERROR_NOT_FOUND && !cleanup ? "true" : "false", primaryCode, cleanupCode);
    }

    const char *GetID() override { return "InterfaceProviderTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Interface Provider Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Publishes a native provider interface for Player acceptance"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        // Construct the logger while the Mod is fully registered; the derived
        // destructor uses the same instance to record the post-cleanup result.
        ILogger *logger = GetLogger();

        const int primaryRegister = m_Primary.Open(kPrimaryInterface);
        const int cleanupRegister = m_Cleanup.Open(kCleanupInterface);

        BML::Interfaces::Reference<BML_PlayerProviderTraits> found;
        const int findCode = found.Open();
        int value = 0;
        const bool exactPointer = found.Get() == &kPrimaryInterface;
        const bool callable = exactPointer && found->ReadValue(35, &value) == BML_OK && value == 42;

        BML::Interfaces::Publication<BML_PlayerProviderTraits> duplicatePublication;
        const int duplicate = duplicatePublication.Open(kPrimaryInterface);
        const int wrongOwner = BML_RegisterInterface("InterfaceConsumerTest", &kWrongOwnerInterface);
        BML::Interfaces::Publication<BuiltinCollisionTraits> builtinPublication;
        const int builtinCollision = builtinPublication.Open(kBuiltinCollision);

        const auto stackInterface =
            BML::Interfaces::MakeInterface<StackTraits>(BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue);
        BML::Interfaces::Publication<StackTraits> stackPublication;
        const int stackRegistration = stackPublication.Open(stackInterface);

        int heapRegistration = BML_ERROR_OUT_OF_MEMORY;
        auto *heapInterface = new (std::nothrow) BML_PlayerProviderInterface(
            BML::Interfaces::MakeInterface<HeapTraits>(BML_PLAYER_PROVIDER_INTERFACE_MINOR, &ReadValue));
        if (heapInterface) {
            BML::Interfaces::Publication<HeapTraits> heapPublication;
            heapRegistration = heapPublication.Open(*heapInterface);
            delete heapInterface;
        }

        int threadRegistration = BML_ERROR_FAIL;
        std::thread worker([&threadRegistration]() {
            BML::Interfaces::Publication<ThreadTraits> threadPublication;
            threadRegistration = threadPublication.Open(kThreadInterface);
        });
        worker.join();

        BML::Interfaces::Publication<BML_PlayerProviderExplicitTraits> explicitPublication;
        BML::Interfaces::Reference<BML_PlayerProviderExplicitTraits> explicitFound;
        const int explicitRegister = explicitPublication.Open(kExplicitInterface);
        const int explicitFind = explicitFound.Open();
        const bool explicitPointer = explicitFound.Get() == &kExplicitInterface;
        const int explicitUnregister = explicitPublication.Close();
        const int explicitMissing = explicitFound.Open();

        const bool passed =
            primaryRegister == BML_OK && cleanupRegister == BML_OK && findCode == BML_OK && exactPointer && callable &&
            duplicate == BML_ERROR_ALREADY_EXISTS && wrongOwner == BML_ERROR_ACCESS_DENIED &&
            builtinCollision == BML_ERROR_ALREADY_EXISTS && stackRegistration == BML_ERROR_INVALID_PARAMETER &&
            heapRegistration == BML_ERROR_INVALID_PARAMETER && threadRegistration == BML_ERROR_WRONG_THREAD &&
            explicitRegister == BML_OK && explicitFind == BML_OK && explicitPointer && !explicitFound &&
            explicitUnregister == BML_OK && explicitMissing == BML_ERROR_NOT_FOUND;

        logger->Info("Interface provider load: status=%s registered=%s callable=%s "
                     "duplicate_rejected=%s owner_rejected=%s builtin_rejected=%s "
                     "stack_rejected=%s heap_rejected=%s thread_rejected=%s "
                     "explicit_cycle=%s",
                     passed ? "pass" : "fail",
                     primaryRegister == BML_OK && cleanupRegister == BML_OK ? "true" : "false",
                     callable ? "true" : "false", duplicate == BML_ERROR_ALREADY_EXISTS ? "true" : "false",
                     wrongOwner == BML_ERROR_ACCESS_DENIED ? "true" : "false",
                     builtinCollision == BML_ERROR_ALREADY_EXISTS ? "true" : "false",
                     stackRegistration == BML_ERROR_INVALID_PARAMETER ? "true" : "false",
                     heapRegistration == BML_ERROR_INVALID_PARAMETER ? "true" : "false",
                     threadRegistration == BML_ERROR_WRONG_THREAD ? "true" : "false",
                     explicitRegister == BML_OK && explicitFind == BML_OK && explicitUnregister == BML_OK &&
                             explicitPointer && explicitMissing == BML_ERROR_NOT_FOUND && !explicitFound
                         ? "true"
                         : "false");
    }

    void OnUnload() override {
        BML::Interfaces::Reference<BML_PlayerProviderCleanupTraits> cleanup;
        BML::Interfaces::Reference<BML_PlayerProviderTraits> primary;
        const int cleanupCode = cleanup.Open();
        const int explicitUnregister = m_Primary.Close();
        const int primaryCode = primary.Open();
        const bool passed = cleanupCode == BML_OK && cleanup.Get() == &kCleanupInterface &&
                            explicitUnregister == BML_OK && primaryCode == BML_ERROR_NOT_FOUND && !primary;
        GetLogger()->Info(
            "Interface provider unload: status=%s cleanup_present=%s "
            "explicit_removed=%s",
            passed ? "pass" : "fail",
            cleanupCode == BML_OK && cleanup.Get() == &kCleanupInterface ? "true" : "false",
            explicitUnregister == BML_OK && primaryCode == BML_ERROR_NOT_FOUND && !primary ? "true" : "false");
    }

private:
    BML::Interfaces::Publication<BML_PlayerProviderTraits> m_Primary;
    BML::Interfaces::Publication<BML_PlayerProviderCleanupTraits> m_Cleanup;
};

} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new InterfaceProviderTest(bml); }

BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
