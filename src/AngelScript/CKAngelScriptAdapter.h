#ifndef BML_CKANGELSCRIPTADAPTER_H
#define BML_CKANGELSCRIPTADAPTER_H

#include <string>

#include "BML/Defines.h"
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

    typedef CKAngelScript *(BML_CDECL *CKGetAngelScriptFn)(CKContext *);
    typedef CKDWORD(BML_CDECL *GetApiVersionFn)();
    typedef CKBOOL(BML_CDECL *HasFeatureFn)(CKAS_FEATURE);
    typedef void(BML_CDECL *InitResultFn)(CKAngelScriptResult *);
    typedef void(BML_CDECL *InitLoadOptionsFn)(CKAngelScriptLoadOptions *);
    typedef void(BML_CDECL *InitImportBindOptionsFn)(CKAngelScriptImportBindOptions *);
    typedef void(BML_CDECL *InitBytecodeSaveOptionsFn)(CKAngelScriptBytecodeSaveOptions *);
    typedef void(BML_CDECL *InitBytecodeLoadOptionsFn)(CKAngelScriptBytecodeLoadOptions *);
    typedef void(BML_CDECL *InitModuleFingerprintFn)(CKAngelScriptModuleFingerprint *);
    typedef void(BML_CDECL *InitObjectOptionsFn)(CKAngelScriptObjectOptions *);
    typedef void(BML_CDECL *InitMethodOptionsFn)(CKAngelScriptMethodOptions *);
    typedef void(BML_CDECL *InitObjectMethodExecuteOptionsFn)(CKAngelScriptObjectMethodExecuteOptions *);
    typedef void(BML_CDECL *InitEngineExtensionFn)(CKAngelScriptEngineExtension *);
    typedef const char *(BML_CDECL *GetStatusNameFn)(CKAS_STATUS);
    typedef const char *(BML_CDECL *GetStatusDescriptionFn)(CKAS_STATUS);
    typedef CKAS_STATUS(BML_CDECL *LoadModuleFn)(CKAngelScript *,
                                               const CKAngelScriptLoadOptions *,
                                               CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *CompileModuleFn)(CKAngelScript *,
                                                  const char *,
                                                  const char *,
                                                  CKDWORD,
                                                  CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *UnloadModuleFn)(CKAngelScript *, const char *, CKAngelScriptResult *);
    typedef CKBOOL(BML_CDECL *HasModuleFn)(CKAngelScript *, const char *);
    typedef CKDWORD(BML_CDECL *GetModuleGenerationFn)(CKAngelScript *, const char *);
    typedef CKAS_STATUS(BML_CDECL *EnumerateMetadataFn)(CKAngelScript *,
                                                      const char *,
                                                      CKAngelScriptMetadataCallback,
                                                      void *,
                                                      CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *GetImportedFunctionCountFn)(CKAngelScript *,
                                                             const char *,
                                                             CKDWORD *,
                                                             CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *EnumerateImportedFunctionsFn)(CKAngelScript *,
                                                               const char *,
                                                               CKAngelScriptImportCallback,
                                                               void *,
                                                               CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *BindImportedFunctionFn)(CKAngelScript *,
                                                         const CKAngelScriptImportBindOptions *,
                                                         CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *BindAllImportedFunctionsFn)(CKAngelScript *,
                                                             const char *,
                                                             CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *UnbindImportedFunctionFn)(CKAngelScript *,
                                                           const char *,
                                                           CKDWORD,
                                                           CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *UnbindAllImportedFunctionsFn)(CKAngelScript *,
                                                               const char *,
                                                               CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *SaveModuleBytecodeFn)(CKAngelScript *,
                                                       const CKAngelScriptBytecodeSaveOptions *,
                                                       CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *LoadModuleBytecodeFn)(CKAngelScript *,
                                                       const CKAngelScriptBytecodeLoadOptions *,
                                                       CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *EnumerateBoundImportEdgesFn)(CKAngelScript *,
                                                              const char *,
                                                              CKAngelScriptBoundImportEdgeCallback,
                                                              void *,
                                                              CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *EnumerateModuleIncludeEdgesFn)(CKAngelScript *,
                                                                const char *,
                                                                CKAngelScriptIncludeEdgeCallback,
                                                                void *,
                                                                CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *GetModuleFingerprintFn)(CKAngelScript *,
                                                         const char *,
                                                         CKAngelScriptModuleFingerprint *,
                                                         CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *CreateObjectFn)(CKAngelScript *,
                                                 const CKAngelScriptObjectOptions *,
                                                 CKAngelScriptObject **,
                                                 CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *ReleaseObjectFn)(CKAngelScript *, CKAngelScriptObject *, CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *FindObjectMethodFn)(CKAngelScript *,
                                                     const CKAngelScriptMethodOptions *,
                                                     CKAngelScriptMethod **,
                                                     CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *ReleaseMethodFn)(CKAngelScript *, CKAngelScriptMethod *, CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *BorrowEngineFn)(CKAngelScript *, asIScriptEngine **, CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *BorrowActiveContextFn)(CKAngelScript *, asIScriptContext **, CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *SetActiveContextExceptionFn)(CKAngelScript *, const char *, CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *AssignObjectHandleFn)(void **, void *, asITypeInfo *);
    typedef CKAS_STATUS(BML_CDECL *ArgSetBoolFn)(CKAngelScriptArgWriter *, CKDWORD, CKBOOL);
    typedef CKAS_STATUS(BML_CDECL *ArgSetIntFn)(CKAngelScriptArgWriter *, CKDWORD, int);
    typedef CKAS_STATUS(BML_CDECL *ArgSetFloatFn)(CKAngelScriptArgWriter *, CKDWORD, float);
    typedef CKAS_STATUS(BML_CDECL *ArgSetStringFn)(CKAngelScriptArgWriter *, CKDWORD, const char *);
    typedef CKAS_STATUS(BML_CDECL *ArgSetBorrowedObjectFn)(CKAngelScriptArgWriter *, CKDWORD, void *);
    typedef CKAS_STATUS(BML_CDECL *ArgSetObjectHandleFn)(CKAngelScriptArgWriter *, CKDWORD, void *);
    typedef CKAS_STATUS(BML_CDECL *ResultGetBoolFn)(CKAngelScriptResultReader *, CKBOOL *);
    typedef CKAS_STATUS(BML_CDECL *ResultGetIntFn)(CKAngelScriptResultReader *, int *);
    typedef CKAS_STATUS(BML_CDECL *ResultGetFloatFn)(CKAngelScriptResultReader *, float *);
    typedef CKAS_STATUS(BML_CDECL *ResultGetStringFn)(CKAngelScriptResultReader *, char *, size_t, size_t *);
    typedef CKAS_STATUS(BML_CDECL *ResultGetStringViewFn)(CKAngelScriptResultReader *, const char **, size_t *);
    typedef CKAS_STATUS(BML_CDECL *CallObjectMethodFn)(CKAngelScript *,
                                                     const CKAngelScriptObjectMethodExecuteOptions *,
                                                     CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *RegisterEngineExtensionFn)(CKAngelScript *,
                                                            const CKAngelScriptEngineExtension *,
                                                            CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *UnregisterEngineExtensionFn)(CKAngelScript *,
                                                              const char *,
                                                              CKAngelScriptResult *);
    typedef CKAS_STATUS(BML_CDECL *CreateArrayFn)(CKAngelScript *, const char *, CKDWORD, void **);
    typedef CKAS_STATUS(BML_CDECL *CreateArrayByTypeFn)(asITypeInfo *, CKDWORD, void **);
    typedef CKAS_STATUS(BML_CDECL *ArrayAddRefFn)(void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayReleaseFn)(void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetRefCountFn)(void *, int *);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetArrayTypeFn)(void *, asITypeInfo **);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetArrayTypeIdFn)(void *, int *);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetSizeFn)(void *, CKDWORD *);
    typedef CKAS_STATUS(BML_CDECL *ArrayResizeFn)(void *, CKDWORD);
    typedef CKAS_STATUS(BML_CDECL *ArrayReserveFn)(void *, CKDWORD);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetElementTypeIdFn)(void *, int *);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetElementAddressFn)(void *, CKDWORD, void **);
    typedef CKAS_STATUS(BML_CDECL *ArrayGetConstElementAddressFn)(const void *, CKDWORD, const void **);
    typedef CKAS_STATUS(BML_CDECL *ArraySetElementValueFn)(void *, CKDWORD, const void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayInsertAtFn)(void *, CKDWORD, const void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayInsertLastFn)(void *, const void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayRemoveAtFn)(void *, CKDWORD);
    typedef CKAS_STATUS(BML_CDECL *ArrayRemoveLastFn)(void *);
    typedef CKAS_STATUS(BML_CDECL *ArrayClearFn)(void *);

    struct Api {
        CKGetAngelScriptFn GetAngelScript = nullptr;
        GetApiVersionFn GetApiVersion = nullptr;
        HasFeatureFn HasFeature = nullptr;
        InitResultFn InitResult = nullptr;
        InitLoadOptionsFn InitLoadOptions = nullptr;
        InitImportBindOptionsFn InitImportBindOptions = nullptr;
        InitBytecodeSaveOptionsFn InitBytecodeSaveOptions = nullptr;
        InitBytecodeLoadOptionsFn InitBytecodeLoadOptions = nullptr;
        InitModuleFingerprintFn InitModuleFingerprint = nullptr;
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
        GetImportedFunctionCountFn GetImportedFunctionCount = nullptr;
        EnumerateImportedFunctionsFn EnumerateImportedFunctions = nullptr;
        BindImportedFunctionFn BindImportedFunction = nullptr;
        BindAllImportedFunctionsFn BindAllImportedFunctions = nullptr;
        UnbindImportedFunctionFn UnbindImportedFunction = nullptr;
        UnbindAllImportedFunctionsFn UnbindAllImportedFunctions = nullptr;
        SaveModuleBytecodeFn SaveModuleBytecode = nullptr;
        LoadModuleBytecodeFn LoadModuleBytecode = nullptr;
        EnumerateBoundImportEdgesFn EnumerateBoundImportEdges = nullptr;
        EnumerateModuleIncludeEdgesFn EnumerateModuleIncludeEdges = nullptr;
        GetModuleFingerprintFn GetModuleFingerprint = nullptr;
        CreateObjectFn CreateObject = nullptr;
        ReleaseObjectFn ReleaseObject = nullptr;
        FindObjectMethodFn FindObjectMethod = nullptr;
        ReleaseMethodFn ReleaseMethod = nullptr;
        BorrowEngineFn BorrowEngine = nullptr;
        BorrowActiveContextFn BorrowActiveContext = nullptr;
        SetActiveContextExceptionFn SetActiveContextException = nullptr;
        AssignObjectHandleFn AssignObjectHandle = nullptr;
        ArgSetBoolFn ArgSetBool = nullptr;
        ArgSetIntFn ArgSetInt = nullptr;
        ArgSetFloatFn ArgSetFloat = nullptr;
        ArgSetStringFn ArgSetString = nullptr;
        ArgSetBorrowedObjectFn ArgSetBorrowedObject = nullptr;
        ArgSetObjectHandleFn ArgSetObjectHandle = nullptr;
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
    static bool IsArrayOf(const Api &api, const void *array, const char *elementName, const char *elementNamespace);

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
    static constexpr int kFeatureCount = static_cast<int>(CKAS_FEATURE_MODULE_FINGERPRINT) + 1;
    bool m_Features[kFeatureCount] = {};
    std::string m_Diagnostic;
};

#endif
