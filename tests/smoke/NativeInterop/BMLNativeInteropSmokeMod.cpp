#include "BML/IMod.h"
#include "BML/Interop.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr const char *SMOKE_MOD_ID = "bml.native.interop.smoke";

static int NativeEchoExport(BML_CallFrame *frame, void *) {
    char input[256] = {};
    size_t required = 0;
    int status = BML_CallFrame_GetString(frame, 0, input, sizeof(input), &required);
    if (status != BML_OK)
        return status;

    std::string output = "native:";
    output += input;
    return BML_CallFrame_SetResultString(frame, output.c_str());
}

static int NativeNotExport(BML_CallFrame *frame, void *) {
    int value = 0;
    const int status = BML_CallFrame_GetBool(frame, 0, &value);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultBool(frame, value == 0);
}

static int NativeAddOneExport(BML_CallFrame *frame, void *) {
    int value = 0;
    const int status = BML_CallFrame_GetInt(frame, 0, &value);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultInt(frame, value + 1);
}

static int NativeSumExport(BML_CallFrame *frame, void *) {
    int left = 0;
    int right = 0;
    int status = BML_CallFrame_GetInt(frame, 0, &left);
    if (status != BML_OK)
        return status;
    status = BML_CallFrame_GetInt(frame, 1, &right);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultInt(frame, left + right);
}

static int NativeScaleExport(BML_CallFrame *frame, void *) {
    float value = 0.0f;
    const int status = BML_CallFrame_GetFloat(frame, 0, &value);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultFloat(frame, value * 2.0f);
}

static int NativeFrameReportExport(BML_CallFrame *frame, void *) {
    const size_t argCount = BML_CallFrame_GetArgCount(frame);
    const int type0 = BML_CallFrame_GetArgType(frame, 0);
    const int type1 = BML_CallFrame_GetArgType(frame, 1);
    const int resultTypeBefore = BML_CallFrame_GetResultType(frame);

    int status = BML_CallFrame_ClearArg(frame, 1);
    if (status != BML_OK)
        return status;

    status = BML_CallFrame_SetResultInt(frame,
                                        static_cast<int>(argCount * 1000) +
                                        type0 * 100 +
                                        type1 * 10 +
                                        resultTypeBefore);
    if (status != BML_OK)
        return status;

    return BML_CallFrame_GetArgCount(frame) == 1 ? BML_OK : BML_ERROR_FAIL;
}

static int NativeAutoCleanupSmokeExport(BML_CallFrame *, void *) {
    return BML_OK;
}

static int NativeAmbiguousIntExport(BML_CallFrame *frame, void *) {
    int value = 0;
    const int status = BML_CallFrame_GetInt(frame, 0, &value);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultInt(frame, value);
}

static int NativeAmbiguousFloatExport(BML_CallFrame *frame, void *) {
    float value = 0.0f;
    const int status = BML_CallFrame_GetFloat(frame, 0, &value);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultFloat(frame, value);
}

static int NativeThrowExport(BML_CallFrame *, void *) {
    throw std::runtime_error("native smoke exception");
}

static int NativeFailWithResultExport(BML_CallFrame *frame, void *) {
    BML_CallFrame_SetResultInt(frame, 99);
    return BML_ERROR_FAIL;
}

static int NativeSumIntArrayExport(BML_CallFrame *frame, void *) {
    const void *data = nullptr;
    size_t required = 0;
    size_t elementSize = 0;
    const int status = BML_CallFrame_BorrowData(frame,
                                                0,
                                                BML_CALL_VALUE_INT_ARRAY,
                                                &data,
                                                &required,
                                                &elementSize);
    if (status != BML_OK)
        return status;
    if (elementSize != sizeof(int))
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    const int *values = static_cast<const int *>(data);
    int sum = 0;
    for (size_t i = 0; i < required; ++i)
        sum += values[i];
    return BML_CallFrame_SetResultInt(frame, sum);
}

