/* Intentionally compiled as C to protect the public IMC DLL boundary. */
#include "BML/Imc.h"
#include "BML/Types.h"

typedef char ObjectRefMustBeThreeWords[(sizeof(BML_ObjectRef) == sizeof(uint32_t) * 3u) ? 1 : -1];
typedef char Mat4MustBeSixteenFloats[(sizeof(BML_Mat4) == sizeof(float) * 16u) ? 1 : -1];

#if UINTPTR_MAX == UINT32_MAX
typedef char MessagePrefixMustBe40Bytes[(BML_IMC_MESSAGE_1_0_SIZE == 40u) ? 1 : -1];
typedef char RpcOptionsPrefixMustBe12Bytes[(BML_IMC_RPC_REGISTRATION_OPTIONS_1_0_SIZE == 12u) ? 1 : -1];
typedef char CallOptionsPrefixMustBe12Bytes[(BML_IMC_CALL_OPTIONS_1_0_SIZE == 12u) ? 1 : -1];
typedef char SubscribeOptionsPrefixMustBe24Bytes[(BML_IMC_SUBSCRIBE_OPTIONS_1_0_SIZE == 24u) ? 1 : -1];
typedef char StatsPrefixMustBe76Bytes[(BML_IMC_STATS_1_0_SIZE == 76u) ? 1 : -1];
#endif

int bml_imc_c_abi_compile_probe(void) {
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    BML_ImcRpcRegistrationOptions registration = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    BML_ImcCallOptions call_options = BML_IMC_CALL_OPTIONS_INIT;
    BML_ImcSubscribeOptions subscribe_options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    BML_ImcStats stats = BML_IMC_STATS_INIT;
    BML_ObjectRef object = {BML_OBJECT_DOMAIN_VIRTOOLS, 7u, 2u};
    int (BML_CDECL *open_client)(const char *, BML_ImcClient *) =
        &BML_Imc_OpenClient;
    int (BML_CDECL *is_rpc_available)(BML_ImcClient, BML_ImcRpcId, int *) =
        &BML_Imc_IsRpcAvailable;
    int (BML_CDECL *get_topic_subscribers)(BML_ImcClient, BML_ImcTopicId,
                                            size_t *) =
        &BML_Imc_GetTopicSubscriberCount;
    return (int)(message.Size + registration.Size + call_options.Size +
                 subscribe_options.Size + stats.Size + object.Slot +
                 (open_client != 0) + (is_rpc_available != 0) +
                 (get_topic_subscribers != 0) + BML_IMC_ABI_VERSION);
}
