#include <BML/Mod.hpp>
#include <BML/Interface.hpp>
#include <stdexcept>

#include <BMLExample/ValueInterface.h>

namespace {

int BML_CDECL ReadValue(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr auto kValueInterface = BML::Interfaces::MakeInterface<BML_ExampleValueTraits>(
    BML_EXAMPLE_VALUE_INTERFACE_MINOR, &ReadValue);

class ValueProvider final : public BML::Mod::Instance {
  public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        const int status = m_Value.Open(kValueInterface);
        if (status != BML_OK)
            throw std::runtime_error("Interface publication failed: " + std::to_string(status));
        (void)ctx.LogInfo(std::string("Value interface publication status: ") + std::to_string(status));
    }

    void OnUnload(const BML::Mod::Context &ctx) override {
        const int status = m_Value.Close();
        if (status != BML_OK && status != BML_ERROR_NOT_FOUND)
            throw std::runtime_error("Registration cleanup failed: " + std::to_string(status));
        (void)ctx.LogInfo(std::string("Value interface cleanup status: ") + std::to_string(status));
    }

  private:
    BML::Interfaces::Publication<BML_ExampleValueTraits> m_Value;
};

} // namespace

BML_DEFINE_MOD(ValueProvider, {
    .Id = BML_EXAMPLE_VALUE_PROVIDER_ID,
    .Name = "BML Example Value Provider",
    .Version = "1.0.0",
    .Author = "BML",
    .Description = "Publishes the example.value native interface",
    .Dependencies = {{"BML", "", false}},
})
