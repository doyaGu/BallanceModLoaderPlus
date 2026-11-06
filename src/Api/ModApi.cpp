#include "Api/ModApi.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "Platform/FileError.h"
#include "Platform/FileSystemNative.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptMod.h"
#endif
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <climits>

namespace BML::Api {
namespace {
int BML_CDECL GetOwner(BML_ModContextHandle handle, BML_ModHandle *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    return WithModContext(handle, [&](ModContext &ctx, ModRecord &owner) {
        *out = ctx.GetModLoader().GetModGeneration(&owner);
        return *out ? BML_OK : BML_ERROR_INVALID_HANDLE;
    });
}

int VisitOne(ModContext &ctx, ModRecord &mod, BML_ModVisitor visitor, void *data) {
    BML_ModInfo info{};
    info.StructSize = sizeof(info);
    info.Handle = ctx.GetModLoader().GetModGeneration(&mod);
    info.Kind = BML_MOD_KIND_NATIVE;
    info.State = ctx.AreModsInited() ? BML_MOD_STATE_LOADED : BML_MOD_STATE_REGISTERED;
    if (auto *native = ctx.GetModLoader().FindNativeMod(&mod)) {
        info.State = native->IsFailed()   ? BML_MOD_STATE_FAILED
                     : native->IsLoaded() ? BML_MOD_STATE_LOADED
                                          : BML_MOD_STATE_REGISTERED;
        const std::string diagnostic = native->Diagnostic();
        info.Definition = &native->Definition();
        info.Diagnostic = diagnostic.c_str();
        return visitor(data, &info);
    }
    std::string diagnostic;
#if BML_ENABLE_ANGELSCRIPT
    if (auto *script = mod.Script) {
        info.Kind = BML_MOD_KIND_SCRIPT;
        info.State = script->IsFailed()   ? BML_MOD_STATE_FAILED
                     : script->IsLoaded() ? BML_MOD_STATE_LOADED
                                          : BML_MOD_STATE_REGISTERED;
        diagnostic = script->GetLastDiagnostic();
        info.Diagnostic = diagnostic.c_str();
    }
#endif
    const std::string id = mod.GetID(), name = mod.GetName(), version = mod.GetVersion(),
                      author = mod.GetAuthor(), description = mod.GetDescription();
    const std::string minimum = mod.GetBMLVersion().ToString();
    const auto source = ctx.GetModLoader().ReadDependencies(&mod);
    std::vector<std::string> ids, versions;
    std::vector<uint32_t> optional;
    ids.reserve(source.size());
    versions.reserve(source.size());
    optional.reserve(source.size());
    for (const auto &d : source) {
        ids.push_back(d.Id);
        versions.push_back(d.MinVersion.ToString());
        optional.push_back(d.Optional ? 1u : 0u);
    }
    std::vector<BML_ModDependency> dependencies;
    dependencies.reserve(ids.size());
    for (size_t i = 0; i < ids.size(); ++i)
        dependencies.push_back(
            {sizeof(BML_ModDependency), ids[i].c_str(), versions[i].c_str(), optional[i]});
    const BML_ModDefinition definition{
        sizeof(BML_ModDefinition), id.c_str(),          name.c_str(),
        version.c_str(),           author.c_str(),      description.c_str(),
        minimum.c_str(),           dependencies.data(), dependencies.size(),
        sizeof(BML_ModDependency)};
    info.Definition = &definition;
    return visitor(data, &info);
}

int BML_CDECL ReadMod(BML_ModContextHandle contextHandle, BML_ModHandle mod, BML_ModVisitor visitor,
                      void *data) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &) {
        auto *found = ctx.GetModLoader().ResolveModHandle(mod);
        return found ? VisitOne(ctx, *found, visitor, data) : BML_ERROR_INVALID_HANDLE;
    });
}

int BML_CDECL FindMod(BML_ModContextHandle contextHandle, const char *id, BML_ModHandle *out) {
    if (!id || !*id || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &) {
        auto *found = ctx.GetModLoader().FindMod(id);
        if (!found)
            return BML_ERROR_NOT_FOUND;
        *out = ctx.GetModLoader().GetModGeneration(found);
        return BML_OK;
    });
}

int BML_CDECL VisitMods(BML_ModContextHandle contextHandle, BML_ModVisitor visitor, void *data) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &) {
        const int count = ctx.GetModLoader().GetModCount();
        for (int i = 0; i < count; ++i) {
            const int code = VisitOne(ctx, *ctx.GetModLoader().GetMod(i), visitor, data);
            if (code != BML_OK)
                return code;
        }
        return BML_OK;
    });
}

