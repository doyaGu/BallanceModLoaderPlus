#include <BML/Mod.hpp>
#include <BML/Interface.hpp>
#include <stdexcept>

#include "__IMC_HEADER__"

namespace {

namespace Service = BML::Imc::Generated::__IMC_NAMESPACE__;

int Increment(const Service::IncrementRequestValue &request, Service::IncrementReplyValue &reply,
              void *) {
    reply.Value = request.Value + 1;
    return BML_OK;
}

class HelloMod final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        Service::Provider::Handlers handlers{};
        handlers.Increment = &Increment;
        const int status = m_Provider.Start(handlers);
        if (status != BML_OK)
            throw std::runtime_error("IMC registration failed: " + std::to_string(status));
        (void)ctx.LogInfo(std::string("Native profile IMC provider: start_status=") +
                    std::to_string(status));
    }

    void OnUnload(const BML::Mod::Context &ctx) override {
        const int status = m_Provider.Close();
        if (status != BML_OK && status != BML_ERROR_NOT_FOUND)
            throw std::runtime_error("Registration cleanup failed: " + std::to_string(status));
        (void)ctx.LogInfo(std::string("Native profile IMC provider: cleanup_status=") +
                    std::to_string(status));
    }

private:
    Service::Provider m_Provider;
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
