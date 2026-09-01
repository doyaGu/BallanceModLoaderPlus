#include "BML/Behavior.h"

#include <stddef.h>
#include <stdint.h>

#define BML_BEHAVIOR_ABI_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

BML_BEHAVIOR_ABI_ASSERT(BehaviorGuidSize,
                        sizeof(BML_BehaviorGuid) == 8u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorStatusSize,
                        sizeof(BML_BehaviorStatus) == 296u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorFrameSize,
                        sizeof(BML_BehaviorRunFrame) == 64u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorOutRecordSize,
                        sizeof(BML_BehaviorOutRecord) == 20u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorPoutRecordSize,
                        sizeof(BML_BehaviorPoutRecord) == 40u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorDiagnosticRecordSize,
                        sizeof(BML_BehaviorDiagnosticRecord) == 44u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorPrototypeRefSize,
                        sizeof(BML_BehaviorPrototypeRef) == 24u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorManagerInfoSize,
                        sizeof(BML_BehaviorManagerInfo) == 16u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorPrototypeInfoSize,
                        sizeof(BML_BehaviorPrototypeInfo) == 96u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorSlotRecordSize,
                        sizeof(BML_BehaviorSlotRecord) == 48u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorLayoutSize,
                        sizeof(BML_BehaviorLayout) == 128u);

#if UINTPTR_MAX == UINT32_MAX
BML_BEHAVIOR_ABI_ASSERT(BehaviorSelectorSize,
                        sizeof(BML_BehaviorSelector) == 24u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorBindingSize,
                        sizeof(BML_BehaviorBinding) == 108u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorGenerationOffset,
                        offsetof(BML_BehaviorBlock,
                                 PrototypeGeneration) == 80u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorBlockSize,
                        sizeof(BML_BehaviorBlock) == 88u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorPrototypeQuerySize,
                        sizeof(BML_BehaviorPrototypeQuery) == 64u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorInterfaceSize,
                        sizeof(BML_BehaviorInterface) == 64u);
#else
BML_BEHAVIOR_ABI_ASSERT(BehaviorSelectorSize,
                        sizeof(BML_BehaviorSelector) == 32u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorBindingSize,
                        sizeof(BML_BehaviorBinding) == 120u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorGenerationOffset,
                        offsetof(BML_BehaviorBlock,
                                 PrototypeGeneration) == 96u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorBlockSize,
                        sizeof(BML_BehaviorBlock) == 104u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorPrototypeQuerySize,
                        sizeof(BML_BehaviorPrototypeQuery) == 96u);
BML_BEHAVIOR_ABI_ASSERT(BehaviorInterfaceSize,
                        sizeof(BML_BehaviorInterface) == 128u);
#endif

static int BML_BEHAVIOR_CALL BehaviorOpenSessionSignature(
    BML_BehaviorString owner_id,
    BML_BehaviorSession *session,
    BML_BehaviorStatus *status) {
    (void) owner_id;
    (void) session;
    (void) status;
    return BML_OK;
}

static int (BML_BEHAVIOR_CALL *BehaviorOpenSessionPointer)(
    BML_BehaviorString,
    BML_BehaviorSession *,
    BML_BehaviorStatus *) = &BehaviorOpenSessionSignature;

int BML_TestBehaviorCAbi(void) {
    return BehaviorOpenSessionPointer != 0;
}
