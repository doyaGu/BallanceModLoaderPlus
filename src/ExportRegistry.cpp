#include "ExportRegistry.h"

#include <algorithm>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/IMod.h"
#include "CallFrameInternal.h"
#include "InteropSignature.h"
#include "ModContext.h"

#if BML_ENABLE_ANGELSCRIPT
#include "ScriptExportDispatcher.h"
#include "ScriptMod.h"
#endif

BML_ModExport::BML_ModExport(BML_ExportKey key)
    : RefCount(1),
      Key(std::move(key)) {
}

namespace BML {

struct ExportKeyHash {
    size_t operator()(const BML_ExportKey &key) const {
        const std::hash<std::string> hash;
        size_t value = hash(key.ModId);
        value ^= hash(key.Name) + 0x9e3779b9u + (value << 6) + (value >> 2);
        value ^= hash(key.Signature) + 0x9e3779b9u + (value << 6) + (value >> 2);
        return value;
    }
};

struct ExportKeyEqual {
    bool operator()(const BML_ExportKey &left, const BML_ExportKey &right) const {
        return left.ModId == right.ModId &&
               left.Name == right.Name &&
               left.Signature == right.Signature;
    }
};

struct ExportNameKey {
    std::string ModId;
    std::string Name;
};

struct ExportNameKeyHash {
    size_t operator()(const ExportNameKey &key) const {
        const std::hash<std::string> hash;
        size_t value = hash(key.ModId);
        value ^= hash(key.Name) + 0x9e3779b9u + (value << 6) + (value >> 2);
        return value;
    }
};

struct ExportNameKeyEqual {
    bool operator()(const ExportNameKey &left, const ExportNameKey &right) const {
        return left.ModId == right.ModId && left.Name == right.Name;
    }
};

struct ExportListCache {
    uint64_t NativeGeneration = 0;
    uint64_t ScriptGeneration = 0;
    std::vector<ExportInfo> Exports;
};

static std::mutex g_ExportRegistryMutex;
static std::unordered_map<BML_ExportKey, BML_NativeExportEntry, ExportKeyHash, ExportKeyEqual> g_NativeExports;
static std::unordered_map<ExportNameKey, std::vector<std::string>, ExportNameKeyHash, ExportNameKeyEqual> g_NativeExportsByName;
static std::unordered_map<std::string, ExportListCache> g_ExportListCache;
static std::atomic<uint64_t> g_NativeGeneration(1);
static std::atomic<uint64_t> g_ScriptGeneration(1);

static bool IsValidKeyPart(const char *value) {
    return value && value[0] != '\0';
}

static BML_ExportKey MakeKey(const char *modId, const char *name, const char *signature) {
    BML_ExportKey key;
    key.ModId = modId ? modId : "";
    key.Name = name ? name : "";
    key.Signature = signature ? signature : "";
    return key;
}

static ExportNameKey MakeNameKey(const char *modId, const char *name) {
    ExportNameKey key;
    key.ModId = modId ? modId : "";
    key.Name = name ? name : "";
    return key;
}

static void AddNativeNameIndexLocked(const BML_ExportKey &key) {
    g_NativeExportsByName[MakeNameKey(key.ModId.c_str(), key.Name.c_str())].push_back(key.Signature);
}

static void RemoveNativeNameIndexLocked(const BML_ExportKey &key) {
    auto it = g_NativeExportsByName.find(MakeNameKey(key.ModId.c_str(), key.Name.c_str()));
    if (it == g_NativeExportsByName.end())
        return;

    auto &signatures = it->second;
    signatures.erase(std::remove(signatures.begin(), signatures.end(), key.Signature), signatures.end());
    if (signatures.empty())
        g_NativeExportsByName.erase(it);
}

static void SortAndUniqueExports(std::vector<ExportInfo> &exports) {
    std::sort(exports.begin(), exports.end(), [](const ExportInfo &left, const ExportInfo &right) {
        if (left.Name == right.Name)
            return left.Signature < right.Signature;
        return left.Name < right.Name;
    });
    exports.erase(std::unique(exports.begin(), exports.end(), [](const ExportInfo &left, const ExportInfo &right) {
        return left.Name == right.Name && left.Signature == right.Signature;
    }), exports.end());
}

static void AppendNativeExportsLocked(const char *modId, std::vector<ExportInfo> &exports) {
    for (const auto &entry : g_NativeExports) {
        if (entry.first.ModId == modId)
            exports.push_back(ExportInfo{entry.first.Name, entry.first.Signature});
    }
}

static const std::vector<std::string> *FindNativeSignaturesByNameLocked(const char *modId, const char *name) {
    auto it = g_NativeExportsByName.find(MakeNameKey(modId, name));
    return it == g_NativeExportsByName.end() ? nullptr : &it->second;
}

static IMod *FindTargetMod(const char *modId) {
    ModContext *context = BML_GetModContext();
    return context && IsValidKeyPart(modId) ? context->FindMod(modId) : nullptr;
}

static int GetTargetStatus(const char *modId, IMod **outMod = nullptr) {
    IMod *mod = FindTargetMod(modId);
    if (outMod)
        *outMod = mod;
    if (!mod)
        return BML_ERROR_INTEROP_TARGET_NOT_FOUND;
#if BML_ENABLE_ANGELSCRIPT
    if (auto *scriptMod = dynamic_cast<ScriptMod *>(mod)) {
        if (scriptMod->IsFailed())
            return BML_ERROR_INTEROP_TARGET_FAILED;
    }
#endif
    return BML_OK;
}

static int ResolveNativeLocked(BML_ModExport &handle) {
    auto it = g_NativeExports.find(handle.Key);
    if (it == g_NativeExports.end()) {
        if (handle.Key.Signature.empty())
            return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

        InteropSignatureInfo requestedSignature;
        if (InteropSignature::Validate(handle.Key.Signature, handle.Key.Name, &requestedSignature) != BML_OK)
            return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

        const std::vector<std::string> *candidateSignatures =
            FindNativeSignaturesByNameLocked(handle.Key.ModId.c_str(), handle.Key.Name.c_str());
        if (!candidateSignatures)
            return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

        BML_ExportKey matchedKey;
        const BML_NativeExportEntry *matchedEntry = nullptr;
        for (const std::string &candidateSignature : *candidateSignatures) {
            auto candidate = g_NativeExports.find(MakeKey(handle.Key.ModId.c_str(),
                                                          handle.Key.Name.c_str(),
                                                          candidateSignature.c_str()));
            if (candidate == g_NativeExports.end())
                continue;
            const auto &entry = *candidate;
            if (!InteropSignature::Equivalent(entry.second.SignatureInfo, requestedSignature))
                continue;
            if (matchedEntry)
                return BML_ERROR_INTEROP_EXPORT_AMBIGUOUS;
            matchedKey = entry.first;
            matchedEntry = &entry.second;
        }

        if (!matchedEntry)
            return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

        handle.Key.Signature = matchedKey.Signature;
        handle.Kind = BML_ResolvedExportKind::Native;
        handle.Native = *matchedEntry;
        handle.NativeGeneration = g_NativeGeneration.load(std::memory_order_acquire);
        return BML_OK;
    }

    handle.Kind = BML_ResolvedExportKind::Native;
    handle.Native = it->second;
    handle.NativeGeneration = g_NativeGeneration.load(std::memory_order_acquire);
    return BML_OK;
}

#if BML_ENABLE_ANGELSCRIPT
static ScriptMod *FindScriptMod(const std::string &modId) {
    ModContext *context = BML_GetModContext();
    return context ? dynamic_cast<ScriptMod *>(context->FindMod(modId.c_str())) : nullptr;
}

static int ResolveScript(BML_ModExport &handle) {
    ScriptMod *scriptMod = FindScriptMod(handle.Key.ModId);
    if (!scriptMod)
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
    if (scriptMod->IsFailed())
        return BML_ERROR_INTEROP_TARGET_FAILED;

    const ScriptExportBinding *binding = scriptMod->ResolveExport(handle.Key.Name, handle.Key.Signature);
    if (!binding)
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

    handle.Kind = BML_ResolvedExportKind::Script;
    handle.Script = scriptMod;
    handle.ScriptBinding = binding;
    handle.ScriptGeneration = g_ScriptGeneration.load(std::memory_order_acquire);
    return BML_OK;
}

static void AppendScriptExports(const char *modId, std::vector<ExportInfo> &exports) {
    ScriptMod *scriptMod = FindScriptMod(modId ? modId : "");
    if (!scriptMod)
        return;

    const int count = scriptMod->GetExportCount();
    for (int i = 0; i < count; ++i) {
        std::string name;
        std::string signature;
        if (scriptMod->GetExportInfo(i, name, signature))
            exports.push_back(ExportInfo{std::move(name), std::move(signature)});
    }
}

static void AppendScriptExportsForName(const char *modId, const char *name, std::vector<ExportInfo> &exports) {
    ScriptMod *scriptMod = FindScriptMod(modId ? modId : "");
    if (!scriptMod)
        return;

    std::vector<std::string> signatures;
    scriptMod->GetExportSignatures(name ? name : "", signatures);
    exports.reserve(exports.size() + signatures.size());
    for (std::string &signature : signatures)
        exports.push_back(ExportInfo{name ? name : "", std::move(signature)});
}
#else
static int ResolveScript(BML_ModExport &) {
    return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
}

static void AppendScriptExports(const char *, std::vector<ExportInfo> &) {
}

static void AppendScriptExportsForName(const char *, const char *, std::vector<ExportInfo> &) {
}
#endif

static int ResolveUniqueKey(const char *modId, const char *name, BML_ExportKey &key) {
    std::vector<ExportInfo> exports;
    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        const std::vector<std::string> *nativeSignatures = FindNativeSignaturesByNameLocked(modId, name);
        if (nativeSignatures) {
            exports.reserve(exports.size() + nativeSignatures->size());
            for (const std::string &signature : *nativeSignatures)
                exports.push_back(ExportInfo{name, signature});
        }
    }
    AppendScriptExportsForName(modId, name, exports);
    SortAndUniqueExports(exports);

