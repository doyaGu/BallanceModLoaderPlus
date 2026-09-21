#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/Interface.hpp>

#include <HelloMod/ValueInterface.h>

namespace {

int BML_CDECL ReadValue(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr auto kValueInterface =
    BML::Interfaces::MakeInterface<HelloModTraits>(
        BML_HELLOMOD_INTERFACE_MINOR, &ReadValue);

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
        const int status = m_Value.Open(kValueInterface);
        GetLogger()->Info("Native profile interface provider: publication_status=%d", status);
    }

    void OnUnload() override {
        const int status = m_Value.Close();
        GetLogger()->Info("Native profile interface provider: cleanup_status=%d", status);
    }

private:
    BML::Interfaces::Publication<HelloModTraits> m_Value;
};

} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new HelloMod(bml); }

BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
