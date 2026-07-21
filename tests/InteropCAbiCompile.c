/* This target is intentionally compiled as C, not C++.  It protects the
 * public Interop boundary from accidentally gaining a C++-only declaration,
 * exported C++ type, STL dependency, or exception ABI requirement. */
#include "BML/EventKinds.h"
#include "BML/InteropApi.h"
#include "BML/Generated/bml_events_api.h"
#include "BML/Generated/bml_gameplay_api.h"
#include "BML/Generated/bml_runtime_api.h"
#include "BML/Generated/bml_scene_api.h"
#include "BML/Generated/bml_ui_api.h"

typedef char ObjectRefMustBeThreeWords[(sizeof(BML_ObjectRef) == sizeof(uint32_t) * 3u) ? 1 : -1];
typedef char Mat4MustBeSixteenFloats[(sizeof(BML_Mat4) == sizeof(float) * 16u) ? 1 : -1];

static int ReadResource(const BML_InteropProviderRequest *request,
                        BML_InteropRecordBuilder *record,
                        void *userdata) {
    (void)request;
    (void)record;
    (void)userdata;
    return BML_OK;
}

int bml_interop_c_abi_compile_probe(void) {
    BML_InteropFieldDescriptor fields[] = {
        {1u, "value", BML_INTEROP_FIELD_INT, 0},
    };
    BML_InteropSchemaDescriptor schemas[] = {
        {1u, "sample", fields, sizeof(fields) / sizeof(fields[0])},
    };
    BML_InteropEndpointDescriptor endpoints[] = {
        {"state", BML_INTEROP_ENDPOINT_RESOURCE, 0u, 1u, 0},
    };
    BML_InteropApiDescriptor api = {
        sizeof(api), "test.c_abi", 1u, 0u, 1u,
        schemas, sizeof(schemas) / sizeof(schemas[0]),
        endpoints, sizeof(endpoints) / sizeof(endpoints[0]),
        0, 0,
    };
    BML_InteropProviderCallbacks callbacks = {
        sizeof(callbacks), 0, ReadResource, 0, 0, 0,
    };
    BML_ObjectRef object = {BML_INTEROP_OBJECT_DOMAIN_VIRTOOLS, 7u, 2u};
    return (int)(api.Major + callbacks.Size + object.Slot + BML_EVENT_DEAD +
                 BML_INTEROP_BML_RUNTIME_DESCRIPTOR.Major +
                 BML_INTEROP_BML_SCENE_DESCRIPTOR.Major +
                 BML_INTEROP_BML_GAMEPLAY_DESCRIPTOR.Major +
                 BML_INTEROP_BML_UI_DESCRIPTOR.Major +
                 BML_INTEROP_BML_EVENTS_DESCRIPTOR.Major);
}
