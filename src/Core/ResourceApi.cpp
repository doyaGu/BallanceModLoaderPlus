#include "ResourceApi.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "ModHandle.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

using BML::Core::Context;

/* ============================================================================
 * Resource Handle Management Implementation
 * ============================================================================ */

namespace {
    struct ControlBlock {
        std::atomic<uint32_t> ref_count{1};
        std::atomic<void *> user_data{nullptr};
    };

    struct HandleSlot {
        ControlBlock *control{nullptr};
        uint32_t generation{0};
        bool in_use{false};
    };

    struct HandleTable {
        std::vector<HandleSlot> slots;
        std::vector<uint32_t> free_list;
        mutable std::shared_mutex mutex;
    };

    using HandleTablePtr = std::shared_ptr<HandleTable>;

    std::unordered_map<BML_HandleType, HandleTablePtr> g_HandleTables;
    std::shared_mutex g_TablesMutex;

    struct ResourceTypeMetadata {
        std::string name;
        BML_ResourceHandleFinalize finalize{nullptr};
        void *user_data{nullptr};
        std::string provider_id;
        bool active{true};
    };

    std::unordered_map<BML_HandleType, ResourceTypeMetadata> g_ResourceMetadata;
    std::shared_mutex g_ResourceMetadataMutex;
    constexpr BML_HandleType kFirstDynamicResourceType = 1u << 30;
    std::atomic<BML_HandleType> g_NextResourceType{kFirstDynamicResourceType};

    HandleTablePtr GetOrCreateTable(BML_HandleType type) {
        {
            std::shared_lock shared_lock(g_TablesMutex);
            auto it = g_HandleTables.find(type);
            if (it != g_HandleTables.end()) {
                return it->second;
            }
        }

        std::unique_lock unique_lock(g_TablesMutex);
        auto &entry = g_HandleTables[type];
        if (!entry) {
            entry = std::make_shared<HandleTable>();
        }
        return entry;
    }

    HandleTablePtr FindTable(BML_HandleType type) {
        std::shared_lock lock(g_TablesMutex);
        auto it = g_HandleTables.find(type);
        return it != g_HandleTables.end() ? it->second : nullptr;
    }

    ControlBlock *AllocateControlBlock() {
        auto *block = new(std::nothrow) ControlBlock();
        if (block) {
            block->ref_count.store(1, std::memory_order_relaxed);
            block->user_data.store(nullptr, std::memory_order_relaxed);
        }
        return block;
    }

    std::string GetCurrentProviderId() {
        auto &ctx = Context::Instance();
        if (auto *mod = ctx.ResolveModHandle(Context::GetCurrentModule())) {
            return mod->id;
        }
        return "BML";
    }
} // namespace

/* ============================================================================
 * C API Implementation
 * ============================================================================ */

static BML_Result BML_HandleCreateImpl(BML_HandleType type, BML_HandleDesc *out_desc) {
    if (!out_desc) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    {
        std::shared_lock metaLock(g_ResourceMetadataMutex);
        auto metaIt = g_ResourceMetadata.find(type);
        if (metaIt == g_ResourceMetadata.end() || !metaIt->second.active) {
            return BML_RESULT_INVALID_HANDLE;
        }
    }

    auto table = GetOrCreateTable(type);
    if (!table) {
        return BML_RESULT_OUT_OF_MEMORY;
    }

    auto *control = AllocateControlBlock();
    if (!control) {
        return BML_RESULT_OUT_OF_MEMORY;
    }

    std::unique_lock lock(table->mutex);

    uint32_t slot_index;
    if (!table->free_list.empty()) {
        slot_index = table->free_list.back();
        table->free_list.pop_back();
    } else {
        slot_index = static_cast<uint32_t>(table->slots.size());
        table->slots.emplace_back();
    }

    auto &slot = table->slots[slot_index];
    slot.control = control;
    slot.in_use = true;

    out_desc->struct_size = sizeof(BML_HandleDesc);
    out_desc->type = type;
    out_desc->generation = slot.generation;
    out_desc->slot = slot_index;

    return BML_RESULT_OK;
}

static BML_Result BML_HandleRetainImpl(const BML_HandleDesc *desc) {
    if (!desc) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    std::shared_lock lock(table->mutex);

    if (desc->slot >= table->slots.size()) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto &slot = table->slots[desc->slot];
    if (!slot.in_use || slot.generation != desc->generation || !slot.control) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    slot.control->ref_count.fetch_add(1, std::memory_order_acq_rel);
    return BML_RESULT_OK;
}

