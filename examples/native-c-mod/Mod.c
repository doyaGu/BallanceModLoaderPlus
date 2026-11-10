#include <BML/Mod.h>

/* The instance belongs to this C module; even a stateless Mod returns a real,
 * non-null instance. No C++ runtime, CK2 or ImGui headers are required. */
static struct {
    BML_ModContextHandle Context;
} instance;

static int BML_CDECL Create(BML_ModContextHandle context, void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    instance.Context = context;
    *out = &instance;
    return BML_OK;
}

static void BML_CDECL Destroy(void *value) {
    (void)value;
    instance.Context = 0;
}

static int BML_CDECL OnLoad(void *value, BML_ModContextHandle context) {
    const void *found = NULL;
    const BML_ModContextInterface *host;
    int status;
    (void)value;
    status = BML_GetInterface(BML_MOD_CONTEXT_INTERFACE_ID, 1, &found);
    if (status != BML_OK)
        return status;
    host = (const BML_ModContextInterface *)found;
    return host->Log(context, BML_LOG_INFO, "C Mod loaded", sizeof("C Mod loaded") - 1);
}

static const BML_ModDefinition definition = {sizeof(BML_ModDefinition),
                                             "BMLCExample",
                                             "C Mod Example",
                                             "1.0.0",
                                             "BML",
                                             "Minimal native Mod using the C protocol",
                                             BML_VERSION,
                                             NULL,
                                             0,
                                             0};
static const BML_ModCallbacks callbacks = {.StructSize = sizeof(BML_ModCallbacks),
                                           .OnLoad = OnLoad};
static const BML_ModInterface mod = {BML_IFACE_HEADER(BML_ModInterface, BML_MOD_INTERFACE_ID, 1, 0),
                                     &definition, &callbacks, Create, Destroy};

BML_MOD_ENTRY(int) BML_GetModInterface(uint16_t major, const BML_ModInterface **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = NULL;
    if (major != 1)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &mod;
    return BML_OK;
}
