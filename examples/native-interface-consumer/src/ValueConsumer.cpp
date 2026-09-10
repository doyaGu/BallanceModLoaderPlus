#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ModInterface.hpp>

#include <BMLExample/ValueInterface.h>

namespace {

class ValueConsumer final : public IMod {
public:
    explicit ValueConsumer(IBML *bml)
        : IMod(bml),
          m_Value(*this, BML_EXAMPLE_VALUE_PROVIDER_ID,
                  BMLVersion(BML_EXAMPLE_VALUE_PROVIDER_VERSION_MAJOR,
                             BML_EXAMPLE_VALUE_PROVIDER_VERSION_MINOR,
                             BML_EXAMPLE_VALUE_PROVIDER_VERSION_PATCH)) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BMLExampleValueConsumer"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "BML Example Value Consumer"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override { return "Consumes the example.value native interface"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const int openStatus = m_Value.Open();
        if (openStatus != BML_OK) {
            GetLogger()->Error("Value interface lookup failed: %d", openStatus);
            return;
        }

        int value = 0;
        const int readStatus = m_Value->ReadValue(35, &value);
        GetLogger()->Info("Value interface read: status=%d value=%d", readStatus, value);
    }

    void OnUnload() override { m_Value.Reset(); }

private:
    BML::Interfaces::RequiredInterface<BML_ExampleValueTraits> m_Value;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new ValueConsumer(bml); }

MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
