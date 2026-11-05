#ifdef _WIN32
#include <Windows.h>
#endif
#include "PathUtils.h"
#include "UpdaterPaths.h"
#include "StringUtils.h"
#include <SDL3/SDL.h>

#ifndef _WIN32
#define BML_UPDATER_COMMAND "Updater"
#else
#define BML_UPDATER_COMMAND "Updater.exe"
#endif

#include <iostream>
#include <string>
#include <vector>

#include "UpdaterService.h"

namespace {
    std::string Narrow(const std::wstring &text) {
        return utils::Utf16ToUtf8(text);
    }

    void PrintResult(const bmlupdater::Result &result) {
        std::cout << (result.ok ? "OK: " : "ERROR: ") << result.message << "\n";
    }

    void PrintPlanSummary(const bmlupdater::ApplyPlan &plan) {
        size_t installOrReplace = 0;
        size_t remove = 0;
        for (const bmlupdater::ApplyOperation &operation : plan.operations) {
            if (operation.kind == bmlupdater::OperationKind::InstallOrReplace) {
                ++installOrReplace;
            } else {
                ++remove;
            }
        }
        std::cout << "planVersion=" << plan.version << "\n";
        std::cout << "planOperations=" << plan.operations.size() << "\n";
        std::cout << "planInstallOrReplace=" << installOrReplace << "\n";
        std::cout << "planRemove=" << remove << "\n";
    }

    void PrintPlanDiagnostics(const bmlupdater::ApplyPlan &plan) {
        for (const std::string &diagnostic : plan.diagnostics) {
            std::cout << diagnostic << "\n";
        }
    }

    void PrintStatusInfo(bmlupdater::UpdaterService &service) {
        const bmlupdater::StatusInfo status = service.GetStatus();
        std::cout << "installedVersion=" << status.installedVersion << "\n";
        std::cout << "pendingTransaction=" << (status.pendingTransaction ? "true" : "false") << "\n";
        if (!status.lastError.empty()) {
            std::cout << "lastError=" << status.lastError << "\n";
        }
    }

    bmlupdater::Result PrintSourceInfo(bmlupdater::UpdaterService &service) {
        bmlupdater::UpdaterSourceConfig config;
        bmlupdater::Result result = service.GetSourceConfig(config);
        if (!config.baseUrl.empty()) {
            std::cout << "baseUrl=" << config.baseUrl << "\n";
            std::cout << "defaultChannel=" << config.defaultChannel << "\n";
        } else {
            std::cout << "baseUrl=<none>\n";
        }
        return result;
    }

    int PrintOverview(bmlupdater::UpdaterService &service) {
        std::cout << "BML+ Updater\n";
        PrintStatusInfo(service);
        bmlupdater::Result source = PrintSourceInfo(service);
        if (!source.ok) {
            PrintResult(source);
            return 1;
        }
        bmlupdater::UpdaterSourceConfig config;
        (void)service.GetSourceConfig(config);
        if (config.baseUrl.empty()) {
            std::cout << "nextStep=" BML_UPDATER_COMMAND " configure <https-base-url>\n";
        } else {
            std::cout << "nextStep=" BML_UPDATER_COMMAND " check\n";
        }
        std::cout << "help=" BML_UPDATER_COMMAND " --help\n";
        return 0;
    }

    int PrintNoRemoteWork(const bmlupdater::Result &result, const bmlupdater::RemoteUpdateInfo &info) {
        if (info.latestVersion.empty()) {
            std::cout << "nextStep=" BML_UPDATER_COMMAND " configure <https-base-url>\n";
        } else if (!info.updateAvailable) {
            std::cout << "nextStep=" BML_UPDATER_COMMAND " update --force\n";
        }
        PrintResult(result);
        return result.ok ? 0 : 1;
    }

    void PrintUsage() {
        std::cout << "SDL updater: --game-root <directory> selects a standard or flat runtime.\n";
        std::cout <<
            "BML+ Updater\n"
            "\n"
            "Common workflows:\n"
            "  " BML_UPDATER_COMMAND "\n"
            "  " BML_UPDATER_COMMAND " configure <https-base-url>\n"
            "  " BML_UPDATER_COMMAND " check\n"
            "  " BML_UPDATER_COMMAND " plan\n"
            "  " BML_UPDATER_COMMAND " update\n"
            "  " BML_UPDATER_COMMAND " apply-local <BMLPlus-Update-vX.Y.Z.zip>\n"
            "\n"
            "Commands:\n"
            "  overview\n"
            "  status\n"
            "  doctor\n"
            "  configure <https-base-url> [--channel stable|beta]\n"
            "  source [show]\n"
            "  source set <https-base-url> [--channel stable|beta]\n"
            "  source clear\n"
            "  check [-c stable|beta]\n"
            "  download [-c stable|beta] [--force]\n"
            "  plan [-c stable|beta] [--force]\n"
            "  plan-remote [-c stable|beta] [--force]\n"
            "  apply-remote [-c stable|beta] [--force]\n"
            "  update [-c stable|beta] [--force]\n"
            "  verify-local <package>\n"
            "  plan-local <package>\n"
            "  apply-local <package>\n"
            "  rollback\n"
            "  --help\n";
    }

