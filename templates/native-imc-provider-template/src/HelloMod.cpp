#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "__IMC_HEADER__"

namespace {

namespace Service = BML::Imc::Generated::__IMC_NAMESPACE__;

int Increment(const Service::IncrementRequestValue &request,
              Service::IncrementReplyValue &reply, void *) {
    reply.Value = request.Value + 1;
    return BML_OK;
}

class HelloMod final : public IMod {
public:
    explicit HelloMod(IBML *bml) : IMod(bml) { AddDependency("BML"); }

    const char *GetID() override { return "__MOD_ID_CPP__"; }
    const char *GetVersion() override { return "__MOD_VERSION_CPP__"; }
    const char *GetName() override { return "__MOD_NAME_CPP__"; }
    const char *GetAuthor() override { return "__MOD_AUTHOR_CPP__"; }
    const char *GetDescription() override { return "__MOD_DESCRIPTION_CPP__"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        Service::Provider::Handlers handlers{};
        handlers.Increment = &Increment;
        const int status = m_Provider.Start(handlers);
        GetLogger()->Info("Native profile IMC provider: start_status=%d", status);
    }

    void OnUnload() override {
        const int status = m_Provider.Close();
        GetLogger()->Info("Native profile IMC provider: cleanup_status=%d", status);
    }

private:
    Service::Provider m_Provider;
};

} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new HelloMod(bml); }

BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
