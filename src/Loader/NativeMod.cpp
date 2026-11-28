#include "Loader/NativeMod.h"
#include "Loader/ModDefinitionValidation.h"
#include <limits>
#include <cstring>
#include <unordered_set>

std::unique_ptr<NativeMod> NativeMod::Query(ModContext *host, const BML_ModInterface *api,
                                            std::string &diagnostic) {
    const auto reject = [&](const char *reason) -> std::unique_ptr<NativeMod> {
        diagnostic = reason;
        return {};
    };
    if (!api || api->Header.StructSize < BML_MOD_INTERFACE_1_0_SIZE ||
        api->Header.MajorVersion != 1 || !api->Header.InterfaceId ||
        std::strcmp(api->Header.InterfaceId, BML_MOD_INTERFACE_ID) || !api->Create || !api->Destroy)
        return reject("invalid bml.mod 1.0 table (header, Create or Destroy)");
    const auto *def = api->Definition;
    if (!def || def->StructSize < BML_MOD_DEFINITION_1_0_SIZE)
        return reject("short or missing Definition");
    if (!def->Id || !BML::IsModId(def->Id))
        return reject("Definition.Id must contain only letters, digits, '.', '_' or '-'");
    if (!def->Name || !*def->Name)
        return reject("Definition.Name is required");
    BMLVersion parsed{0, 0, 0};
    if (!def->Version || !BML::ParseModVersion(def->Version, parsed))
        return reject("Definition.Version must be MAJOR.MINOR.PATCH");
    auto result = std::unique_ptr<NativeMod>(new NativeMod(host));
    result->m_Id = def->Id;
    result->m_Name = def->Name;
    result->m_Version = def->Version;
    result->m_Author = def->Author ? def->Author : "";
    result->m_Description = def->Description ? def->Description : "";
    result->m_MinimumText = def->MinimumLoaderVersion ? def->MinimumLoaderVersion : "";
    if (!result->m_MinimumText.empty() &&
        !BML::ParseModVersion(result->m_MinimumText, result->m_MinimumVersion))
        return reject("Definition.MinimumLoaderVersion must be MAJOR.MINOR.PATCH");
    if (def->DependencyCount &&
        (!def->Dependencies || def->DependencyStride < BML_MOD_DEPENDENCY_1_0_SIZE ||
         def->DependencyStride % alignof(BML_ModDependency) != 0 ||
         def->DependencyCount > std::numeric_limits<size_t>::max() / def->DependencyStride))
        return reject("invalid dependency array or stride");
    result->m_Dependencies.reserve(def->DependencyCount);
    result->m_DependencyVersions.reserve(def->DependencyCount);
    result->m_DependencyViews.reserve(def->DependencyCount);
    std::unordered_set<std::string> ids;
    for (size_t i = 0; i != def->DependencyCount; ++i) {
        const auto *dep = reinterpret_cast<const BML_ModDependency *>(
            reinterpret_cast<const unsigned char *>(def->Dependencies) + i * def->DependencyStride);
        if (dep->StructSize < BML_MOD_DEPENDENCY_1_0_SIZE ||
            dep->StructSize > def->DependencyStride || !dep->Id || !BML::IsModId(dep->Id) ||
            dep->Optional > 1 || dep->Id == result->m_Id || !ids.insert(dep->Id).second)
            return reject("invalid, duplicate or self dependency");
        const std::string minimum = dep->MinimumVersion ? dep->MinimumVersion : "";
        BMLVersion version{0, 0, 0};
        if (!minimum.empty() && !BML::ParseModVersion(minimum, version))
            return reject("dependency MinimumVersion must be MAJOR.MINOR.PATCH");
        result->m_Dependencies.emplace_back(dep->Id, version, dep->Optional != 0);
        result->m_DependencyVersions.push_back(minimum);
    }
    for (size_t i = 0; i != result->m_Dependencies.size(); ++i) {
        const auto &dep = result->m_Dependencies[i];
        result->m_DependencyViews.push_back({sizeof(BML_ModDependency), dep.Id.c_str(),
                                             result->m_DependencyVersions[i].c_str(),
                                             static_cast<uint32_t>(dep.Optional)});
    }
    result->m_Definition = {sizeof(BML_ModDefinition),        result->m_Id.c_str(),
                            result->m_Name.c_str(),           result->m_Version.c_str(),
                            result->m_Author.c_str(),         result->m_Description.c_str(),
                            result->m_MinimumText.c_str(),    result->m_DependencyViews.data(),
                            result->m_DependencyViews.size(), sizeof(BML_ModDependency)};
    result->m_Callbacks.StructSize = sizeof(BML_ModCallbacks);
    if (const auto *callbacks = api->Callbacks) {
        if (callbacks->StructSize < sizeof(size_t))
            return reject("short Callbacks header");
#define COPY_CALLBACK(member)                                                                      \
    if (callbacks->StructSize >= offsetof(BML_ModCallbacks, member) + sizeof(callbacks->member))   \
    result->m_Callbacks.member = callbacks->member
        COPY_CALLBACK(OnLoad);
        COPY_CALLBACK(OnUnload);
        COPY_CALLBACK(OnProcess);
        COPY_CALLBACK(OnRender);
        COPY_CALLBACK(OnGameEvent);
        COPY_CALLBACK(OnCheatEnabled);
        COPY_CALLBACK(OnLoadObject);
        COPY_CALLBACK(OnLoadScript);
        COPY_CALLBACK(OnCommandEvent);
        COPY_CALLBACK(OnModifyConfig);
        COPY_CALLBACK(OnPhysicalize);
        COPY_CALLBACK(OnUnphysicalize);
#undef COPY_CALLBACK
    }
    result->m_Create = api->Create;
    result->m_Destroy = api->Destroy;
    result->m_Record.RefreshMetadata(*result);
    return result;
}