    const ExportInfo *match = nullptr;
    for (const ExportInfo &exportInfo : exports) {
        if (exportInfo.Name != name)
            continue;
        if (match)
            return BML_ERROR_INTEROP_EXPORT_AMBIGUOUS;
        match = &exportInfo;
    }

    if (!match)
        return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;

    key = MakeKey(modId, name, match->Signature.c_str());
    return BML_OK;
}

static int Resolve(BML_ModExport &handle) {
    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        const int nativeStatus = ResolveNativeLocked(handle);
        if (nativeStatus == BML_OK)
            return BML_OK;
    }

    const int scriptStatus = ResolveScript(handle);
    if (scriptStatus == BML_OK)
        return BML_OK;
    return scriptStatus == BML_ERROR_INTEROP_TARGET_FAILED
               ? scriptStatus
               : BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
}

static int ResolveExplicitKey(BML_ModExport &handle) {
    const int status = Resolve(handle);
    if (status == BML_OK)
        return BML_OK;
    if (status == BML_ERROR_INTEROP_TARGET_FAILED)
        return status;

    std::vector<ExportInfo> exports;
    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        const std::vector<std::string> *nativeSignatures =
            FindNativeSignaturesByNameLocked(handle.Key.ModId.c_str(), handle.Key.Name.c_str());
        if (nativeSignatures) {
            exports.reserve(exports.size() + nativeSignatures->size());
            for (const std::string &signature : *nativeSignatures)
                exports.push_back(ExportInfo{handle.Key.Name, signature});
        }
    }
    AppendScriptExportsForName(handle.Key.ModId.c_str(), handle.Key.Name.c_str(), exports);
    for (const ExportInfo &exportInfo : exports) {
        if (exportInfo.Name == handle.Key.Name)
            return BML_ERROR_INTEROP_SIGNATURE_MISMATCH;
    }
    return BML_ERROR_INTEROP_EXPORT_NOT_FOUND;
}

