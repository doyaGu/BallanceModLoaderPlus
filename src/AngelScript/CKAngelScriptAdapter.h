#ifndef BML_CKANGELSCRIPTADAPTER_H
#define BML_CKANGELSCRIPTADAPTER_H

#include <string>

#include "CKAngelScript.h"

class CKAngelScriptAdapter {
public:
    enum class State {
        Unchecked,
        MissingModule,
        MissingExport,
        MissingRuntime,
        VersionTooOld,
        MissingFeature,
        Available
    };

    typedef CKAngelScript *(__cdecl *CKGetAngelScriptFn)(CKContext *);
    typedef CKDWORD(__cdecl *GetApiVersionFn)();
    typedef CKBOOL(__cdecl *HasFeatureFn)(CKAS_FEATURE);
    typedef void(__cdecl *InitResultFn)(CKAngelScriptResult *);
    typedef void(__cdecl *InitLoadOptionsFn)(CKAngelScriptLoadOptions *);
    typedef void(__cdecl *InitObjectOptionsFn)(CKAngelScriptObjectOptions *);
    typedef void(__cdecl *InitMethodOptionsFn)(CKAngelScriptMethodOptions *);
    typedef void(__cdecl *InitObjectMethodExecuteOptionsFn)(CKAngelScriptObjectMethodExecuteOptions *);
    typedef void(__cdecl *InitEngineExtensionFn)(CKAngelScriptEngineExtension *);
    typedef const char *(__cdecl *GetStatusNameFn)(CKAS_STATUS);
    typedef const char *(__cdecl *GetStatusDescriptionFn)(CKAS_STATUS);
    typedef CKAS_STATUS(__cdecl *LoadModuleFn)(CKAngelScript *,
                                               const CKAngelScriptLoadOptions *,
                                               CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *CompileModuleFn)(CKAngelScript *,
                                                  const char *,
                                                  const char *,
                                                  CKDWORD,
                                                  CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *UnloadModuleFn)(CKAngelScript *, const char *, CKAngelScriptResult *);
    typedef CKBOOL(__cdecl *HasModuleFn)(CKAngelScript *, const char *);
    typedef CKDWORD(__cdecl *GetModuleGenerationFn)(CKAngelScript *, const char *);
    typedef CKAS_STATUS(__cdecl *EnumerateMetadataFn)(CKAngelScript *,
                                                      const char *,
                                                      CKAngelScriptMetadataCallback,
                                                      void *,
                                                      CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *CreateObjectFn)(CKAngelScript *,
                                                 const CKAngelScriptObjectOptions *,
                                                 CKAngelScriptObject **,
                                                 CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *ReleaseObjectFn)(CKAngelScript *, CKAngelScriptObject *, CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *FindObjectMethodFn)(CKAngelScript *,
                                                     const CKAngelScriptMethodOptions *,
                                                     CKAngelScriptMethod **,
                                                     CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *ReleaseMethodFn)(CKAngelScript *, CKAngelScriptMethod *, CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *BorrowActiveContextFn)(CKAngelScript *, asIScriptContext **, CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *AssignObjectHandleFn)(void **, void *, asITypeInfo *);
    typedef CKAS_STATUS(__cdecl *ArgSetBoolFn)(CKAngelScriptArgWriter *, CKDWORD, CKBOOL);
    typedef CKAS_STATUS(__cdecl *ArgSetIntFn)(CKAngelScriptArgWriter *, CKDWORD, int);
    typedef CKAS_STATUS(__cdecl *ArgSetFloatFn)(CKAngelScriptArgWriter *, CKDWORD, float);
    typedef CKAS_STATUS(__cdecl *ArgSetStringFn)(CKAngelScriptArgWriter *, CKDWORD, const char *);
    typedef CKAS_STATUS(__cdecl *ArgSetBorrowedObjectFn)(CKAngelScriptArgWriter *, CKDWORD, void *);
    typedef CKAS_STATUS(__cdecl *ResultGetBoolFn)(CKAngelScriptResultReader *, CKBOOL *);
    typedef CKAS_STATUS(__cdecl *ResultGetIntFn)(CKAngelScriptResultReader *, int *);
    typedef CKAS_STATUS(__cdecl *ResultGetFloatFn)(CKAngelScriptResultReader *, float *);
    typedef CKAS_STATUS(__cdecl *ResultGetStringFn)(CKAngelScriptResultReader *, char *, size_t, size_t *);
    typedef CKAS_STATUS(__cdecl *ResultGetStringViewFn)(CKAngelScriptResultReader *, const char **, size_t *);
    typedef CKAS_STATUS(__cdecl *CallObjectMethodFn)(CKAngelScript *,
                                                     const CKAngelScriptObjectMethodExecuteOptions *,
                                                     CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *RegisterEngineExtensionFn)(CKAngelScript *,
                                                            const CKAngelScriptEngineExtension *,
                                                            CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *UnregisterEngineExtensionFn)(CKAngelScript *,
                                                              const char *,
                                                              CKAngelScriptResult *);
    typedef CKAS_STATUS(__cdecl *CreateArrayFn)(CKAngelScript *, const char *, CKDWORD, void **);
    typedef CKAS_STATUS(__cdecl *CreateArrayByTypeFn)(asITypeInfo *, CKDWORD, void **);
    typedef CKAS_STATUS(__cdecl *ArrayAddRefFn)(void *);
    typedef CKAS_STATUS(__cdecl *ArrayReleaseFn)(void *);
    typedef CKAS_STATUS(__cdecl *ArrayGetRefCountFn)(void *, int *);
    typedef CKAS_STATUS(__cdecl *ArrayGetArrayTypeFn)(void *, asITypeInfo **);
    typedef CKAS_STATUS(__cdecl *ArrayGetArrayTypeIdFn)(void *, int *);
    typedef CKAS_STATUS(__cdecl *ArrayGetSizeFn)(void *, CKDWORD *);
    typedef CKAS_STATUS(__cdecl *ArrayResizeFn)(void *, CKDWORD);
    typedef CKAS_STATUS(__cdecl *ArrayReserveFn)(void *, CKDWORD);
    typedef CKAS_STATUS(__cdecl *ArrayGetElementTypeIdFn)(void *, int *);
    typedef CKAS_STATUS(__cdecl *ArrayGetElementAddressFn)(void *, CKDWORD, void **);
    typedef CKAS_STATUS(__cdecl *ArrayGetConstElementAddressFn)(const void *, CKDWORD, const void **);
    typedef CKAS_STATUS(__cdecl *ArraySetElementValueFn)(void *, CKDWORD, const void *);
    typedef CKAS_STATUS(__cdecl *ArrayInsertAtFn)(void *, CKDWORD, const void *);
    typedef CKAS_STATUS(__cdecl *ArrayInsertLastFn)(void *, const void *);
    typedef CKAS_STATUS(__cdecl *ArrayRemoveAtFn)(void *, CKDWORD);
    typedef CKAS_STATUS(__cdecl *ArrayRemoveLastFn)(void *);
    typedef CKAS_STATUS(__cdecl *ArrayClearFn)(void *);

