#include <BML/Mod.hpp>
#include <BML/Interface.hpp>
#include <stdexcept>

#include <__PROVIDER_CLASS__/ValueInterface.h>

namespace {

class HelloMod final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        const int openStatus = m_Value.Open();
        if (openStatus != BML_OK)
            throw std::runtime_error("Value interface lookup failed: " +
                                     std::to_string(openStatus));
        int value = 0;
        const int readStatus = m_Value->ReadValue(35, &value);
        (void)ctx.LogInfo("Native profile interface consumer: open_status=" + std::to_string(openStatus) +
                    " read_status=" + std::to_string(readStatus) +
                    " value=" + std::to_string(value));
        if (readStatus != BML_OK)
            throw std::runtime_error("Value interface read failed");
    }

    void OnUnload(const BML::Mod::Context &ctx) override {
        m_Value.Reset();
        (void)ctx.LogInfo("Native profile interface consumer: reset=true");
    }

private:
    BML::Interfaces::Reference<__PROVIDER_CLASS__Traits> m_Value;
};

} // namespace

BML_DEFINE_MOD(HelloMod, {
    .Id = "__MOD_ID_CPP__",
    .Name = "__MOD_NAME_CPP__",
    .Version = "__MOD_VERSION_CPP__",
    .Author = "__MOD_AUTHOR_CPP__",
    .Description = "__MOD_DESCRIPTION_CPP__",
    .Dependencies = {{"BML"}, {BML___PROVIDER_SYMBOL___PROVIDER_ID, std::to_string(BML___PROVIDER_SYMBOL___PROVIDER_VERSION_MAJOR) + "." + std::to_string(BML___PROVIDER_SYMBOL___PROVIDER_VERSION_MINOR) + "." + std::to_string(BML___PROVIDER_SYMBOL___PROVIDER_VERSION_PATCH), false}},
})
