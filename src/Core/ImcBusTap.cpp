#include "ImcBusInternal.h"

#include <Windows.h>

namespace BML::Core {
    void ImcBusImpl::FireTap(BML_Mod owner, BML_TopicId topic, const BML_ImcMessage *msg) {
        TapState *tap = m_Tap.load(std::memory_order_acquire);
        if (!tap) {
            return;
        }

        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);

        BML_ImcMessageTrace trace{};
        trace.topic = topic;
        trace.owner = owner;
        trace.timestamp_qpc = static_cast<uint64_t>(qpc.QuadPart);
        trace.payload_size = msg ? msg->size : 0;
        trace.payload_type_id = msg ? msg->payload_type_id : BML_PAYLOAD_TYPE_NONE;
        trace.priority = msg ? msg->priority : BML_IMC_PRIORITY_NORMAL;
        trace.flags = msg ? msg->flags : 0;

        tap->callback(&trace, tap->user_data);
    }

    BML_Result ImcBusImpl::RegisterMessageTap(BML_Mod owner, BML_ImcMessageTap tap,
                                              void *user_data) {
        if (!tap) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        TapState *new_tap = new (std::nothrow) TapState{tap, user_data, resolved_owner};
        if (!new_tap) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        TapState *old_tap = m_Tap.exchange(new_tap, std::memory_order_acq_rel);
        delete old_tap;

        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::UnregisterMessageTap(BML_Mod owner) {
        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        TapState *old_tap = m_Tap.exchange(nullptr, std::memory_order_acq_rel);
        if (!old_tap) {
            return BML_RESULT_NOT_FOUND;
        }

        if (old_tap->owner != resolved_owner) {
            // Restore the tap since owner doesn't match
            m_Tap.store(old_tap, std::memory_order_release);
            return BML_RESULT_NOT_FOUND;
        }

        delete old_tap;
        return BML_RESULT_OK;
    }
} // namespace BML::Core
