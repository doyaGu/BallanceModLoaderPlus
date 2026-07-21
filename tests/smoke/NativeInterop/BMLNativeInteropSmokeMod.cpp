#include "BML/IMod.h"
#include "BML/InteropApi.h"

#include <cstring>

namespace {

constexpr BML_InteropFieldDescriptor kStateFields[] = {
    {1u, "value", BML_INTEROP_FIELD_INT, 0},
};

constexpr BML_InteropSchemaDescriptor kSchemas[] = {
    {1u, "state", kStateFields, sizeof(kStateFields) / sizeof(kStateFields[0])},
};

constexpr BML_InteropEndpointDescriptor kEndpoints[] = {
    {"state", BML_INTEROP_ENDPOINT_RESOURCE, 0u, 1u, 0},
};

constexpr BML_InteropApiDescriptor kApi = {
    sizeof(BML_InteropApiDescriptor),
    "bml.smoke.api",
    1u,
    0u,
    0x5467F4E62C5ADA4DULL,
    kSchemas,
    sizeof(kSchemas) / sizeof(kSchemas[0]),
    kEndpoints,
    sizeof(kEndpoints) / sizeof(kEndpoints[0]),
    nullptr,
    0,
};

int ReadState(const BML_InteropProviderRequest *, BML_InteropRecordBuilder *record, void *) {
    const int value = 42;
    return BML_Interop_RecordBuilder_SetValue(record,
                                              1u,
                                              BML_INTEROP_FIELD_INT,
                                              &value,
                                              1u);
}

const BML_InteropProviderCallbacks kCallbacks = {
    sizeof(BML_InteropProviderCallbacks),
    nullptr,
    &ReadState,
    nullptr,
    nullptr,
    nullptr,
};

int VerifyScriptProviderState(BML_RecordRef record, int &outValue) {
    outValue = 0;
    int status = BML_Interop_RecordGetInt(record, 1u, &outValue);
    if (status != BML_OK || outValue != 42)
        return status != BML_OK ? status : BML_ERROR_INTEROP_RECORD_INVALID;

    const void *data = nullptr;
    size_t count = 0;
    size_t elementSize = 0;
    status = BML_Interop_RecordBorrowValue(record,
                                            2u,
                                            BML_INTEROP_FIELD_INT_ARRAY,
                                            &data,
                                            &count,
                                            &elementSize);
    if (status != BML_OK)
        return status;
    const int *numbers = static_cast<const int *>(data);
    if (elementSize != sizeof(int) || count != 3 || !numbers ||
        numbers[0] != 1 || numbers[1] != 2 || numbers[2] != 3) {
        return BML_ERROR_INTEROP_RECORD_INVALID;
    }

    size_t stringCount = 0;
    status = BML_Interop_RecordGetStringArrayCount(record, 3u, &stringCount);
    if (status != BML_OK)
        return status;
    if (stringCount != 2)
        return BML_ERROR_INTEROP_RECORD_INVALID;
    const char *expectedNames[] = {"alpha", "beta"};
    for (size_t index = 0; index < stringCount; ++index) {
        char value[16]{};
        size_t required = 0;
        status = BML_Interop_RecordGetStringArrayItem(record, 3u, index, value, sizeof(value), &required);
        if (status != BML_OK)
            return status;
        if (required != std::strlen(expectedNames[index]) + 1 || std::strcmp(value, expectedNames[index]) != 0)
            return BML_ERROR_INTEROP_RECORD_INVALID;
    }

    data = nullptr;
    count = 0;
    elementSize = 0;
    status = BML_Interop_RecordBorrowValue(record,
                                            4u,
                                            BML_INTEROP_FIELD_VEC3_ARRAY,
                                            &data,
                                            &count,
                                            &elementSize);
    if (status != BML_OK)
        return status;
    const BML_Vec3 *points = static_cast<const BML_Vec3 *>(data);
    if (elementSize != sizeof(BML_Vec3) || count != 1 || !points ||
        points[0].x != 1.0f || points[0].y != 2.0f || points[0].z != 3.0f) {
        return BML_ERROR_INTEROP_RECORD_INVALID;
    }

    data = nullptr;
    count = 0;
    elementSize = 0;
    status = BML_Interop_RecordBorrowValue(record,
                                            5u,
                                            BML_INTEROP_FIELD_MAT4_ARRAY,
                                            &data,
                                            &count,
                                            &elementSize);
    if (status != BML_OK)
        return status;
    const BML_Mat4 *matrices = static_cast<const BML_Mat4 *>(data);
    if (elementSize != sizeof(BML_Mat4) || count != 1 || !matrices ||
        matrices[0].m00 != 1.0f || matrices[0].m11 != 1.0f ||
        matrices[0].m22 != 1.0f || matrices[0].m33 != 1.0f) {
        return BML_ERROR_INTEROP_RECORD_INVALID;
    }
    return BML_OK;
}

class BMLNativeInteropSmokeMod final : public IMod {
public:
    explicit BMLNativeInteropSmokeMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "bml.native.interop.smoke"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Interop API smoke"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Registers and consumes one C-ABI Interop API.";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        m_RegisterStatus = BML_Interop_RegisterProvider(&kApi, &kCallbacks, this);
        if (m_RegisterStatus != BML_OK) {
            Log("register", m_RegisterStatus, 0);
            return;
        }

        BML_RecordRef record{};
        int value = 0;
        int status = BML_Interop_ReadResource(kApi.ApiId, "state", &record);
        if (status == BML_OK)
            status = BML_Interop_RecordGetInt(record, 1u, &value);
        if (record.Value)
            (void)BML_Interop_ReleaseRecord(record);
        Log("read", status, value);
    }

    void OnProcess() override {
        if (m_RegisterStatus != BML_OK || m_ScriptConsumerLogged)
            return;

        BML_RecordRef record{};
        int value = 0;
        int status = BML_Interop_ReadResource("bml.smoke.script", "state", &record);
        if (status == BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND)
            return; // The dependent script has not completed OnLoad yet.
        if (status == BML_OK)
            status = VerifyScriptProviderState(record, value);
        if (record.Value)
            (void)BML_Interop_ReleaseRecord(record);
        Log("script-read", status, value);
        m_ScriptConsumerLogged = true;
    }

    void OnUnload() override {
        if (m_RegisterStatus == BML_OK)
            (void)BML_Interop_UnregisterProvider(kApi.ApiId);
        m_RegisterStatus = BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    }

private:
    void Log(const char *operation, int status, int value) {
        if (ILogger *logger = GetLogger())
            logger->Info("Interop API smoke %s: status=%d value=%d", operation, status, value);
    }

    int m_RegisterStatus = BML_ERROR_INTEROP_PROVIDER_UNLOADED;
    bool m_ScriptConsumerLogged = false;
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BMLNativeInteropSmokeMod(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
