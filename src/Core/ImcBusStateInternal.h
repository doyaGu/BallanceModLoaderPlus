#ifndef BML_CORE_IMC_BUS_STATE_INTERNAL_H
#define BML_CORE_IMC_BUS_STATE_INTERNAL_H

#include "ImcBusSharedInternal.h"

namespace BML::Core {
    struct RetainedStateEntry {
        BML_Mod owner{nullptr};
        uint64_t timestamp{0};
        uint32_t flags{0};
        uint32_t priority{BML_IMC_PRIORITY_NORMAL};
        BufferStorage payload;
    };

    struct RetainedStateStore {
        std::mutex state_mutex;
        std::unordered_map<BML_TopicId, RetainedStateEntry> retained_states;
    };

    struct ImcGlobalStats {
        std::atomic<uint64_t> total_messages_published{0};
        std::atomic<uint64_t> total_messages_delivered{0};
        std::atomic<uint64_t> total_messages_dropped{0};
        std::atomic<uint64_t> total_bytes_published{0};
        std::atomic<uint64_t> total_rpc_calls{0};
        std::atomic<uint64_t> total_rpc_completions{0};
        std::atomic<uint64_t> total_rpc_failures{0};
        std::atomic<uint64_t> pump_cycles{0};
        std::atomic<uint64_t> last_pump_time{0};
        std::atomic<uint64_t> start_time{0};

        ImcGlobalStats() {
            start_time.store(GetTimestampNs(), std::memory_order_relaxed);
        }

        void Reset() {
            total_messages_published.store(0, std::memory_order_relaxed);
            total_messages_delivered.store(0, std::memory_order_relaxed);
            total_messages_dropped.store(0, std::memory_order_relaxed);
            total_bytes_published.store(0, std::memory_order_relaxed);
            total_rpc_calls.store(0, std::memory_order_relaxed);
            total_rpc_completions.store(0, std::memory_order_relaxed);
            total_rpc_failures.store(0, std::memory_order_relaxed);
            pump_cycles.store(0, std::memory_order_relaxed);
            start_time.store(GetTimestampNs(), std::memory_order_relaxed);
        }
    };
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_STATE_INTERNAL_H
