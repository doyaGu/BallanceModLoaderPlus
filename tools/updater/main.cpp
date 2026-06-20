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

        if (command == L"check") {
            std::string channel = "stable";
            for (int i = 2; i + 1 < argc; ++i) {
                if (std::wstring(argv[i]) == L"--channel") {
                    channel = Narrow(argv[i + 1]);
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