    struct Api {
        CKGetAngelScriptFn GetAngelScript = nullptr;
        GetApiVersionFn GetApiVersion = nullptr;
        HasFeatureFn HasFeature = nullptr;
        InitResultFn InitResult = nullptr;
        InitLoadOptionsFn InitLoadOptions = nullptr;
        InitObjectOptionsFn InitObjectOptions = nullptr;
        InitMethodOptionsFn InitMethodOptions = nullptr;
        InitObjectMethodExecuteOptionsFn InitObjectMethodExecuteOptions = nullptr;
        InitEngineExtensionFn InitEngineExtension = nullptr;
        GetStatusNameFn GetStatusName = nullptr;
        GetStatusDescriptionFn GetStatusDescription = nullptr;
        LoadModuleFn LoadModule = nullptr;
        CompileModuleFn CompileModule = nullptr;
        UnloadModuleFn UnloadModule = nullptr;
        HasModuleFn HasModule = nullptr;
        GetModuleGenerationFn GetModuleGeneration = nullptr;
        EnumerateMetadataFn EnumerateMetadata = nullptr;
        CreateObjectFn CreateObject = nullptr;
        ReleaseObjectFn ReleaseObject = nullptr;
        FindObjectMethodFn FindObjectMethod = nullptr;
        ReleaseMethodFn ReleaseMethod = nullptr;
        BorrowActiveContextFn BorrowActiveContext = nullptr;
        AssignObjectHandleFn AssignObjectHandle = nullptr;
        ArgSetBoolFn ArgSetBool = nullptr;
        ArgSetIntFn ArgSetInt = nullptr;
        ArgSetFloatFn ArgSetFloat = nullptr;
        ArgSetStringFn ArgSetString = nullptr;
        ArgSetBorrowedObjectFn ArgSetBorrowedObject = nullptr;
        ResultGetBoolFn ResultGetBool = nullptr;
        ResultGetIntFn ResultGetInt = nullptr;
        ResultGetFloatFn ResultGetFloat = nullptr;
        ResultGetStringFn ResultGetString = nullptr;
        ResultGetStringViewFn ResultGetStringView = nullptr;
        CallObjectMethodFn CallObjectMethod = nullptr;
        RegisterEngineExtensionFn RegisterEngineExtension = nullptr;
        UnregisterEngineExtensionFn UnregisterEngineExtension = nullptr;
        CreateArrayFn CreateArray = nullptr;
        CreateArrayByTypeFn CreateArrayByType = nullptr;
        ArrayAddRefFn ArrayAddRef = nullptr;
        ArrayReleaseFn ArrayRelease = nullptr;
        ArrayGetRefCountFn ArrayGetRefCount = nullptr;
        ArrayGetArrayTypeFn ArrayGetArrayType = nullptr;
        ArrayGetArrayTypeIdFn ArrayGetArrayTypeId = nullptr;
        ArrayGetSizeFn ArrayGetSize = nullptr;
        ArrayResizeFn ArrayResize = nullptr;
        ArrayReserveFn ArrayReserve = nullptr;
        ArrayGetElementTypeIdFn ArrayGetElementTypeId = nullptr;
        ArrayGetElementAddressFn ArrayGetElementAddress = nullptr;
        ArrayGetConstElementAddressFn ArrayGetConstElementAddress = nullptr;
        ArraySetElementValueFn ArraySetElementValue = nullptr;
        ArrayInsertAtFn ArrayInsertAt = nullptr;
        ArrayInsertLastFn ArrayInsertLast = nullptr;
        ArrayRemoveAtFn ArrayRemoveAt = nullptr;
        ArrayRemoveLastFn ArrayRemoveLast = nullptr;
        ArrayClearFn ArrayClear = nullptr;
    };

    bool Refresh(CKContext *context);

    bool IsAvailable() const;
    bool HasFeature(CKAS_FEATURE feature) const;
    State GetState() const;
    CKDWORD GetApiVersion() const;
    CKAngelScript *GetAngelScript() const;
    const Api &GetApi() const;
    const std::string &GetDiagnostic() const;

    static const char *StatusName(CKAS_STATUS status);
    static const char *FeatureName(CKAS_FEATURE feature);
    static std::string FormatResult(CKAS_STATUS status, const CKAngelScriptResult &result);

private:
    bool ResolveRequiredExports(void *module);
    bool ValidateFeatures();
    void SetUnavailable(State state, const std::string &diagnostic);

    void *m_Module = nullptr;
    CKContext *m_Context = nullptr;
    CKAngelScript *m_AngelScript = nullptr;
    Api m_Api;
    State m_State = State::Unchecked;
    CKDWORD m_ApiVersion = 0;
    bool m_Features[CKAS_FEATURE_SCRIPT_ARRAY_ACCESS + 1] = {};
    std::string m_Diagnostic;
};

#endif
