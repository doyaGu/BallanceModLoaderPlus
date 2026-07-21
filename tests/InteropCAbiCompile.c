/* This target is intentionally compiled as C, not C++.  It protects the
 * public Interop boundary from accidentally gaining a C++-only declaration,
 * exported C++ type, STL dependency, or exception ABI requirement. */
#include "BML/EventKinds.h"
#include "BML/Imc.h"
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
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    BML_ImcCallOptions call_options = BML_IMC_CALL_OPTIONS_INIT;
    BML_ImcSubscribeOptions subscribe_options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    int (*get_subscription_dropped)(BML_ImcClient, BML_ImcSubscription, uint64_t *) =
        &BML_Imc_GetSubscriptionDroppedCount;
    int (*get_topic_subscribers)(BML_ImcClient, BML_ImcTopicId, size_t *) =
        &BML_Imc_GetTopicSubscriberCount;
    int (*is_rpc_available)(BML_ImcClient, BML_ImcRpcId, int *) =
        &BML_Imc_IsRpcAvailable;
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
                 message.Size + call_options.Size + subscribe_options.Size +
                 (get_subscription_dropped != 0) + (get_topic_subscribers != 0) +
                 (is_rpc_available != 0) +
                 BML_IMC_ABI_VERSION +
                 BML_INTEROP_BML_RUNTIME_DESCRIPTOR.Major +
                 BML_INTEROP_BML_SCENE_DESCRIPTOR.Major +
                 BML_INTEROP_BML_GAMEPLAY_DESCRIPTOR.Major +
                 BML_INTEROP_BML_UI_DESCRIPTOR.Major +
                 BML_INTEROP_BML_EVENTS_DESCRIPTOR.Major);
}
