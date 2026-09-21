#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ModInterface.hpp>

#include <__PROVIDER_CLASS__/ValueInterface.h>

namespace {

class HelloMod final : public IMod {
public:
    explicit HelloMod(IBML *bml)
        : IMod(bml),
          m_Value(*this, BML___PROVIDER_SYMBOL___PROVIDER_ID,
                  BMLVersion(BML___PROVIDER_SYMBOL___PROVIDER_VERSION_MAJOR,
                             BML___PROVIDER_SYMBOL___PROVIDER_VERSION_MINOR,
                             BML___PROVIDER_SYMBOL___PROVIDER_VERSION_PATCH)) {
        AddDependency("BML");
    }

    const char *GetID() override { return "__MOD_ID_CPP__"; }
    const char *GetVersion() override { return "__MOD_VERSION_CPP__"; }
    const char *GetName() override { return "__MOD_NAME_CPP__"; }
    const char *GetAuthor() override { return "__MOD_AUTHOR_CPP__"; }
    const char *GetDescription() override { return "__MOD_DESCRIPTION_CPP__"; }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        const int openStatus = m_Value.Open();
        int readStatus = BML_ERROR_INVALID_HANDLE;
        int value = 0;
        if (openStatus == BML_OK)
            readStatus = m_Value->ReadValue(35, &value);
        GetLogger()->Info(
            "Native profile interface consumer: open_status=%d read_status=%d value=%d",
            openStatus, readStatus, value);
    }

    void OnUnload() override {
        m_Value.Reset();
        GetLogger()->Info("Native profile interface consumer: reset=true");
    }

private:
    BML::Interfaces::RequiredInterface<__PROVIDER_CLASS__Traits> m_Value;
};

} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new HelloMod(bml); }

BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
