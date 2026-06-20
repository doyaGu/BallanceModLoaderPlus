#include <Windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "UpdaterService.h"

namespace {
    std::string Narrow(const std::wstring &text) {
        if (text.empty()) {
            return {};
        }
        const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), count, nullptr, nullptr);
        return out;
    }

    void PrintResult(const bmlupdater::Result &result) {
        std::cout << (result.ok ? "OK: " : "ERROR: ") << result.message << "\n";
    }

    void PrintUsage() {
        std::cout <<
            "Updater.exe commands:\n"
            "  status\n"
            "  doctor\n"
            "  source [show]\n"
            "  source set <https-base-url> [--channel stable|beta]\n"
            "  source clear\n"
            "  check [--channel stable|beta]\n"
            "  verify-local <package>\n"
            "  apply-local <package>\n"
            "  rollback\n"
            "  --help\n";
    }

    int RunCli(int argc, wchar_t **argv, bmlupdater::UpdaterService &service) {
        const std::wstring command = argc > 1 ? argv[1] : L"--help";
        if (command == L"--help" || command == L"-h" || command == L"/?") {
            PrintUsage();
            return 0;
        }

        if (command == L"status") {
            const bmlupdater::StatusInfo status = service.GetStatus();
            std::cout << "installedVersion=" << status.installedVersion << "\n";
            std::cout << "pendingTransaction=" << (status.pendingTransaction ? "true" : "false") << "\n";
            if (!status.lastError.empty()) {
                std::cout << "lastError=" << status.lastError << "\n";
            }
            return 0;
        }

        if (command == L"doctor") {
            std::vector<std::string> diagnostics;
            bmlupdater::Result result = service.RunDoctor(diagnostics);
            for (const std::string &line : diagnostics) {
                std::cout << line << "\n";
            }
            PrintResult(result);
            return result.ok ? 0 : 1;
        }

        if (command == L"source") {
            const std::wstring action = argc > 2 ? argv[2] : L"show";
            if (action == L"show") {
                bmlupdater::UpdaterSourceConfig config;
                bmlupdater::Result result = service.GetSourceConfig(config);
                if (!config.baseUrl.empty()) {
                    std::cout << "baseUrl=" << config.baseUrl << "\n";
                    std::cout << "defaultChannel=" << config.defaultChannel << "\n";
                } else {
                    std::cout << "baseUrl=<none>\n";
                }
                PrintResult(result);
                return result.ok ? 0 : 1;
            }
            if (action == L"set") {
                if (argc < 4) {
                    PrintUsage();
                    return 2;
                }
                std::string defaultChannel = "stable";
                for (int i = 4; i < argc; ++i) {
                    const std::wstring option = argv[i];
                    if (option == L"--channel" && i + 1 < argc) {
                        defaultChannel = Narrow(argv[++i]);
                    } else {
                        PrintUsage();
                        return 2;
                    }
                }
                bmlupdater::Result result = service.SetSourceBaseUrl(Narrow(argv[3]), defaultChannel);
                PrintResult(result);
                return result.ok ? 0 : 1;
            }
            if (action == L"clear") {
                bmlupdater::Result result = service.ClearSource();
                PrintResult(result);
                return result.ok ? 0 : 1;
            }
            PrintUsage();
            return 2;
        }

        if (command == L"check") {
            std::string channel;
            for (int i = 2; i < argc; ++i) {
                const std::wstring option = argv[i];
                if (option == L"--channel" && i + 1 < argc) {
                    channel = Narrow(argv[++i]);
                } else {
                    PrintUsage();
                    return 2;
                }
            }
            std::vector<std::string> diagnostics;
            bmlupdater::Result result = service.CheckForUpdates(channel, diagnostics);
            for (const std::string &line : diagnostics) {
                std::cout << line << "\n";
            }
            PrintResult(result);
            return result.ok ? 0 : 1;
        }

        if (command == L"verify-local" || command == L"apply-local") {
            if (argc < 3) {
                PrintUsage();
                return 2;
            }
            const std::wstring package = argv[2];
            bmlupdater::LocalPackageVerification verification;
            bmlupdater::Result result = service.VerifyLocalPackage(package, verification, [](const std::string &line) {
                std::cout << line << "\n";
            });
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            if (command == L"verify-local") {
                std::cout << "version=" << verification.manifest.version << "\n";
                std::cout << "managedFiles=" << verification.manifest.managedFiles.size() << "\n";
                PrintResult(result);
                return 0;
            }
            bmlupdater::ApplyPlan plan;
            result = service.BuildApplyPlan(verification, plan);
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            for (const std::string &diagnostic : plan.diagnostics) {
                std::cout << diagnostic << "\n";
            }
            std::cout << "checking write access\n";
            result = service.CheckApplyAccess(plan);
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            result = service.Apply(verification, plan, [](const std::string &line) {
                std::cout << line << "\n";
            });
            PrintResult(result);
            return result.ok ? 0 : 1;
        }

        if (command == L"rollback") {
            std::cout << "checking write access\n";
            bmlupdater::Result result = service.CheckRollbackAccess();
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            result = service.Rollback([](const std::string &line) {
                std::cout << line << "\n";
            });
            PrintResult(result);
            return result.ok ? 0 : 1;
        }

        PrintUsage();
        return 2;
    }
}

int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    bmlupdater::UpdaterService service(bmlupdater::CreateDefaultContext());
    return RunCli(argc, argv, service);
}
