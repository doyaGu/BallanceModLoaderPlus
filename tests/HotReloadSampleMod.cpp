#include <filesystem>
#include <fstream>
#include <string>

#include <Windows.h>

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include "bml_logging.h"

namespace {

std::filesystem::path GetLogPath() {
    DWORD required = GetEnvironmentVariableW(L"BML_TEST_HOT_RELOAD_LOG", nullptr, 0);
    if (required == 0)
        return {};

    std::wstring buffer;
    buffer.resize(required);
    DWORD copied = GetEnvironmentVariableW(L"BML_TEST_HOT_RELOAD_LOG", buffer.data(), required);
    if (copied == 0)
        return {};
    if (copied < buffer.size())
        buffer.resize(copied);
    if (!buffer.empty() && buffer.back() == L'\0')
        buffer.pop_back();
    if (buffer.empty())
        return {};
    return std::filesystem::path(buffer);
}

int CountOccurrences(const std::filesystem::path &path, const std::string &prefix) {
    std::ifstream file(path);
    if (!file.is_open())
        return 0;

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        if (line.rfind(prefix, 0) == 0)
            ++count;
    }
    return count;
}

void AppendLine(const std::filesystem::path &path, const std::string &line) {
    std::ofstream file(path, std::ios::app);
    if (!file.is_open())
        return;
    file << line << '\n';
}

void LogLifecycleEvent(const std::filesystem::path &path, const char *prefix) {
    if (path.empty())
        return;
    const int next = CountOccurrences(path, prefix) + 1;
    AppendLine(path, std::string(prefix) + std::to_string(next));
}

struct SampleState {
    BML_Mod mod{nullptr};
    std::filesystem::path log_path;
} g_State;

BML_Result HandleAttach(const BML_ModAttachArgs *args) {
    if (!args || args->struct_size < sizeof(BML_ModAttachArgs))
        return BML_RESULT_INVALID_ARGUMENT;

    g_State.mod = args->mod;
    g_State.log_path = GetLogPath();

    BML_Result res = bmlLoadAPI(args->get_proc);
    if (res != BML_RESULT_OK)
        return res;

    LogLifecycleEvent(g_State.log_path, "init:");
    if (bmlLog) {
        bmlLog(nullptr, BML_LOG_INFO, "HotReloadSample", "Sample mod initialized");
    }
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs *args) {
    if (!args || args->struct_size < sizeof(BML_ModDetachArgs))
        return BML_RESULT_INVALID_ARGUMENT;

    LogLifecycleEvent(g_State.log_path, "shutdown:");
    bmlUnloadAPI();
    g_State = {};
    return BML_RESULT_OK;
}

} // namespace

extern "C" __declspec(dllexport) BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand command, void *payload) {
    switch (command) {
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach(static_cast<BML_ModAttachArgs *>(payload));
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach(static_cast<BML_ModDetachArgs *>(payload));
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}