static int CallNativeExport(const BML_NativeExportEntry &entry, BML_CallFrame *frame) {
    if (!entry.Callback)
        return BML_ERROR_INTEROP_HANDLE_STALE;

    try {
        return entry.Callback(frame, entry.UserData);
    } catch (...) {
        return BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED;
    }
}

int ExportRegistry::RegisterNative(const char *modId,
                                   const char *name,
                                   const char *signature,
                                   BML_NativeExportCallback callback,
                                   void *userData) {
    if (!IsValidKeyPart(modId) || !IsValidKeyPart(name) || !IsValidKeyPart(signature) || !callback)
        return BML_ERROR_INVALID_PARAMETER;
    InteropSignatureInfo signatureInfo;
    const int signatureStatus = InteropSignature::Validate(signature, name, &signatureInfo);
    if (signatureStatus != BML_OK)
        return signatureStatus;

    std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
    if (const std::vector<std::string> *candidateSignatures = FindNativeSignaturesByNameLocked(modId, name)) {
        for (const std::string &candidateSignature : *candidateSignatures) {
            auto candidate = g_NativeExports.find(MakeKey(modId, name, candidateSignature.c_str()));
            if (candidate != g_NativeExports.end() &&
                InteropSignature::Equivalent(candidate->second.SignatureInfo, signatureInfo)) {
                return BML_ERROR_ALREADY_EXISTS;
            }
        }
    }

    BML_ExportKey key = MakeKey(modId, name, signature);
    auto inserted = g_NativeExports.emplace(key,
                                            BML_NativeExportEntry{callback, userData, signatureInfo});
    if (!inserted.second)
        return BML_ERROR_ALREADY_EXISTS;
    AddNativeNameIndexLocked(key);

    g_NativeGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_ExportListCache.erase(modId);
    return BML_OK;
}