static int NativeMirrorBufferExport(BML_CallFrame *frame, void *) {
    const void *borrowed = nullptr;
    size_t required = 0;
    size_t elementSize = 0;
    const int status = BML_CallFrame_BorrowData(frame,
                                                0,
                                                BML_CALL_VALUE_BUFFER,
                                                &borrowed,
                                                &required,
                                                &elementSize);
    if (status != BML_OK)
        return status;
    if (elementSize != sizeof(std::uint8_t))
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    const auto *bytesIn = static_cast<const std::uint8_t *>(borrowed);
    std::vector<std::uint8_t> bytes;
    if (bytesIn && required > 0)
        bytes.assign(bytesIn, bytesIn + required);
    for (std::uint8_t &byte : bytes)
        byte = static_cast<std::uint8_t>(byte ^ 0xffu);
    return BML_CallFrame_SetResultData(frame, BML_CALL_VALUE_BUFFER, bytes.data(), bytes.size());
}

static int NativeStringArrayCountExport(BML_CallFrame *frame, void *) {
    const size_t count = BML_CallFrame_GetStringArrayCount(frame, 0);
    if (count == 0 && BML_CallFrame_GetArgType(frame, 0) != BML_CALL_VALUE_STRING_ARRAY)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    const char *first = nullptr;
    size_t firstSize = 0;
    if (count > 0 && BML_CallFrame_BorrowStringArrayItem(frame, 0, 0, &first, &firstSize) != BML_OK)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;
    return BML_CallFrame_SetResultInt(frame, static_cast<int>(count));
}

static int NativeObjectIdentityExport(BML_CallFrame *frame, void *) {
    int objectId = 0;
    const int status = BML_CallFrame_GetObjectId(frame, 0, &objectId);
    if (status != BML_OK)
        return status;
    return BML_CallFrame_SetResultObjectId(frame, objectId);
}

static int NativeLifecycleSmokeExport(BML_CallFrame *frame, void *userdata) {
    auto *called = static_cast<bool *>(userdata);
    if (called)
        *called = true;
    return BML_CallFrame_SetResultInt(frame, 7);
}

static void ReleaseExport(BML_ModExport *handle) {
    if (handle)
        BML_ReleaseModExport(handle);
}

static void DestroyFrame(BML_CallFrame *frame) {
    if (frame)
        BML_DestroyCallFrame(frame);
}

class BMLNativeInteropSmokeMod final : public IMod {
public:
    explicit BMLNativeInteropSmokeMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return SMOKE_MOD_ID; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "BML Native Interop Smoke"; }
    const char *GetAuthor() override { return "BML+"; }
    const char *GetDescription() override {
        return "Validation-only native mod for BML script interop smoke tests.";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        RegisterExport("NativeEcho", "string NativeEcho(const string &in value)", NativeEchoExport);
        RegisterExport("NativeNot", "bool NativeNot(bool value)", NativeNotExport);
        RegisterExport("NativeAddOne", "int NativeAddOne(int value)", NativeAddOneExport);
        RegisterExport("NativeSum", "int NativeSum(int left, int right)", NativeSumExport);
        RegisterExport("NativeScale", "float NativeScale(float value)", NativeScaleExport);
        RegisterExport("NativeFrameReport", "int NativeFrameReport(int value, const string &in label)", NativeFrameReportExport);
        RegisterExport("NativeAutoCleanupSmoke", "void NativeAutoCleanupSmoke()", NativeAutoCleanupSmokeExport);
        RegisterExport("NativeAmbiguous", "int NativeAmbiguous(int value)", NativeAmbiguousIntExport);
        RegisterExport("NativeAmbiguous", "float NativeAmbiguous(float value)", NativeAmbiguousFloatExport);
        RegisterExport("NativeThrow", "void NativeThrow()", NativeThrowExport);
        RegisterExport("NativeFailWithResult", "int NativeFailWithResult()", NativeFailWithResultExport);
        RegisterExport("NativeSumIntArray",
                       "int NativeSumIntArray(const array<int> &in values)",
                       NativeSumIntArrayExport);
        RegisterExport("NativeMirrorBuffer",
                       "array<uint8>@ NativeMirrorBuffer(const array<uint8> &in bytes)",
                       NativeMirrorBufferExport);
        RegisterExport("NativeStringArrayCount",
                       "int NativeStringArrayCount(const array<string> &in values)",
                       NativeStringArrayCountExport);
        RegisterExport("NativeObjectIdentity",
                       "CKObject@ NativeObjectIdentity(CKObject@ object)",
                       NativeObjectIdentityExport);
        RunNativeExportLifecycleSmoke();
        RunNativeExportHardeningSmoke();
        RunNativeExtendedExportSmoke();
    }

    void OnUnload() override {
        BML_UnregisterNativeModExports(GetID());
    }

    void OnProcess() override {
        if (!m_InteropChecked) {
            m_InteropChecked = true;
            RunNativeToScriptExportSmoke();
            RunScriptCommandCompletionSmoke();
        }

        if (m_Checked)
            return;
        ++m_Frames;
        if (m_Frames < 180)
            return;
        m_Checked = true;

        GetLogger()->Info("BML native shutdown smoke requesting exit");
        if (m_BML)
            m_BML->ExecuteCommand("exit");
    }

