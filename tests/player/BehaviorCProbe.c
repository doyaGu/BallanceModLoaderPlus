#include "BehaviorCProbe.h"

#include <stdlib.h>
#include <string.h>

static BML_BehaviorStatus EmptyStatus(void) {
    BML_BehaviorStatus status;
    memset(&status, 0, sizeof(status));
    status.StructSize = sizeof(status);
    return status;
}

static BML_BehaviorString Text(const char *text) {
    BML_BehaviorString value;
    value.Data = text;
    value.Length = (uint32_t) strlen(text);
    return value;
}

static int RangeFits(uint32_t size, uint32_t offset, uint32_t count,
                     uint32_t stride) {
    const uint64_t end = (uint64_t) offset + (uint64_t) count * stride;
    return end <= size;
}

static int HasOut(const BML_BehaviorRunFrame *frame,
                  const unsigned char *payload, uint32_t payloadSize,
                  const char *expected) {
    uint32_t index;
    const uint32_t expectedLength = (uint32_t) strlen(expected);
    if (!frame || !payload || !frame->OutCount ||
        !RangeFits(payloadSize, frame->OutOffset, frame->OutCount,
                   (uint32_t) sizeof(BML_BehaviorOutRecord)))
        return 0;

    for (index = 0; index < frame->OutCount; ++index) {
        BML_BehaviorOutRecord record;
        const uint32_t offset = frame->OutOffset +
            index * (uint32_t) sizeof(BML_BehaviorOutRecord);
        memcpy(&record, payload + offset, sizeof(record));
        if (record.StructSize < sizeof(record) ||
            record.NameLength != expectedLength ||
            !RangeFits(payloadSize, record.NameOffset, record.NameLength, 1u))
            continue;
        if (memcmp(payload + record.NameOffset, expected,
                   expectedLength) == 0)
            return 1;
    }
    return 0;
}