int ExportRegistry::UnregisterNative(const char *modId, const char *name, const char *signature) {
    if (!IsValidKeyPart(modId) || !IsValidKeyPart(name) || !IsValidKeyPart(signature))
        return BML_ERROR_INVALID_PARAMETER;

    std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
    auto key = MakeKey(modId, name, signature);
    auto it = g_NativeExports.find(key);
    if (it == g_NativeExports.end()) {
        InteropSignatureInfo requestedSignature;
        if (InteropSignature::Validate(signature, name, &requestedSignature) == BML_OK) {
            if (const std::vector<std::string> *candidateSignatures =
                    FindNativeSignaturesByNameLocked(modId, name)) {
                for (const std::string &candidateSignature : *candidateSignatures) {
                    auto candidate = g_NativeExports.find(MakeKey(modId, name, candidateSignature.c_str()));
                    if (candidate != g_NativeExports.end() &&
                        InteropSignature::Equivalent(candidate->second.SignatureInfo, requestedSignature)) {
                        it = candidate;
                        break;
                    }
                }
            }
        }
    }
    if (it == g_NativeExports.end())
        return BML_ERROR_NOT_FOUND;
    const BML_ExportKey removedKey = it->first;
    g_NativeExports.erase(it);
    RemoveNativeNameIndexLocked(removedKey);

    g_NativeGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_ExportListCache.erase(modId);
    return BML_OK;
}

