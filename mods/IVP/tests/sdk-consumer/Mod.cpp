#include <BML/IVP.h>
#include <BML/IMod.h>

#include <new>

class IvpSdkConsumer final : public IMod {
public:
    explicit IvpSdkConsumer(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }

    const char *GetID() override { return "IvpSdkConsumer"; }
    const char *GetVersion() override { return "0.1.0"; }
    const char *GetName() override { return "IVP SDK Consumer"; }
    const char *GetAuthor() override { return "BML+"; }
    const char *GetDescription() override {
        return "Checks that the installed IVP SDK resolves its API through BML";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::IVP::ApiInfo info{};
        (void) BML::IVP::ReadApiInfo(info);
    }
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new (std::nothrow) IvpSdkConsumer(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
