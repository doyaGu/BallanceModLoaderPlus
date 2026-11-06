#include <BML/Mod.hpp>
#include <BML/Interface.hpp>
#include <stdexcept>

#include <BMLExample/ValueInterface.h>

namespace {

class ValueConsumer final : public BML::Mod::Instance {
  public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        const int openStatus = m_Value.Open();
        if (openStatus != BML_OK)
            throw std::runtime_error("Value interface lookup failed: " +
                                     std::to_string(openStatus));
        int value = 0;
        const int readStatus = m_Value->ReadValue(35, &value);
        (void)ctx.LogInfo("Value interface: open_status=" + std::to_string(openStatus) + " read_status=" +
                    std::to_string(readStatus) + " value=" + std::to_string(value));
        if (readStatus != BML_OK)
            throw std::runtime_error("Value interface read failed");
    }

    void OnUnload(const BML::Mod::Context &ctx) override { m_Value.Reset(); }

  private:
    BML::Interfaces::Reference<BML_ExampleValueTraits> m_Value;
};

} // namespace

BML_DEFINE_MOD(ValueConsumer, {
    .Id = "BMLExampleValueConsumer",
    .Name = "BML Example Value Consumer",
    .Version = "1.0.0",
    .Author = "BML",
    .Description = "Consumes the example.value native interface",
    .Dependencies = {{"BML", "", false}, {BML_EXAMPLE_VALUE_PROVIDER_ID, std::to_string(BML_EXAMPLE_VALUE_PROVIDER_VERSION_MAJOR) + "." + std::to_string(BML_EXAMPLE_VALUE_PROVIDER_VERSION_MINOR) + "." + std::to_string(BML_EXAMPLE_VALUE_PROVIDER_VERSION_PATCH), false}},
})