int ExportRegistry::UnregisterNativeForMod(const char *modId) {
    if (!IsValidKeyPart(modId))
        return BML_ERROR_INVALID_PARAMETER;

    int removed = 0;
    std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
    for (auto it = g_NativeExports.begin(); it != g_NativeExports.end();) {
        if (it->first.ModId == modId) {
            RemoveNativeNameIndexLocked(it->first);
            it = g_NativeExports.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        g_NativeGeneration.fetch_add(1, std::memory_order_acq_rel);
        g_ExportListCache.erase(modId);
    }
    return removed;
}

BML_ModExport *ExportRegistry::Find(const char *modId, const char *name, const char *signature) {
    BML_ModExport *handle = nullptr;
    return FindEx(modId, name, signature, &handle) == BML_OK ? handle : nullptr;
}

int ExportRegistry::FindEx(const char *modId, const char *name, const char *signature, BML_ModExport **outHandle) {
    if (outHandle)
        *outHandle = nullptr;
    if (!outHandle)
        return BML_ERROR_INVALID_PARAMETER;
    if (!IsValidKeyPart(modId) || !IsValidKeyPart(name))
        return BML_ERROR_INVALID_PARAMETER;

    const int targetStatus = GetTargetStatus(modId);
    if (targetStatus != BML_OK)
        return targetStatus;

    if (IsValidKeyPart(signature)) {
        const int signatureStatus = InteropSignature::Validate(signature, name);
        if (signatureStatus != BML_OK)
            return signatureStatus;
    }

    BML_ExportKey key = MakeKey(modId, name, signature);
    if (key.Signature.empty()) {
        const int uniqueStatus = ResolveUniqueKey(modId, name, key);
        if (uniqueStatus != BML_OK)
            return uniqueStatus;
    }

    BML_ModExport *handle = new (std::nothrow) BML_ModExport(std::move(key));
    if (!handle)
        return BML_ERROR_OUT_OF_MEMORY;

    const int resolveStatus = ResolveExplicitKey(*handle);
    if (resolveStatus != BML_OK) {
        delete handle;
        return resolveStatus;
    }

    *outHandle = handle;
    return BML_OK;
}

int ExportRegistry::Call(BML_ModExport *handle, BML_CallFrame *frame) {
    if (!handle)
        return BML_ERROR_INVALID_PARAMETER;
    if (!frame)
        return BML_ERROR_INTEROP_BAD_CALL_FRAME;

    BML_ResetCallValue(frame->Result);

    if (handle->Kind == BML_ResolvedExportKind::Native &&
        handle->NativeGeneration == g_NativeGeneration.load(std::memory_order_acquire) &&
        handle->Native.Callback) {
        const int status = CallNativeExport(handle->Native, frame);
        if (status != BML_OK)
            BML_ResetCallValue(frame->Result);
        return status;
    }

#if BML_ENABLE_ANGELSCRIPT
    if (handle->Kind == BML_ResolvedExportKind::Script &&
        handle->ScriptGeneration == g_ScriptGeneration.load(std::memory_order_acquire) &&
        handle->Script) {
        const int status = handle->Script->CallExport(handle->Key.Name, handle->Key.Signature, frame);
        if (status != BML_OK)
            BML_ResetCallValue(frame->Result);
        return status;
    }
#endif

    const int resolveStatus = ResolveExplicitKey(*handle);
    if (resolveStatus != BML_OK)
        return resolveStatus == BML_ERROR_INTEROP_TARGET_FAILED
                   ? resolveStatus
                   : BML_ERROR_INTEROP_HANDLE_STALE;

    if (handle->Kind == BML_ResolvedExportKind::Native && handle->Native.Callback) {
        const int status = CallNativeExport(handle->Native, frame);
        if (status != BML_OK)
            BML_ResetCallValue(frame->Result);
        return status;
    }

#if BML_ENABLE_ANGELSCRIPT
    if (handle->Kind == BML_ResolvedExportKind::Script && handle->Script) {
        const int status = handle->Script->CallExport(handle->Key.Name, handle->Key.Signature, frame);
        if (status != BML_OK)
            BML_ResetCallValue(frame->Result);
        return status;
    }
#endif

    return BML_ERROR_INTEROP_HANDLE_STALE;
}

bool ExportRegistry::IsValid(BML_ModExport *handle) {
    return handle && ResolveExplicitKey(*handle) == BML_OK;
}

int ExportRegistry::GetCount(const char *modId) {
    if (!IsValidKeyPart(modId))
        return BML_ERROR_INVALID_PARAMETER;
    const int targetStatus = GetTargetStatus(modId);
    if (targetStatus != BML_OK)
        return targetStatus;

    ExportInfo ignored;
    const int status = GetInfo(modId, 0, ignored);
    if (status == BML_ERROR_NOT_FOUND) {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        auto it = g_ExportListCache.find(modId);
        return it == g_ExportListCache.end() ? 0 : static_cast<int>(it->second.Exports.size());
    }
    if (status < 0)
        return status;

    std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
    auto it = g_ExportListCache.find(modId);
    return it == g_ExportListCache.end() ? 0 : static_cast<int>(it->second.Exports.size());
}

int ExportRegistry::GetInfo(const char *modId, size_t index, ExportInfo &info) {
    if (!IsValidKeyPart(modId))
        return BML_ERROR_INVALID_PARAMETER;
    const int targetStatus = GetTargetStatus(modId);
    if (targetStatus != BML_OK)
        return targetStatus;

    const uint64_t scriptGeneration = g_ScriptGeneration.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        auto it = g_ExportListCache.find(modId);
        if (it != g_ExportListCache.end() &&
            it->second.NativeGeneration == g_NativeGeneration.load(std::memory_order_acquire) &&
            it->second.ScriptGeneration == scriptGeneration) {
            if (index >= it->second.Exports.size())
                return BML_ERROR_NOT_FOUND;
            info = it->second.Exports[index];
            return BML_OK;
        }
    }

    uint64_t nativeGeneration = 0;
    std::vector<ExportInfo> exports;
    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        nativeGeneration = g_NativeGeneration.load(std::memory_order_acquire);
        AppendNativeExportsLocked(modId, exports);
    }
    AppendScriptExports(modId, exports);
    SortAndUniqueExports(exports);

    {
        std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
        ExportListCache &cache = g_ExportListCache[modId];
        cache.NativeGeneration = nativeGeneration;
        cache.ScriptGeneration = scriptGeneration;
        cache.Exports = std::move(exports);
        if (index >= cache.Exports.size())
            return BML_ERROR_NOT_FOUND;
        info = cache.Exports[index];
        return BML_OK;
    }
}

void ExportRegistry::NotifyScriptExportsChanged() {
    g_ScriptGeneration.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(g_ExportRegistryMutex);
    g_ExportListCache.clear();
}

uint64_t ExportRegistry::GetScriptGeneration() {
    return g_ScriptGeneration.load(std::memory_order_acquire);
}

} // namespace BML
