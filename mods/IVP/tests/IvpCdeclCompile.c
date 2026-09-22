#include "BML/IVP.h"

static int BML_CDECL ReadApiInfo(BML_IvpApiInfo *out) {
    return out != NULL ? BML_OK : BML_ERROR_INVALID_PARAMETER;
}

static int BML_CDECL ReadPointer(uintptr_t *out) {
    if (out != NULL)
        *out = 0;
    return BML_OK;
}

static int BML_CDECL ReadObject(void *entity, uintptr_t *out) {
    (void) entity;
    return ReadPointer(out);
}

static int BML_CDECL ResolveSymbol(const char *name, uintptr_t *out) {
    (void) name;
    return ReadPointer(out);
}

static int BML_CDECL ResolveRva(uint32_t rva, uintptr_t *out) {
    (void) rva;
    return ReadPointer(out);
}

static uint32_t BML_CDECL GetSymbolCount(void) {
    return 0;
}

static int BML_CDECL GetSymbol(uint32_t index, BML_IvpSymbol *out) {
    (void) index;
    (void) out;
    return BML_ERROR_NOT_FOUND;
}

static const BML_IvpInterface Interface = {
    BML_IFACE_HEADER(BML_IvpInterface, BML_IVP_INTERFACE_ID,
                     BML_IVP_INTERFACE_MAJOR, BML_IVP_INTERFACE_MINOR),
    &ReadApiInfo,
    &ReadPointer,
    &ReadPointer,
    &ReadObject,
    &ReadObject,
    &ReadObject,
    &ReadObject,
    &ResolveSymbol,
    &ResolveRva,
    &GetSymbolCount,
    &GetSymbol,
};

const BML_IvpInterface *BML_CDECL BML_TestIvpCdeclSurface(void) {
    return &Interface;
}