static BML_Result BML_HandleReleaseImpl(const BML_HandleDesc *desc) {
    if (!desc) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // First, get a shared lock to validate and attempt the atomic decrement
    ControlBlock *control = nullptr;
    {
        std::shared_lock shared_lock(table->mutex);

        if (desc->slot >= table->slots.size()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto &slot = table->slots[desc->slot];
        if (!slot.in_use || slot.generation != desc->generation || !slot.control) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        control = slot.control;

        // Use compare-exchange loop for safe decrement with underflow protection
        uint32_t current = control->ref_count.load(std::memory_order_acquire);
        while (true) {
            if (current == 0) {
                // Already at zero - invalid state, don't decrement
                return BML_RESULT_INVALID_STATE;
            }
            // Try to decrement atomically
            if (control->ref_count.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Successfully decremented from 'current' to 'current - 1'
                break;
            }
            // CAS failed, 'current' now contains the updated value, retry
        }

        // If we didn't decrement to zero, we're done
        if (current != 1) {
            return BML_RESULT_OK;
        }
    }
    // Shared lock released here

    // We decremented from 1 to 0 - we need to finalize and free the slot
    // Upgrade to unique lock for slot cleanup
    BML_ResourceHandleFinalize finalize = nullptr;
    void *finalize_user_data = nullptr;

    {
        std::unique_lock unique_lock(table->mutex);

        // Re-check slot state in case another thread raced us
        if (desc->slot >= table->slots.size()) {
            // Slot no longer exists (shouldn't happen)
            return BML_RESULT_OK;
        }

        auto &slot = table->slots[desc->slot];

        // Verify the slot still has our control block (prevents double-free race)
        if (!slot.in_use || slot.generation != desc->generation || slot.control != control) {
            // Another thread already cleaned up, or slot was reused
            return BML_RESULT_OK;
        }

        // Get finalizer before cleanup
        {
            std::shared_lock metaLock(g_ResourceMetadataMutex);
            auto it = g_ResourceMetadata.find(desc->type);
            if (it != g_ResourceMetadata.end() && it->second.active) {
                finalize = it->second.finalize;
                finalize_user_data = it->second.user_data;
            }
        }

        // Cleanup the slot
        delete control;
        slot.control = nullptr;
        slot.in_use = false;

        uint32_t next_generation = slot.generation + 1;
        if (next_generation == 0) {
            // Prevent generation wrap (ABA); retire the slot permanently
            slot.generation = std::numeric_limits<uint32_t>::max();
        } else {
            slot.generation = next_generation;
            table->free_list.push_back(desc->slot);
        }
    }
    // Unique lock released here

    // Call finalizer outside the lock with exception protection
    if (finalize) {
        BML_HandleDesc copy = *desc;
        try {
            finalize(Context::Instance().GetHandle(), &copy, finalize_user_data);
        } catch (...) {
            // Finalizer threw exception - log and swallow
            // Handle is already freed, nothing more we can do
        }
    }

    return BML_RESULT_OK;
}

static BML_Result BML_HandleValidateImpl(const BML_HandleDesc *desc, BML_Bool *out_valid) {
    if (!desc || !out_valid) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        *out_valid = BML_FALSE;
        return BML_RESULT_OK;
    }

    std::shared_lock lock(table->mutex);

    *out_valid = BML_FALSE;

    if (desc->slot >= table->slots.size()) {
        return BML_RESULT_OK;
    }

    auto &slot = table->slots[desc->slot];
    if (slot.in_use && slot.generation == desc->generation) {
        *out_valid = BML_TRUE;
    }

    return BML_RESULT_OK;
}

static BML_Result BML_HandleAttachUserDataImpl(const BML_HandleDesc *desc, void *user_data) {
    if (!desc) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    std::shared_lock lock(table->mutex);

    if (desc->slot >= table->slots.size()) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto &slot = table->slots[desc->slot];
    if (!slot.in_use || slot.generation != desc->generation || !slot.control) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    slot.control->user_data.store(user_data, std::memory_order_release);
    return BML_RESULT_OK;
}

static BML_Result BML_HandleGetUserDataImpl(const BML_HandleDesc *desc, void **out_user_data) {
    if (!desc || !out_user_data) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    std::shared_lock lock(table->mutex);

    if (desc->slot >= table->slots.size()) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto &slot = table->slots[desc->slot];
    if (!slot.in_use || slot.generation != desc->generation || !slot.control) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    *out_user_data = slot.control->user_data.load(std::memory_order_acquire);
    return BML_RESULT_OK;
}

