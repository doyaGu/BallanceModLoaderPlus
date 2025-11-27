#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <cstring>

#include "bml_export.h"

namespace {

struct Options {
    bool help = false;
    bool listOrder = false;
    bool parseError = false;
    std::filesystem::path modsOverride;
};

void PrintUsage() {
    std::wcout << L"BMLCoreDriver usage:\n"
               << L"  BMLCoreDriver.exe [--mods <path>] [--list]\n\n"
               << L"Options:\n"
               << L"  --mods <path>  Override Mods directory (sets BML_MODS_DIR for this process).\n"
               << L"  --list         Always print resolved load order, even if empty.\n"
               << L"  --help         Show this message.\n";
}

Options ParseOptions(int argc, wchar_t **argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--help" || arg == L"-h") {
            opts.help = true;
        } else if (arg == L"--mods") {
            if (i + 1 >= argc) {
                std::wcerr << L"--mods requires a path" << std::endl;
                opts.parseError = true;
                break;
            }
            opts.modsOverride = std::filesystem::path(argv[++i]);
        } else if (arg == L"--list") {
            opts.listOrder = true;
        } else {
            std::wcerr << L"Unknown argument: " << arg << std::endl;
            opts.parseError = true;
            break;
        }
    }
    return opts;
}

std::filesystem::path GetExecutableDirectory() {
    std::wstring buffer(260, L'\0');
    while (true) {
        DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            return {};
        }
        if (copied < buffer.size() - 1) {
            buffer.resize(copied);
            break;
        }
        buffer.resize(buffer.size() * 2, L'\0');
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path NormalizePath(const std::filesystem::path &input) {
    try {
        return std::filesystem::weakly_canonical(input);
    } catch (const std::exception &) {
        return std::filesystem::absolute(input);
    }
}

std::wstring Utf8ToWide(const char *text) {
    if (!text)
        return {};
    int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (required <= 1)
        return {};
    std::wstring buffer(static_cast<size_t>(required - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer.data(), required - 1);
    return buffer;
}

void PrintManifestErrors(const BML_BootstrapDiagnostics *diag) {
    if (!diag || diag->manifest_error_count == 0 || !diag->manifest_errors)
        return;

    std::cout << "Manifest errors (" << diag->manifest_error_count << "):" << std::endl;
    for (uint32_t i = 0; i < diag->manifest_error_count; ++i) {
        const auto &err = diag->manifest_errors[i];
        std::cout << "  - " << (err.message ? err.message : "<unknown>");
        if (err.has_file && err.file) {
            std::cout << " (" << err.file;
            if (err.has_line)
                std::cout << ":" << err.line;
            if (err.has_column)
                std::cout << "," << err.column;
            std::cout << ")";
        }
        std::cout << std::endl;
    }
}

void PrintDependencyError(const BML_BootstrapDiagnostics *diag) {
    if (!diag)
        return;
    const auto &error = diag->dependency_error;
    if (!error.message || std::strlen(error.message) == 0)
        return;

    std::cout << "Dependency resolution failed: " << error.message;
    if (error.chain && error.chain_count) {
        std::cout << " | chain=";
        for (uint32_t i = 0; i < error.chain_count; ++i) {
            if (i)
                std::cout << " -> ";
            std::cout << (error.chain[i] ? error.chain[i] : "<unknown>");
        }
    }
    std::cout << std::endl;
}

void PrintLoadError(const BML_BootstrapDiagnostics *diag) {
    if (!diag)
        return;
    const auto &error = diag->load_error;
    if (!error.has_error || !error.message)
        return;

    std::wstring modId = Utf8ToWide(error.module_id);
    std::wstring msg = Utf8ToWide(error.message);
    std::wstring path = Utf8ToWide(error.path_utf8);

    std::wcout << L"Module load failed for '" << (modId.empty() ? L"<unknown>" : modId) << L"'";
    if (!path.empty()) {
        std::wcout << L" (" << path << L")";
    }
    if (!msg.empty()) {
        std::wcout << L": " << msg;
    }
    if (error.system_code != 0) {
        std::wcout << L" [code=" << error.system_code << L"]";
    }
    std::wcout << std::endl;
}

void PrintLoadOrder(const BML_BootstrapDiagnostics *diag) {
    if (!diag || !diag->load_order_count || !diag->load_order) {
        std::cout << "Load order: (empty)" << std::endl;
        return;
    }
    std::cout << "Load order (" << diag->load_order_count << "):";
    for (uint32_t i = 0; i < diag->load_order_count; ++i) {
        if (i)
            std::cout << ",";
        std::cout << " " << (diag->load_order[i] ? diag->load_order[i] : "<unknown>");
    }
    std::cout << std::endl;
}

bool DiagnosticsAreClean(const BML_BootstrapDiagnostics *diag) {
    if (!diag)
        return false;
    const bool hasManifest = diag->manifest_error_count != 0;
    const bool hasDependency = diag->dependency_error.message && std::strlen(diag->dependency_error.message) > 0;
    const bool hasLoad = diag->load_error.has_error;
    return !hasManifest && !hasDependency && !hasLoad;
}

} // namespace

int wmain(int argc, wchar_t **argv) {
    auto opts = ParseOptions(argc, argv);
    if (opts.help || opts.parseError) {
        PrintUsage();
        return opts.parseError ? 1 : 0;
    }

    auto exeDir = GetExecutableDirectory();
    if (exeDir.empty()) {
        std::wcerr << L"Unable to determine executable directory" << std::endl;
        return 2;
    }

    std::filesystem::path modsDir = exeDir / L"Mods";
    if (!opts.modsOverride.empty()) {
        modsDir = opts.modsOverride;
    }
    modsDir = NormalizePath(modsDir);

    std::wstring modsDirWide = modsDir.wstring();
    if (!modsDirWide.empty()) {
        if (!SetEnvironmentVariableW(L"BML_MODS_DIR", modsDirWide.c_str())) {
            std::wcerr << L"Failed to set BML_MODS_DIR" << std::endl;
        }
    }

    std::wcout << L"[driver] Mods directory: " << modsDirWide << std::endl;
    if (!std::filesystem::exists(modsDir)) {
        std::wcout << L"[driver] WARNING: directory does not exist." << std::endl;
    }

    BML_Result attach = bmlAttach();
    if (attach != BML_RESULT_OK) {
        std::wcout << L"[driver] bmlAttach failed: " << attach << std::endl;
        return 3;
    }

    const BML_BootstrapDiagnostics *diag = bmlGetBootstrapDiagnostics();
    PrintManifestErrors(diag);
    PrintDependencyError(diag);
    PrintLoadError(diag);
    if (opts.listOrder || (diag && diag->load_order_count > 0)) {
        PrintLoadOrder(diag);
    }

    bmlDetach();

    if (!modsDirWide.empty()) {
        SetEnvironmentVariableW(L"BML_MODS_DIR", nullptr);
    }

    if (!DiagnosticsAreClean(diag)) {
        std::wcout << L"[driver] Bootstrap completed with issues." << std::endl;
        return 4;
    }

    std::wcout << L"[driver] Bootstrap succeeded." << std::endl;
    return 0;
}