int NativeMod::Create(BML_ModContextHandle context) {
    if (m_Instance)
        return BML_ERROR_ALREADY_EXISTS;
    m_Context = context;
    void *instance = nullptr;
    const int code = m_Create(context, &instance);
    if (code != BML_OK || !instance) {
        Fail(code < 0 ? code : BML_ERROR_FAIL, "Create");
        return code < 0 ? code : BML_ERROR_FAIL;
    }
    m_Instance = instance;
    return BML_OK;
}

void NativeMod::Destroy() noexcept {
    if (m_Instance) {
        m_Destroy(m_Instance);
        m_Instance = nullptr;
    }
    m_Context = 0;
}

void NativeMod::Fail(int code, const char *operation) {
    m_State.Fail();
    m_Diagnostic = std::string(operation) + " returned " + std::to_string(code);
}

int NativeMod::Load() {
    m_Loading = true;
    int code = BML_ERROR_FAIL;
    try {
        code = m_Callbacks.OnLoad ? m_Callbacks.OnLoad(m_Instance, m_Context) : BML_OK;
    } catch (...) {
        m_Loading = false;
        Fail(BML_ERROR_FAIL, "OnLoad exception");
        throw;
    }
    m_Loading = false;
    if (code != BML_OK) {
        Fail(code, "OnLoad");
        return code < 0 ? code : BML_ERROR_FAIL;
    }
    if (m_State.IsFailed())
        return BML_ERROR_MOD_INITIALIZATION;
    m_State.MarkLoaded(true);
    return BML_OK;
}

int NativeMod::Unload() {
    if (!m_Instance || m_Unloaded)
        return BML_OK;
    m_Unloading = true;
    int code = m_Callbacks.OnUnload ? m_Callbacks.OnUnload(m_Instance, m_Context) : BML_OK;
    if (code != BML_OK) {
        Fail(code, "OnUnload");
        return code < 0 ? code : BML_ERROR_FAIL;
    }
    m_Unloaded = true;
    return BML_OK;
}