int BML_CDECL Log(BML_ModContextHandle contextHandle, uint32_t level, const char *message,
                  size_t length) {
    if ((!message && length) || level > BML_LOG_ERROR || length > static_cast<size_t>(INT_MAX) ||
        (length && std::memchr(message, 0, length)))
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &owner) {
        auto *logger = ctx.GetLogger();
        if (!logger)
            return BML_ERROR_UNAVAILABLE;
        const char *text = message ? message : "";
        if (level == BML_LOG_INFO)
            logger->Info("[%s] %.*s", owner.GetID(), static_cast<int>(length), text);
        else if (level == BML_LOG_WARN)
            logger->Warn("[%s] %.*s", owner.GetID(), static_cast<int>(length), text);
        else
            logger->Error("[%s] %.*s", owner.GetID(), static_cast<int>(length), text);
        return BML_OK;
    });
}

int BML_CDECL GetDirectory(BML_ModContextHandle contextHandle, uint32_t directory, char *buffer,
                           size_t capacity, size_t *required) {
    if (directory > BML_MOD_DIRECTORY_ROOT)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &owner) {
        const std::string value =
            directory == BML_MOD_DIRECTORY_ROOT
                ? utils::Utf16ToUtf8(ctx.GetModLoader().GetModRootDirectory(owner.GetID()))
                : ctx.GetDirectoryUtf8(static_cast<BML_LoaderDirectory>(directory));
        return CopyText(value, buffer, capacity, required);
    });
}

int ResolveResource(ModContext &ctx, ModRecord &owner, const char *relative, std::string &path) {
    if (!relative || !*relative || !utils::IsRelativePathUtf8(relative))
        return BML_ERROR_INVALID_PARAMETER;
    const auto root = ctx.GetModLoader().GetModRootDirectory(owner.GetID());
    if (root.empty())
        return BML_ERROR_UNAVAILABLE;
    const auto candidate = utils::CombinePathW(root, utils::Utf8ToUtf16(relative));
    if (!utils::IsPathInsideRootW(candidate, root))
        return BML_ERROR_ACCESS_DENIED;
    path = utils::Utf16ToUtf8(utils::ResolvePathW(candidate));
    return path.empty() ? BML_ERROR_UNAVAILABLE : BML_OK;
}

int FileFailure() {
    const auto error = Platform::LastFileError();
    if (error == Platform::FileError::NotFound)
        return BML_ERROR_NOT_FOUND;
    if (error == Platform::FileError::AccessDenied)
        return BML_ERROR_ACCESS_DENIED;
    return BML_ERROR_FAIL;
}

int BML_CDECL GetResourcePath(BML_ModContextHandle contextHandle, const char *relative,
                              char *buffer, size_t capacity, size_t *required) {
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &owner) {
        std::string path;
        const int code = ResolveResource(ctx, owner, relative, path);
        if (code != BML_OK)
            return code;
        SDL_PathInfo info{};
        if (!SDL_GetPathInfo(path.c_str(), &info))
            return FileFailure();
        if (info.type != SDL_PATHTYPE_FILE)
            return BML_ERROR_NOT_FOUND;
        return CopyText(path, buffer, capacity, required);
    });
}

int BML_CDECL ResourceExists(BML_ModContextHandle contextHandle, const char *relative,
                             uint32_t *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &owner) {
        std::string path;
        const int code = ResolveResource(ctx, owner, relative, path);
        if (code != BML_OK)
            return code;
        SDL_PathInfo info{};
        if (!SDL_GetPathInfo(path.c_str(), &info)) {
            const int error = FileFailure();
            return error == BML_ERROR_NOT_FOUND ? BML_OK : error;
        }
        *out = info.type == SDL_PATHTYPE_FILE ? 1u : 0u;
        return BML_OK;
    });
}

int BML_CDECL ReadResource(BML_ModContextHandle contextHandle, const char *relative, void *buffer,
                           size_t capacity, size_t *required) {
    if (!required || (!buffer && capacity))
        return BML_ERROR_INVALID_PARAMETER;
    *required = 0;
    return WithModContext(contextHandle, [&](ModContext &ctx, ModRecord &owner) {
        std::string path;
        const int code = ResolveResource(ctx, owner, relative, path);
        if (code != BML_OK)
            return code;
        std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(
            SDL_IOFromFile(path.c_str(), "rb"), SDL_CloseIO);
        if (!stream)
            return FileFailure();
        const Sint64 size = SDL_GetIOSize(stream.get());
        if (size < 0 || static_cast<uint64_t>(size) > SIZE_MAX)
            return BML_ERROR_FAIL;
        *required = static_cast<size_t>(size);
        if (capacity < *required)
            return BML_ERROR_BUFFER_TOO_SMALL;
        if (!size)
            return BML_OK;
        return SDL_ReadIO(stream.get(), buffer, *required) == *required ? BML_OK : BML_ERROR_FAIL;
    });
}

const BML_ModContextInterface kInterface{
    BML_IFACE_HEADER(BML_ModContextInterface, BML_MOD_CONTEXT_INTERFACE_ID, 1, 0),
    GetOwner,
    ReadMod,
    FindMod,
    VisitMods,
    Log,
    GetDirectory,
    GetResourcePath,
    ReadResource,
    ResourceExists};
} // namespace

const BML_ModContextInterface &ModContextInterface() {
    return kInterface;
}
} // namespace BML::Api
