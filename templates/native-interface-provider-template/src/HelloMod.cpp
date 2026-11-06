#include <BML/Mod.hpp>
#include <BML/Interface.hpp>
#include <stdexcept>

#include <HelloMod/ValueInterface.h>

namespace {

int BML_CDECL ReadValue(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr auto kValueInterface =
    BML::Interfaces::MakeInterface<HelloModTraits>(BML_HELLOMOD_INTERFACE_MINOR, &ReadValue);

class HelloMod final : public BML::Mod::Instance {
  public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        const int status = m_Value.Open(kValueInterface);
        if (status != BML_OK)
            throw std::runtime_error("Interface publication failed: " + std::to_string(status));
        ctx.LogInfo(std::string("Native profile interface provider: publication_status=") +
                    std::to_string(status));
    }

    void OnUnload(const BML::Mod::Context &ctx) override {
        const int status = m_Value.Close();
        if (status != BML_OK && status != BML_ERROR_NOT_FOUND)
            throw std::runtime_error("Registration cleanup failed: " + std::to_string(status));
        ctx.LogInfo(std::string("Native profile interface provider: cleanup_status=") +
                    std::to_string(status));
    }

  private:
    BML::Interfaces::Publication<HelloModTraits> m_Value;
};

} // namespace

BML_DEFINE_MOD(HelloMod, {
    .Id = "__MOD_ID_CPP__",
    .Name = "__MOD_NAME_CPP__",
    .Version = "__MOD_VERSION_CPP__",
    .Author = "__MOD_AUTHOR_CPP__",
    .Description = "__MOD_DESCRIPTION_CPP__",
    .Dependencies = {{"BML"}},
})