    int RunCli(int argc, wchar_t **argv, bmlupdater::UpdaterService &service) {
        const std::wstring command = argc > 1 ? argv[1] : L"overview";
        if (command == L"--help" || command == L"-h" || command == L"/?") {
            PrintUsage();
            return 0;
        }

        if (command == L"overview") {
            return PrintOverview(service);
        }

        if (command == L"status") {
            PrintStatusInfo(service);
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

        if (command == L"configure" || command == L"source") {
            const std::wstring action = command == L"configure" ? L"set" : (argc > 2 ? argv[2] : L"show");
            if (action == L"show") {
                bmlupdater::Result result = PrintSourceInfo(service);
                if (result.ok) {
                    bmlupdater::UpdaterSourceConfig config;
                    (void)service.GetSourceConfig(config);
                    if (config.baseUrl.empty()) {
                        std::cout << "nextStep=" BML_UPDATER_COMMAND " configure <https-base-url>\n";
                    }
                }
                PrintResult(result);
                return result.ok ? 0 : 1;
            }
            if (action == L"set") {
                const int urlIndex = command == L"configure" ? 2 : 3;
                if (argc <= urlIndex) {
                    PrintUsage();
                    return 2;
                }
                std::string defaultChannel = "stable";
                for (int i = urlIndex + 1; i < argc; ++i) {
                    const std::wstring option = argv[i];
                    if ((option == L"--channel" || option == L"-c") && i + 1 < argc) {
                        defaultChannel = Narrow(argv[++i]);
                    } else {
                        PrintUsage();
                        return 2;
                    }
                }
                bmlupdater::Result result = service.SetSourceBaseUrl(Narrow(argv[urlIndex]), defaultChannel);
                PrintResult(result);
                if (result.ok) {
                    std::cout << "nextStep=" BML_UPDATER_COMMAND " check\n";
                }
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

        if (command == L"check" || command == L"download" || command == L"plan" || command == L"plan-remote" ||
            command == L"apply-remote" || command == L"update") {
            std::string channel;
            bool force = false;
            for (int i = 2; i < argc; ++i) {
                const std::wstring option = argv[i];
                if ((option == L"--channel" || option == L"-c") && i + 1 < argc) {
                    channel = Narrow(argv[++i]);
                } else if (option == L"--force") {
                    force = true;
                } else {
                    PrintUsage();
                    return 2;
                }
            }

            if (command == L"check") {
                std::vector<std::string> diagnostics;
                bmlupdater::Result result = service.CheckForUpdates(channel, diagnostics);
                for (const std::string &line : diagnostics) {
                    std::cout << line << "\n";
                }
                PrintResult(result);
                return result.ok ? 0 : 1;
            }

            if (command == L"download") {
                std::vector<std::string> diagnostics;
                bmlupdater::RemoteUpdateInfo info;
                bmlupdater::Result result = service.CheckRemote(channel, force, info, diagnostics);
                for (const std::string &line : diagnostics) {
                    std::cout << line << "\n";
                }
                if (!result.ok) {
                    PrintResult(result);
                    return 1;
                }
                if (info.latestVersion.empty() || !info.updateAvailable) {
                    return PrintNoRemoteWork(result, info);
                }
                bmlupdater::LocalPackageVerification verification;
                result = service.DownloadRemote(info, verification, [](const std::string &line) {
                    std::cout << line << "\n";
                });
                if (result.ok && !verification.manifest.version.empty()) {
                    std::cout << "version=" << verification.manifest.version << "\n";
                    std::cout << "managedFiles=" << verification.manifest.managedFiles.size() << "\n";
                }
                PrintResult(result);
                return result.ok ? 0 : 1;
            }

            std::vector<std::string> diagnostics;
            bmlupdater::RemoteUpdateInfo info;
            bmlupdater::Result result = service.CheckRemote(channel, force, info, diagnostics);
            for (const std::string &line : diagnostics) {
                std::cout << line << "\n";
            }
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            if (info.latestVersion.empty() || !info.updateAvailable) {
                return PrintNoRemoteWork(result, info);
            }

            bmlupdater::LocalPackageVerification verification;
            result = service.DownloadRemote(info, verification, [](const std::string &line) {
                std::cout << line << "\n";
            });
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            bmlupdater::ApplyPlan plan;
            result = service.BuildApplyPlan(verification, plan);
            if (!result.ok) {
                PrintResult(result);
                return 1;
            }
            PrintPlanDiagnostics(plan);
            PrintPlanSummary(plan);
            if (command == L"plan" || command == L"plan-remote") {
                PrintResult(result);
                return 0;
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

        if (command == L"verify-local" || command == L"plan-local" || command == L"apply-local") {
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
            PrintPlanDiagnostics(plan);
            PrintPlanSummary(plan);
            if (command == L"plan-local") {
                PrintResult(result);
                return 0;
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

namespace {
int RunSdlCli(int argc, wchar_t **argv) {
    std::cout.setf(std::ios::unitbuf);
    auto context = bmlupdater::CreateDefaultContext();
    std::vector<wchar_t *> arguments;
    for (int i = 0; i < argc; ++i) {
        if (i > 0 && std::wstring_view(argv[i]) == L"--game-root") {
            if (++i == argc) { std::cerr << "--game-root requires a directory\n"; return 2; }
            context.gameRoot = utils::ResolvePathW(argv[i]);
            context.updaterStateRoot = bmlupdater::DefaultStateRoot(context.gameRoot);
        } else arguments.push_back(argv[i]);
    }
    bmlupdater::UpdaterService service(context);
    return RunCli(static_cast<int>(arguments.size()), arguments.data(), service);
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    return RunSdlCli(argc, argv);
}
#else
int main(int argc, char **argv) {
    std::vector<std::wstring> wide;
    std::vector<wchar_t *> arguments;
    for (int i = 0; i < argc; ++i) {
        if (!utils::IsValidUtf8(argv[i])) { std::cerr << "Arguments must be UTF-8\n"; return 2; }
        wide.push_back(utils::Utf8ToUtf16(argv[i]));
    }
    for (auto &argument : wide) arguments.push_back(argument.data());
    return RunSdlCli(argc, arguments.data());
}
#endif
