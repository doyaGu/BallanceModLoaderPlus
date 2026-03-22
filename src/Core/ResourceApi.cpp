#include "ResourceApi.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "ModHandle.h"
#include "CoreErrors.h"

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
using BML::Core::Kernel;

/* ============================================================================
 * Resource Handle Management Implementation
 * ============================================================================ */

namespace {
    struct ControlBlock {
        std::atomic<uint32_t> ref_count{1};
        std::atomic<void *> user_data{nullptr};
        std::string owner_id;
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

    BML_Mod ResolveLegacyOwner(Context &context) {
        if (auto current = Context::GetCurrentModule()) {
            return current;
        }
        return context.GetSyntheticHostModule();
    }

    std::string ResolveProviderId(Context &context, BML_Mod owner) {
        if (auto *mod = context.ResolveModHandle(owner)) {
            return mod->id;
        }
        return {};
    }

    constexpr size_t kHandleDescMinSize = sizeof(BML_HandleDesc);

    bool HasValidHandleDescriptor(const BML_HandleDesc *desc) {
        return desc && desc->struct_size >= kHandleDescMinSize;
    }

    bool HasValidHandleCreateOutput(const BML_HandleDesc *desc) {
        return desc && (desc->struct_size == 0 || desc->struct_size >= kHandleDescMinSize);
    }

} // namespace

/* ============================================================================
 * C API Implementation
 * ============================================================================ */

static BML_Result BML_HandleCreateImpl(const std::string &owner_id,
                                       BML_HandleType type,
                                       BML_HandleDesc *out_desc) {
    if (!HasValidHandleCreateOutput(out_desc)) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (owner_id.empty()) {
        return BML_RESULT_INVALID_CONTEXT;
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
    slot.control->owner_id = owner_id;

    out_desc->struct_size = sizeof(BML_HandleDesc);
    out_desc->type = type;
    out_desc->generation = slot.generation;
    out_desc->slot = slot_index;

    return BML_RESULT_OK;
}

static BML_Result BML_HandleRetainImpl(const BML_HandleDesc *desc) {
    if (!HasValidHandleDescriptor(desc)) {
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

static BML_Result BML_HandleReleaseImpl(BML_Context finalize_ctx, const BML_HandleDesc *desc) {
    if (!HasValidHandleDescriptor(desc)) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto table = FindTable(desc->type);
    if (!table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    BML_ResourceHandleFinalize finalize = nullptr;
    void *finalize_user_data = nullptr;
    BML_HandleDesc finalized_desc = *desc;

    {
        std::unique_lock unique_lock(table->mutex);

        if (desc->slot >= table->slots.size()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto &slot = table->slots[desc->slot];
        if (!slot.in_use || slot.generation != desc->generation || !slot.control) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ControlBlock *control = slot.control;
        uint32_t current = control->ref_count.load(std::memory_order_acquire);
        if (current == 0) {
            return BML_RESULT_INVALID_STATE;
        }
        control->ref_count.store(current - 1, std::memory_order_release);

        if (current != 1) {
            return BML_RESULT_OK;
        }

        {
            std::shared_lock metaLock(g_ResourceMetadataMutex);
            auto it = g_ResourceMetadata.find(desc->type);
            if (it != g_ResourceMetadata.end() && it->second.active) {
                finalize = it->second.finalize;
                finalize_user_data = it->second.user_data;
            }
        }

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

    if (finalize) {
        try {
            finalize(finalize_ctx, &finalized_desc, finalize_user_data);
        } catch (...) {
        }
    }

    return BML_RESULT_OK;
}

static BML_Result BML_HandleValidateImpl(const BML_HandleDesc *desc, BML_Bool *out_valid) {
    if (!HasValidHandleDescriptor(desc) || !out_valid) {
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
    if (!HasValidHandleDescriptor(desc)) {
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
    if (!HasValidHandleDescriptor(desc) || !out_user_data) {
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

/* ============================================================================
 * Registration Entry Point
 * ============================================================================ */

namespace BML::Core {
    // ============================================================================
    // Resource API Implementation Functions
    // ============================================================================

    BML_Result BML_API_HandleCreate(BML_HandleType type, BML_HandleDesc *out_desc) {
        auto &context = *Kernel().context;
        return BML_HandleCreateImpl(
            ResolveProviderId(context, ResolveLegacyOwner(context)), type, out_desc);
    }

    BML_Result BML_API_HandleCreateOwned(BML_Mod owner, BML_HandleType type, BML_HandleDesc *out_desc) {
        auto &context = *Kernel().context;
        return BML_HandleCreateImpl(ResolveProviderId(context, owner), type, out_desc);
    }

    BML_Result BML_API_HandleRetain(const BML_HandleDesc *desc) {
        return BML_HandleRetainImpl(desc);
    }

    BML_Result BML_API_HandleRelease(const BML_HandleDesc *desc) {
        return BML_HandleReleaseImpl(Kernel().context->GetHandle(), desc);
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

    BML_Result BML_API_RegisterResourceType(const BML_ResourceTypeDesc *desc, BML_HandleType *out_type) {
        auto &context = *Kernel().context;
        return RegisterResourceTypeOwned(ResolveLegacyOwner(context), desc, out_type);
    }

    BML_Result BML_API_RegisterResourceTypeOwned(BML_Mod owner,
                                                 const BML_ResourceTypeDesc *desc,
                                                 BML_HandleType *out_type) {
        return RegisterResourceTypeOwned(owner, desc, out_type);
    }

    void RegisterResourceApis() {
        BML_BEGIN_API_REGISTRATION();

        // Handle management APIs
        BML_REGISTER_API_GUARDED(bmlHandleCreate, "resource", BML_API_HandleCreate);
        BML_REGISTER_API_GUARDED(bmlHandleCreateOwned, "resource", BML_API_HandleCreateOwned);
        BML_REGISTER_API_GUARDED(bmlHandleRetain, "resource", BML_API_HandleRetain);
        BML_REGISTER_API_GUARDED(bmlHandleRelease, "resource", BML_API_HandleRelease);
        BML_REGISTER_API_GUARDED(bmlHandleValidate, "resource", BML_API_HandleValidate);
        BML_REGISTER_API_GUARDED(bmlHandleAttachUserData, "resource", BML_API_HandleAttachUserData);
        BML_REGISTER_API_GUARDED(bmlHandleGetUserData, "resource", BML_API_HandleGetUserData);

        // Resource type registration
        BML_REGISTER_API_GUARDED(bmlRegisterResourceType, "resource", BML_API_RegisterResourceType);
        BML_REGISTER_API_GUARDED(
            bmlRegisterResourceTypeOwned, "resource", BML_API_RegisterResourceTypeOwned);
    }

    BML_Result RegisterResourceType(const BML_ResourceTypeDesc *desc, BML_HandleType *out_type) {
        auto &context = *Kernel().context;
        return RegisterResourceTypeOwned(ResolveLegacyOwner(context), desc, out_type);
    }

    BML_Result RegisterResourceTypeOwned(BML_Mod owner,
                                         const BML_ResourceTypeDesc *desc,
                                         BML_HandleType *out_type) {
        if (!desc || desc->struct_size < sizeof(BML_ResourceTypeDesc) || !out_type || !desc->name || desc->name[0] == '\0') {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto &context = *Kernel().context;
        const std::string provider_id = ResolveProviderId(context, owner);
        if (provider_id.empty()) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto type = g_NextResourceType.fetch_add(1, std::memory_order_relaxed);
        if (type == 0) {
            return BML_RESULT_FAIL;
        }

        ResourceTypeMetadata metadata;
        metadata.name = desc->name;
        metadata.finalize = desc->on_finalize;
        metadata.user_data = desc->user_data;
        metadata.provider_id = provider_id;

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