int BML_TestBehaviorFromC(BML_BehaviorGuid prototype,
                          BML_BehaviorCProbeResult *result) {
    const void *found = NULL;
    const BML_BehaviorInterface *behavior = NULL;
    BML_BehaviorSession session = NULL;
    BML_BehaviorRun run = NULL;
    BML_BehaviorPrototypeInfo *prototypes = NULL;
    BML_BehaviorRunFrame *frames = NULL;
    unsigned char *prototypePayload = NULL;
    unsigned char *framePayload = NULL;
    BML_BehaviorStatus status = EmptyStatus();
    uint32_t prototypeCount = 0;
    uint32_t prototypePayloadSize = 0;
    uint32_t frameCount = 0;
    uint32_t framePayloadSize = 0;
    int closeCode = BML_OK;
    int code = BML_ERROR_FAIL;

    if (!result || result->StructSize < sizeof(*result))
        return BML_ERROR_INVALID_PARAMETER;
    memset(result, 0, sizeof(*result));
    result->StructSize = sizeof(*result);
    result->Code = BML_ERROR_FAIL;

    code = BML_GetInterface(BML_BEHAVIOR_INTERFACE_ID,
                            BML_BEHAVIOR_INTERFACE_MAJOR, &found);
    behavior = (const BML_BehaviorInterface *) found;
    if (code != BML_OK || !BML_BEHAVIOR_HAS_1_0(behavior))
        goto done;
    result->Checks |= BML_BEHAVIOR_C_PROBE_INTERFACE;

    {
        BML_BehaviorString ownerId;
        ownerId.Data = NULL;
        ownerId.Length = 0;
        code = behavior->OpenSession(ownerId, &session, &status);
    }
    if (code != BML_OK || !session)
        goto done;
    result->Checks |= BML_BEHAVIOR_C_PROBE_SESSION;

    {
        BML_BehaviorPrototypeQuery query;
        uint32_t actualCount = 0;
        uint32_t actualPayloadSize = 0;
        uint32_t index;
        BML_BehaviorPrototypeRef selected;
        int selectedFound = 0;

        memset(&query, 0, sizeof(query));
        query.StructSize = sizeof(query);
        query.Match = BML_BEHAVIOR_MATCH_PROTOTYPE;
        query.Prototype = prototype;
        status = EmptyStatus();
        code = behavior->FindPrototypes(
            session, &query, NULL, 0, sizeof(BML_BehaviorPrototypeInfo),
            NULL, 0, &prototypeCount, &prototypePayloadSize, &status);
        if (code != BML_ERROR_BUFFER_TOO_SMALL || prototypeCount == 0)
            goto done;

        prototypes = (BML_BehaviorPrototypeInfo *) calloc(
            prototypeCount, sizeof(BML_BehaviorPrototypeInfo));
        if (prototypePayloadSize != 0)
            prototypePayload = (unsigned char *) malloc(prototypePayloadSize);
        if (!prototypes || (prototypePayloadSize != 0 && !prototypePayload)) {
            code = BML_ERROR_OUT_OF_MEMORY;
            goto done;
        }

        status = EmptyStatus();
        code = behavior->FindPrototypes(
            session, &query, prototypes, prototypeCount,
            sizeof(BML_BehaviorPrototypeInfo), prototypePayload,
            prototypePayloadSize, &actualCount, &actualPayloadSize, &status);
        if (code != BML_OK || actualCount != prototypeCount ||
            actualPayloadSize != prototypePayloadSize)
            goto done;

        memset(&selected, 0, sizeof(selected));
        for (index = 0; index < actualCount; ++index) {
            const BML_BehaviorPrototypeInfo *candidate = &prototypes[index];
            if (candidate->StructSize < sizeof(*candidate) ||
                candidate->Ref.StructSize < sizeof(candidate->Ref)) {
                code = BML_ERROR_MALFORMED_MESSAGE;
                goto done;
            }
            if (candidate->Ref.Prototype.Data1 == prototype.Data1 &&
                candidate->Ref.Prototype.Data2 == prototype.Data2) {
                selected = candidate->Ref;
                selectedFound = 1;
                break;
            }
        }
        if (!selectedFound || selected.Generation == 0) {
            code = BML_ERROR_NOT_FOUND;
            goto done;
        }
        result->Checks |= BML_BEHAVIOR_C_PROBE_PROTOTYPE;

        {
            BML_ObjectRef owner;
            BML_BehaviorBlock block;
            BML_BehaviorFramePolicy retention;
            BML_BehaviorSelector input;
            BML_BehaviorRunInfo info;

            memset(&owner, 0, sizeof(owner));
            memset(&block, 0, sizeof(block));
            memset(&retention, 0, sizeof(retention));
            memset(&input, 0, sizeof(input));
            memset(&info, 0, sizeof(info));
            block.StructSize = sizeof(block);
            block.Prototype = prototype;
            block.Target.StructSize = sizeof(block.Target);
            block.Target.Kind = BML_BEHAVIOR_TARGET_OWNER;
            block.PrototypeGeneration = selected.Generation;
            retention.StructSize = sizeof(retention);
            retention.Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
            retention.Limit = 4;
            input.StructSize = sizeof(input);
            input.Kind = BML_BEHAVIOR_SELECTOR_UNIQUE_NAME;
            input.Name = Text("Run");
            info.StructSize = sizeof(info);
            status = EmptyStatus();
            code = behavior->Call(session, owner, &block, &retention, &input,
                                  &run, &info, &status);
            if (code != BML_OK || !run)
                goto done;
            result->Checks |= BML_BEHAVIOR_C_PROBE_CALL;
        }
    }

    status = EmptyStatus();
    code = behavior->TakeFrames(
        run, NULL, 0, sizeof(BML_BehaviorRunFrame), NULL, 0,
        &frameCount, &framePayloadSize, &status);
    if (code != BML_ERROR_BUFFER_TOO_SMALL || frameCount == 0)
        goto done;
    frames = (BML_BehaviorRunFrame *) calloc(
        frameCount, sizeof(BML_BehaviorRunFrame));
    if (framePayloadSize != 0)
        framePayload = (unsigned char *) malloc(framePayloadSize);
    if (!frames || (framePayloadSize != 0 && !framePayload)) {
        code = BML_ERROR_OUT_OF_MEMORY;
        goto done;
    }

    {
        uint32_t actualCount = 0;
        uint32_t actualPayloadSize = 0;
        uint32_t index;
        int foundOut = 0;
        status = EmptyStatus();
        code = behavior->TakeFrames(
            run, frames, frameCount, sizeof(BML_BehaviorRunFrame),
            framePayload, framePayloadSize, &actualCount, &actualPayloadSize,
            &status);
        if (code != BML_OK || actualCount != frameCount ||
            actualPayloadSize != framePayloadSize)
            goto done;
        result->Checks |= BML_BEHAVIOR_C_PROBE_FRAME;

        for (index = 0; index < actualCount; ++index) {
            if (frames[index].StructSize < sizeof(frames[index])) {
                code = BML_ERROR_MALFORMED_MESSAGE;
                goto done;
            }
            if (frames[index].Sequence > result->Sequence)
                result->Sequence = frames[index].Sequence;
            if (frames[index].Error == BML_BEHAVIOR_ERROR_NONE &&
                HasOut(&frames[index], framePayload, framePayloadSize,
                       "Done"))
                foundOut = 1;
        }
        if (!foundOut) {
            code = BML_ERROR_FAIL;
            goto done;
        }
        result->Checks |= BML_BEHAVIOR_C_PROBE_OUT;
    }

done:
    if (run) {
        closeCode = behavior->CloseRun(run);
        if (code == BML_OK && closeCode != BML_OK)
            code = closeCode;
    }
    if (session) {
        closeCode = behavior->CloseSession(session);
        if (code == BML_OK && closeCode != BML_OK)
            code = closeCode;
    }
    if (run && session && code == BML_OK)
        result->Checks |= BML_BEHAVIOR_C_PROBE_CLOSED;

    free(framePayload);
    free(frames);
    free(prototypePayload);
    free(prototypes);
    result->Code = code;
    result->Status = status;
    return code;
}
