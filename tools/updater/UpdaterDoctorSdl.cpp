#include "Platform/FileSystemNative.h"
#include "UpdaterDoctor.h"
#include "UpdaterPaths.h"
#include "UpdaterSdl.h"
#include "UpdaterTarget.h"
#include "PathUtils.h"
#include "StringUtils.h"
#if BML_RUNTIME_SDL
#include <SDL3/SDL.h>
#else
#include <Windows.h>
#include <cstdio>
#endif
#include <array>
#include <memory>

namespace bmlupdater {
namespace {
uint32_t Word(const unsigned char *p, bool big = false) {
    return big ? (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3] :
        uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
} // namespace
bool IsCompatibleBinary(const std::wstring &path, const UpdateTarget &target, bool executable) {
    if (target.platform == "windows")
        return BML::Platform::MatchesPeImage(path, target.architecture, executable);
    std::array<unsigned char, 4096> bytes{};
#if BML_RUNTIME_SDL
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(SDL_IOFromFile(PathUtf8(path).c_str(), "rb"), SDL_CloseIO);
    if (!file) return false;
    const auto size = SDL_ReadIO(file.get(), bytes.data(), bytes.size());
#else
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(_wfopen(path.c_str(), L"rb"), std::fclose);
    if (!file) return false;
    const auto size = std::fread(bytes.data(), 1, bytes.size(), file.get());
#endif
    if (size < 64) return false;
    if (target.platform == "linux") {
        const uint32_t machine = bytes[18] | (uint32_t(bytes[19]) << 8);
        const uint32_t expected = target.architecture == "arm64" ? 183 : target.architecture == "x64" ? 62 : 3;
        const bool hasEntryPoint = Word(bytes.data() + 24) != 0 || (bytes[4] == 2 && Word(bytes.data() + 28) != 0);
        const bool typeMatches = executable ? (bytes[16] == 2 || bytes[16] == 3) && hasEntryPoint : bytes[16] == 3 && !hasEntryPoint;
        return bytes[0] == 0x7f && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F' &&
            bytes[5] == 1 && bytes[4] == (target.architecture == "x86" ? 1 : 2) && machine == expected &&
            bytes[17] == 0 && typeMatches;
    }
    const auto expected = target.architecture == "arm64" ? 0x100000cu : 0x1000007u;
    const auto magic = Word(bytes.data(), true);
    if (magic == 0xcafebabe || magic == 0xcafebabf) {
        const auto count = Word(bytes.data() + 4, true);
        const size_t stride = magic == 0xcafebabf ? 32 : 20;
        if (count > (size - 8) / stride) return false;
        for (size_t i = 0; i < count; ++i) {
            const auto *entry = bytes.data() + 8 + i * stride;
            if (Word(entry, true) != expected) continue;
            const uint64_t offset = stride == 32 ? (uint64_t(Word(entry + 8, true)) << 32) | Word(entry + 12, true) : Word(entry + 8, true);
            std::array<unsigned char, 16> header{};
            if (offset > INT64_MAX) return false;
#if BML_RUNTIME_SDL
            if (SDL_SeekIO(file.get(), static_cast<int64_t>(offset), SDL_IO_SEEK_SET) < 0 ||
                SDL_ReadIO(file.get(), header.data(), header.size()) != header.size()) return false;
#else
            if (_fseeki64(file.get(), static_cast<int64_t>(offset), SEEK_SET) != 0 ||
                std::fread(header.data(), 1, header.size(), file.get()) != header.size()) return false;
#endif
            const auto sliceMagic = Word(header.data(), true);
            const bool big = sliceMagic == 0xfeedfacf;
            return (sliceMagic == 0xfeedfacf || sliceMagic == 0xcffaedfe) && Word(header.data() + 4, big) == expected &&
                Word(header.data() + 12, big) == (executable ? 2u : 6u);
        }
        return false;
    }
    return (magic == 0xcffaedfe || magic == 0xfeedfacf) && Word(bytes.data() + 4, magic == 0xfeedfacf) == expected &&
        Word(bytes.data() + 12, magic == 0xfeedfacf) == (executable ? 2u : 6u);
}
namespace {
bool CheckBinary(std::vector<std::string> &diagnostics, const char *name, const std::wstring &path, const UpdateTarget &target, bool executable = false) {
    const bool exists = utils::FileExistsW(path);
    const bool matches = exists && IsCompatibleBinary(path, target, executable);
    diagnostics.push_back(std::string("file.") + name + "=" + (!exists ? "missing" : matches ? "compatible" : "wrong-format-or-architecture"));
    return matches;
}
}

Result RunDoctorChecks(const UpdaterContext &context, std::vector<std::string> &diagnostics) {
    diagnostics.clear();
    diagnostics.push_back("gameRoot=" + PathUtf8(context.gameRoot));
    diagnostics.push_back("stateRoot=" + PathUtf8(context.updaterStateRoot));
    if (!DirectoryExists(context.gameRoot)) return Result::Failure("Game root does not exist");
    const auto paths = CheckUpdaterPaths(context);
    if (!paths.ok) return paths;
    UpdateTarget target;
    auto result = ReadRuntimeTarget(context, target);
    if (!result.ok) { diagnostics.push_back("runtime.abi=invalid"); return result; }
    diagnostics.push_back("runtime.target=" + target.runtime + "/" + target.platform + "/" + target.architecture + "/" + target.hostLinkage);
    bool healthy = true;
#if BML_RUNTIME_SDL
    const auto sdlVersion = SDL_GetVersion();
    diagnostics.push_back("runtime.sdlVersion=" + std::to_string(SDL_VERSIONNUM_MAJOR(sdlVersion)) + "." +
        std::to_string(SDL_VERSIONNUM_MINOR(sdlVersion)) + "." + std::to_string(SDL_VERSIONNUM_MICRO(sdlVersion)));
    healthy &= sdlVersion >= SDL_VERSIONNUM(3, 4, 8);
#endif
    const auto player = FindPlayer(context.gameRoot);
    healthy &= CheckBinary(diagnostics, "player", player, target, true);
    const bool executable = !player.empty() && IsExecutableFile(player);
    diagnostics.push_back(std::string("player.executable=") + (executable ? "true" : "false"));
    healthy &= executable;
    const auto bin = player.empty() ? JoinPath(context.gameRoot, L"Bin") : ParentPath(player);
    const std::wstring prefix = target.platform == "windows" ? L"" : L"lib";
    const std::wstring suffix = target.platform == "windows" ? L".dll" : target.platform == "macos" ? L".dylib" : L".so";
    std::string error;
    auto abi = utils::JsonDocument::ParseFile(JoinPath(context.gameRoot, L"ModLoader/BML.abi.json"), error);
    auto sdk = abi.IsValid() ? utils::JsonGetString(abi.Root(), "sdk_abi", "") : std::string();
    const bool validSdk = sdk.size() == 64 && HexToBytes(sdk).size() == 32;
    diagnostics.push_back(std::string("runtime.sdkAbi=") + (validSdk ? "present" : "invalid"));
    healthy &= validSdk;
    if (target.hostLinkage == "dynamic") {
        healthy &= CheckBinary(diagnostics, "ck2", JoinPath(bin, prefix + L"CK2" + suffix), target);
        healthy &= CheckBinary(diagnostics, "vxmath", JoinPath(bin, prefix + L"VxMath" + suffix), target);
        healthy &= CheckBinary(diagnostics, "render", JoinPath(context.gameRoot, L"RenderEngines/" + prefix + L"CK2_3D" + suffix), target);
        healthy &= CheckBinary(diagnostics, "bml", JoinPath(context.gameRoot, utils::Utf8ToUtf16(LoaderPath(target))), target);
        const bool scripts = abi.IsValid() && utils::JsonGetBool(abi.Root(), "scripts", true);
        diagnostics.push_back(std::string("runtime.scripts=") + (scripts ? "enabled" : "disabled"));
        if (scripts) healthy &= CheckBinary(diagnostics, "angelscript", JoinPath(context.gameRoot, L"BuildingBlocks/" + prefix + L"AngelScript" + suffix), target);
    } else diagnostics.push_back("runtime.plugins=embedded; registration-not-inspected");

    bool mixed = false;
    for (const auto *directory : {L"Bin", L"Managers", L"BuildingBlocks"}) {
        for (const auto &name : utils::ListFilesW(JoinPath(context.gameRoot, directory), L"*")) {
            const auto lower = ToLowerAscii(PathUtf8(name));
            if (lower.starts_with("bmlplus") || lower.starts_with("libbmlplus") || lower == "bml.dll") {
                if (target.hostLinkage == "static" || PathUtf8(std::wstring(directory) + L"/" + name) != LoaderPath(target)) mixed = true;
            }
        }
    }
    diagnostics.push_back(std::string("runtime.mixedInstall=") + (mixed ? "incompatible" : "none"));
    healthy &= !mixed;
    const auto stopped = CheckPlayerStopped();
    diagnostics.push_back(std::string("process.player=") + (stopped.ok ? "stopped" : stopped.message));
    healthy &= stopped.ok;
    const bool writable = CanCreateFileInDirectory(context.updaterStateRoot);
    diagnostics.push_back(std::string("write.stateRoot=") + (writable ? "ok" : "denied"));
    healthy &= writable;
    const bool pending = PathExists(JoinPath(context.updaterStateRoot, L"pending.json"));
    diagnostics.push_back(std::string("transaction.pending=") + (pending ? "true; run rollback" : "false"));
    healthy &= !pending;
#if BML_RUNTIME_SDL
    diagnostics.push_back("path.encoding=UTF-8");
#else
    const auto page = GetACP();
    std::string encoded;
    const auto shortPath = BML::Platform::ShortPath(context.gameRoot);
    const bool direct = BML::Platform::EncodePathForCodePage(context.gameRoot, page, encoded);
    const bool fallback = !shortPath.empty() && BML::Platform::EncodePathForCodePage(shortPath, page, encoded);
    diagnostics.push_back("path.acp=" + std::to_string(page));
    diagnostics.push_back(std::string("path.ansiCompatible=") + (direct ? "true" : "false"));
    diagnostics.push_back(std::string("path.shortFallback=") + (direct ? "not-needed" : fallback ? "available" : "unavailable"));
    healthy &= direct || fallback;
#endif
    return healthy ? Result::Success("doctor passed") : Result::Failure("doctor found incompatible or missing requirements");
}
}
