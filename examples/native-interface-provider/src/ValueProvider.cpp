#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Interface.hpp>

#include <BMLExample/ValueInterface.h>

namespace {

int BML_CDECL ReadValue(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr auto kValueInterface =
    BML::Interfaces::MakeInterface<BML_ExampleValueTraits>(BML_EXAMPLE_VALUE_INTERFACE_MINOR, &ReadValue);

class ValueProvider final : public IMod {
public:
    explicit ValueProvider(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    const char *GetID() override { return BML_EXAMPLE_VALUE_PROVIDER_ID; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "BML Example Value Provider"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Publishes the example.value native interface"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const int status = m_Value.Open(kValueInterface);
        GetLogger()->Info("Value interface publication status: %d", status);
    }

    void OnUnload() override {
        const int status = m_Value.Close();
        GetLogger()->Info("Value interface cleanup status: %d", status);
    }

private:
    BML::Interfaces::Publication<BML_ExampleValueTraits> m_Value;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new ValueProvider(bml); }

MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