private:
    void RegisterExport(const char *name, const char *signature, BML_NativeExportCallback callback) {
        const int status = BML_RegisterNativeModExport(GetID(), name, signature, callback, this);
        if (status != BML_OK) {
            GetLogger()->Warn("BML native interop smoke export register failed: %s %s status=%d",
                              name,
                              signature,
                              status);
        }
    }

    void RunNativeExportLifecycleSmoke() {
        bool callbackCalled = false;
        const char *name = "NativeLifecycleSmoke";
        const char *signature = "int NativeLifecycleSmoke()";

        const int firstRegister = BML_RegisterNativeModExport(GetID(),
                                                              name,
                                                              signature,
                                                              NativeLifecycleSmokeExport,
                                                              &callbackCalled);
        const int duplicateRegister = BML_RegisterNativeModExport(GetID(),
                                                                  name,
                                                                  signature,
                                                                  NativeLifecycleSmokeExport,
                                                                  &callbackCalled);

        BML_ModExport *handle = BML_FindModExport(GetID(), name, signature);
        BML_CallFrame *frame = BML_CreateCallFrame();
        const int validBeforeUnregister = handle ? BML_IsModExportValid(handle) : 0;
        const int unregisterStatus = BML_UnregisterNativeModExport(GetID(), name, signature);
        const int validAfterUnregister = handle ? BML_IsModExportValid(handle) : 0;
        const int staleCallStatus = handle && frame ? BML_CallModExport(handle, frame) : BML_ERROR_INVALID_PARAMETER;

        char signatureBuffer[128] = {};
        size_t signatureRequired = 0;
        const int signatureStatus = handle
                                        ? BML_GetModExportSignature(handle,
                                                                    signatureBuffer,
                                                                    sizeof(signatureBuffer),
                                                                    &signatureRequired)
                                        : BML_ERROR_INVALID_PARAMETER;

        GetLogger()->Info("BML native export lifecycle smoke first=%d duplicate=%d unregister=%d validBefore=%d validAfter=%d staleCall=%d callback=%s signatureStatus=%d signature=%s",
                          firstRegister,
                          duplicateRegister,
                          unregisterStatus,
                          validBeforeUnregister,
                          validAfterUnregister,
                          staleCallStatus,
                          callbackCalled ? "true" : "false",
                          signatureStatus,
                          signatureBuffer);

        DestroyFrame(frame);
        ReleaseExport(handle);
        if (firstRegister == BML_OK && unregisterStatus != BML_OK)
            BML_UnregisterNativeModExport(GetID(), name, signature);
    }

    void RunNativeExportHardeningSmoke() {
        BML_ModExport *handle = nullptr;
        int findExStatus = BML_FindModExportEx(GetID(), "NativeAddOne", nullptr, &handle);
        ReleaseExport(handle);
        handle = nullptr;

        BML_ModExport *ambiguousHandle = nullptr;
        const int ambiguousStatus = BML_FindModExportEx(GetID(), "NativeAmbiguous", nullptr, &ambiguousHandle);
        ReleaseExport(ambiguousHandle);

        BML_ModExport *explicitHandle = nullptr;
        const int explicitStatus = BML_FindModExportEx(GetID(),
                                                       "NativeAmbiguous",
                                                       "int NativeAmbiguous(int value)",
                                                       &explicitHandle);
        ReleaseExport(explicitHandle);

        BML_ModExport *mismatchHandle = nullptr;
        const int mismatchStatus = BML_FindModExportEx(GetID(),
                                                       "NativeAddOne",
                                                       "float NativeAddOne(float value)",
                                                       &mismatchHandle);
        ReleaseExport(mismatchHandle);

        BML_ModExport *badSignatureHandle = nullptr;
        const int badSignatureStatus = BML_FindModExportEx(GetID(),
                                                           "NativeAddOne",
                                                           "double NativeAddOne(double value)",
                                                           &badSignatureHandle);
        ReleaseExport(badSignatureHandle);

        BML_ModExport *missingTargetHandle = nullptr;
        const int missingTargetStatus = BML_FindModExportEx("bml.missing.interop.smoke",
                                                            "Missing",
                                                            "void Missing()",
                                                            &missingTargetHandle);
        ReleaseExport(missingTargetHandle);

        BML_ModExport *throwHandle = nullptr;
        int throwStatus = BML_FindModExportEx(GetID(), "NativeThrow", nullptr, &throwHandle);
        BML_CallFrame *throwFrame = BML_CreateCallFrame();
        if (throwStatus == BML_OK)
            throwStatus = BML_CallModExport(throwHandle, throwFrame);
        const int throwResultType = throwFrame ? BML_CallFrame_GetResultType(throwFrame) : BML_CALL_VALUE_EMPTY;
        DestroyFrame(throwFrame);
        ReleaseExport(throwHandle);

        BML_ModExport *failHandle = nullptr;
        int failStatus = BML_FindModExportEx(GetID(), "NativeFailWithResult", nullptr, &failHandle);
        BML_CallFrame *failFrame = BML_CreateCallFrame();
        if (failStatus == BML_OK)
            failStatus = BML_CallModExport(failHandle, failFrame);
        const int failResultType = failFrame ? BML_CallFrame_GetResultType(failFrame) : BML_CALL_VALUE_EMPTY;
        DestroyFrame(failFrame);
        ReleaseExport(failHandle);

        GetLogger()->Info("BML native export hardening smoke findEx=%d ambiguous=%d explicit=%d mismatch=%d badSig=%d missingTarget=%d exception=%d exceptionResult=%d fail=%d failResult=%d",
                          findExStatus,
                          ambiguousStatus,
                          explicitStatus,
                          mismatchStatus,
                          badSignatureStatus,
                          missingTargetStatus,
                          throwStatus,
                          throwResultType,
                          failStatus,
                          failResultType);
    }

    void RunNativeExtendedExportSmoke() {
        int sumStatus = BML_ERROR_FAIL;
        int sumResult = 0;
        {
            BML_ModExport *handle = nullptr;
            sumStatus = BML_FindModExportEx(GetID(),
                                            "NativeSumIntArray",
                                            "int NativeSumIntArray(const array<int> &in input)",
                                            &handle);
            BML_CallFrame *frame = BML_CreateCallFrame();
            const int values[] = {3, 5, 8};
            if (sumStatus == BML_OK && frame)
                sumStatus = BML_CallFrame_SetData(frame, 0, BML_CALL_VALUE_INT_ARRAY, values, 3);
            if (sumStatus == BML_OK)
                sumStatus = BML_CallModExport(handle, frame);
            if (sumStatus == BML_OK)
                sumStatus = BML_CallFrame_GetResultInt(frame, &sumResult);
            DestroyFrame(frame);
            ReleaseExport(handle);
        }

        int bufferStatus = BML_ERROR_FAIL;
        std::vector<std::uint8_t> bufferResult;
        {
            BML_ModExport *handle = nullptr;
            bufferStatus = BML_FindModExportEx(GetID(),
                                               "NativeMirrorBuffer",
                                               "array<uint8>@ NativeMirrorBuffer(const array<uint8> &in bytes)",
                                               &handle);
            BML_CallFrame *frame = BML_CreateCallFrame();
            const std::uint8_t bytes[] = {0x00u, 0x55u, 0xffu};
            if (bufferStatus == BML_OK && frame)
                bufferStatus = BML_CallFrame_SetData(frame, 0, BML_CALL_VALUE_BUFFER, bytes, 3);
            if (bufferStatus == BML_OK)
                bufferStatus = BML_CallModExport(handle, frame);
            if (bufferStatus == BML_OK) {
                size_t required = 0;
                size_t elementSize = 0;
                const void *borrowed = nullptr;
                bufferStatus = BML_CallFrame_BorrowResultData(frame,
                                                              BML_CALL_VALUE_BUFFER,
                                                              &borrowed,
                                                              &required,
                                                              &elementSize);
                const auto *bytesOut = static_cast<const std::uint8_t *>(borrowed);
                if (bufferStatus == BML_OK && elementSize == sizeof(std::uint8_t) && bytesOut)
                    bufferResult.assign(bytesOut, bytesOut + required);
            }
            DestroyFrame(frame);
            ReleaseExport(handle);
        }

        int stringArrayStatus = BML_ERROR_FAIL;
        int stringArrayCount = 0;
        {
            BML_ModExport *handle = nullptr;
            stringArrayStatus = BML_FindModExportEx(GetID(),
                                                    "NativeStringArrayCount",
                                                    "int NativeStringArrayCount(const array<string> &in values)",
                                                    &handle);
            BML_CallFrame *frame = BML_CreateCallFrame();
            const char *values[] = {"alpha", "beta"};
            if (stringArrayStatus == BML_OK && frame)
                stringArrayStatus = BML_CallFrame_SetStringArray(frame, 0, values, 2);
            if (stringArrayStatus == BML_OK)
                stringArrayStatus = BML_CallModExport(handle, frame);
            if (stringArrayStatus == BML_OK)
                stringArrayStatus = BML_CallFrame_GetResultInt(frame, &stringArrayCount);
            DestroyFrame(frame);
            ReleaseExport(handle);
        }

        int objectStatus = BML_ERROR_FAIL;
        int objectResult = -1;
        {
            BML_ModExport *handle = nullptr;
            objectStatus = BML_FindModExportEx(GetID(),
                                               "NativeObjectIdentity",
                                               "CKObject@ NativeObjectIdentity(CKObject@ object)",
                                               &handle);
            BML_CallFrame *frame = BML_CreateCallFrame();
            if (objectStatus == BML_OK && frame)
                objectStatus = BML_CallFrame_SetObjectId(frame, 0, 0);
            if (objectStatus == BML_OK)
                objectStatus = BML_CallModExport(handle, frame);
            if (objectStatus == BML_OK)
                objectStatus = BML_CallFrame_GetResultObjectId(frame, &objectResult);
            DestroyFrame(frame);
            ReleaseExport(handle);
        }

        const int bufferFirst = bufferResult.empty() ? -1 : static_cast<int>(bufferResult[0]);
        const int bufferLast = bufferResult.size() < 3 ? -1 : static_cast<int>(bufferResult[2]);
        GetLogger()->Info("BML native interop extended smoke sumStatus=%d sum=%d bufferStatus=%d bufferFirst=%d bufferLast=%d stringArrayStatus=%d stringCount=%d objectStatus=%d objectId=%d",
                          sumStatus,
                          sumResult,
                          bufferStatus,
                          bufferFirst,
                          bufferLast,
                          stringArrayStatus,
                          stringArrayCount,
                          objectStatus,
                          objectResult);
    }

    void RunNativeToScriptExportSmoke() {
        GetLogger()->Info("BML native interop smoke state self kind=%d state=%d script kind=%d state=%d",
                          BML_GetModKind(GetID()),
                          BML_GetModState(GetID()),
                          BML_GetModKind("bml.bindings.smoke"),
                          BML_GetModState("bml.bindings.smoke"));

        CallScriptEchoSmoke();
        CallScriptAddOneSmoke();
        CallScriptSumSmoke();
    }

    void RunScriptCommandCompletionSmoke() {
        ICommand *command = m_BML ? m_BML->FindCommand("assmoke") : nullptr;
        std::vector<std::string> args;
        args.emplace_back("assmoke");
        args.emplace_back("a");
        const std::vector<std::string> completions = command ? command->GetTabCompletion(m_BML, args) : std::vector<std::string>();
        bool hasAlpha = false;
        bool hasBeta = false;
        for (const std::string &completion : completions) {
            if (completion == "alpha")
                hasAlpha = true;
            if (completion == "beta")
                hasBeta = true;
        }
        GetLogger()->Info("BML native command completion smoke command=%s count=%d alpha=%s beta=%s",
                          command ? "true" : "false",
                          static_cast<int>(completions.size()),
                          hasAlpha ? "true" : "false",
                          hasBeta ? "true" : "false");
    }

    void CallScriptEchoSmoke() {
        BML_ModExport *exportHandle = BML_FindModExport("bml.bindings.smoke",
                                                        "Echo",
                                                        "string Echo(const string &in)");
        if (!exportHandle)
            return;

        BML_CallFrame *frame = BML_CreateCallFrame();
        if (!frame) {
            ReleaseExport(exportHandle);
            GetLogger()->Warn("BML native -> script export smoke failed: out of memory");
            return;
        }

        int status = BML_CallFrame_SetString(frame, 0, "native");
        if (status == BML_OK)
            status = BML_CallModExport(exportHandle, frame);

        std::string result;
        if (status == BML_OK) {
            size_t required = 0;
            BML_CallFrame_GetResultString(frame, nullptr, 0, &required);
            if (required > 0) {
                result.resize(required);
                if (BML_CallFrame_GetResultString(frame, &result[0], result.size(), &required) == BML_OK &&
                    !result.empty() && result.back() == '\0') {
                    result.pop_back();
                }
            }
        }

        GetLogger()->Info("BML native -> script export Echo status=%d result=%s", status, result.c_str());
        DestroyFrame(frame);
        ReleaseExport(exportHandle);
    }

    void CallScriptAddOneSmoke() {
        BML_ModExport *exportHandle = BML_FindModExport("bml.bindings.smoke", "AddOne", "int AddOne(int)");
        if (!exportHandle)
            return;

        BML_CallFrame *frame = BML_CreateCallFrame();
        if (!frame) {
            ReleaseExport(exportHandle);
            GetLogger()->Warn("BML native -> script export AddOne smoke failed: out of memory");
            return;
        }

        int result = 0;
        int status = BML_CallFrame_SetInt(frame, 0, 40);
        if (status == BML_OK)
            status = BML_CallModExport(exportHandle, frame);
        if (status == BML_OK)
            BML_CallFrame_GetResultInt(frame, &result);

        GetLogger()->Info("BML native -> script export AddOne status=%d result=%d", status, result);
        DestroyFrame(frame);
        ReleaseExport(exportHandle);
    }

    void CallScriptSumSmoke() {
        BML_ModExport *exportHandle = BML_FindModExport("bml.bindings.smoke", "Sum", "int Sum(int, int)");
        if (!exportHandle)
            return;

        BML_CallFrame *frame = BML_CreateCallFrame();
        if (!frame) {
            ReleaseExport(exportHandle);
            GetLogger()->Warn("BML native -> script export Sum smoke failed: out of memory");
            return;
        }

        int result = 0;
        int status = BML_CallFrame_SetInt(frame, 0, 20);
        if (status == BML_OK)
            status = BML_CallFrame_SetInt(frame, 1, 22);
        if (status == BML_OK)
            status = BML_CallModExport(exportHandle, frame);
        if (status == BML_OK)
            BML_CallFrame_GetResultInt(frame, &result);

        GetLogger()->Info("BML native -> script export Sum status=%d result=%d", status, result);
        DestroyFrame(frame);
        ReleaseExport(exportHandle);
    }

    bool m_Checked = false;
    bool m_InteropChecked = false;
    int m_Frames = 0;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BMLNativeInteropSmokeMod(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