static BML_Result BML_GetResourceCapsImpl(BML_ResourceCaps *out_caps) {
    if (!out_caps)
        return BML_RESULT_INVALID_ARGUMENT;
    size_t type_count = 0;
    {
        std::shared_lock lock(g_TablesMutex);
        type_count = g_HandleTables.size();
    }
    BML_ResourceCaps caps{};
    caps.struct_size = sizeof(BML_ResourceCaps);
    caps.api_version = bmlGetApiVersion();
    caps.capability_flags = BML_RESOURCE_CAP_STRONG_REFERENCES |
        BML_RESOURCE_CAP_USER_DATA |
        BML_RESOURCE_CAP_THREAD_SAFE |
        BML_RESOURCE_CAP_TYPE_ISOLATION;
    const auto clamped = std::min<size_t>(type_count, static_cast<size_t>((std::numeric_limits<uint32_t>::max)()));
    caps.active_handle_types = static_cast<uint32_t>(clamped);
    caps.user_data_alignment = static_cast<uint32_t>(alignof(void *));
    *out_caps = caps;
    return BML_RESULT_OK;
}

/* ============================================================================
 * Registration Entry Point
 * ============================================================================ */

namespace BML::Core {
    // ============================================================================
    // Resource API Implementation Functions
    // ============================================================================

    BML_Result BML_API_HandleCreate(BML_HandleType type, BML_HandleDesc *out_desc) {
        return BML_HandleCreateImpl(type, out_desc);
    }

    BML_Result BML_API_HandleRetain(const BML_HandleDesc *desc) {
        return BML_HandleRetainImpl(desc);
    }

    BML_Result BML_API_HandleRelease(const BML_HandleDesc *desc) {
        return BML_HandleReleaseImpl(desc);
    }

    BML_Result BML_API_HandleValidate(const BML_HandleDesc *desc, BML_Bool *out_valid) {
        return BML_HandleValidateImpl(desc, out_valid);
    }

    BML_Result BML_API_HandleAttachUserData(const BML_HandleDesc *desc, void *user_data) {
        return BML_HandleAttachUserDataImpl(desc, user_data);
    }

    BML_Result BML_API_HandleGetUserData(const BML_HandleDesc *desc, void **out_user_data) {
        return BML_HandleGetUserDataImpl(desc, out_user_data);
    }

    BML_Result BML_API_ResourceGetCaps(BML_ResourceCaps *out_caps) {
        return BML_GetResourceCapsImpl(out_caps);
    }

    BML_Result BML_API_RegisterResourceType(const BML_ResourceTypeDesc *desc, BML_HandleType *out_type) {
        return RegisterResourceType(desc, out_type);
    }

    void RegisterResourceApis() {
        BML_BEGIN_API_REGISTRATION();

        // Handle management APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleCreate, "resource", BML_API_HandleCreate, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleRetain, "resource", BML_API_HandleRetain, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleRelease, "resource", BML_API_HandleRelease, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleValidate, "resource", BML_API_HandleValidate, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleAttachUserData, "resource", BML_API_HandleAttachUserData, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlHandleGetUserData, "resource", BML_API_HandleGetUserData, BML_CAP_HANDLE_SYSTEM);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlResourceGetCaps, "resource", BML_API_ResourceGetCaps, BML_CAP_HANDLE_SYSTEM);

        // Resource type registration
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRegisterResourceType, "resource", BML_API_RegisterResourceType, BML_CAP_HANDLE_SYSTEM);
    }

    BML_Result RegisterResourceType(const BML_ResourceTypeDesc *desc, BML_HandleType *out_type) {
        if (!desc || desc->struct_size < sizeof(BML_ResourceTypeDesc) || !out_type || !desc->name || desc->name[0] == '\0') {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto type = g_NextResourceType.fetch_add(1, std::memory_order_relaxed);
        if (type == 0) {
            return BML_RESULT_FAIL;
        }

        ResourceTypeMetadata metadata;
        metadata.name = desc->name;
        metadata.finalize = desc->on_finalize;
        metadata.user_data = desc->user_data;
        metadata.provider_id = GetCurrentProviderId();

        {
            std::unique_lock lock(g_ResourceMetadataMutex);
            g_ResourceMetadata.emplace(type, std::move(metadata));
        }

        *out_type = type;
        return BML_RESULT_OK;
    }

    void UnregisterResourceTypesForProvider(const std::string &provider_id) {
        if (provider_id.empty()) {
            return;
        }

        std::unique_lock lock(g_ResourceMetadataMutex);
        for (auto &[type, meta] : g_ResourceMetadata) {
            if (meta.provider_id == provider_id && meta.active) {
                meta.active = false;
                meta.finalize = nullptr;
                meta.user_data = nullptr;
            }
        }
    }
} // namespace BML::Core
