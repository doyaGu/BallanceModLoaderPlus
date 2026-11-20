#include <BML/BML.h>
#include <BML/Mod.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
    using Path = std::filesystem::path;

    void Require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class Library {
    public:
        explicit Library(const Path &path) {
#ifdef _WIN32
            m_Handle = ::LoadLibraryExW(path.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!m_Handle)
                throw std::runtime_error("LoadLibraryExW failed with code " +
                                         std::to_string(::GetLastError()));
#else
            m_Handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!m_Handle)
                throw std::runtime_error(dlerror());
#endif
        }

        ~Library() {
#ifdef _WIN32
            ::FreeLibrary(m_Handle);
#else
            dlclose(m_Handle);
#endif
        }

        Library(const Library &) = delete;
        Library &operator=(const Library &) = delete;

        auto Symbol(const char *name) const {
#ifdef _WIN32
            return ::GetProcAddress(m_Handle, name);
#else
            return dlsym(m_Handle, name);
#endif
        }

        template <class Function>
        Function Get(const char *name) const {
            auto symbol = Symbol(name);
            Require(symbol != nullptr, name);
            return reinterpret_cast<Function>(symbol);
        }

    private:
#ifdef _WIN32
        HMODULE m_Handle = nullptr;
#else
        void *m_Handle = nullptr;
#endif
    };

    const BML_ModInterface *Query(const Library &lib) {
        const auto query = lib.Get<decltype(&BML_GetModInterface)>("BML_GetModInterface");
        const BML_ModInterface *interface = nullptr;
        Require(query(1, &interface) == 0 && interface && interface->Definition, "Missing Mod metadata");
        const auto *original = interface;
        Require(query(1, &interface) == 0 && interface == original, "Mod query reparsed its metadata");
        Require(query(2, &interface) < 0 && !interface, "Unsupported Mod interface accepted");
        return original;
    }

    void CheckLoader(const Library &loader, std::string_view release) {
        const auto text = loader.Get<decltype(&BML_GetVersionString)>("BML_GetVersionString");
        const auto version = loader.Get<decltype(&BML_GetVersion)>("BML_GetVersion");
        int major = 0, minor = 0, patch = 0;
        version(&major, &minor, &patch);
        Require(text() == release, "SDK version string did not match Loader");
        Require(release == std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch),
                "SDK numeric version did not match Loader");
        for (const char *name : {"CKGetPluginInfoCount", "CKGetPluginInfo", "RegisterBehaviorDeclarations",
                                 "BML_GetInterface"})
            Require(loader.Symbol(name) != nullptr, name);
        for (const char *name : {"BML_GetLoaderPathW", "BML_GetLoaderPathUtf8", "BML_GetModRootW",
                "BML_GetModRootUtf8", "BML_UnregisterCommand", "BML_SetCommandStatus", "BML_GetCommandInput",
                "BML_GetConfigPropertyEditor", "BML_SetConfigPropertyEditor", "BML_GetConfigPropertyChoiceCount",
                "BML_GetConfigPropertyChoice", "BML_SetConfigPropertyChoices", "_ZTI4IMod", "_ZTI4IBML",
                "_ZTI9IProperty", "_ZTI7IConfig", "_ZTI7ILogger", "_ZTI16IMessageReceiver"})
            Require(loader.Symbol(name) == nullptr, name);
    }

    void CheckIsolation(const Path &first, const Path &second) {
        const Library a(first), b(second);
        const auto *left = Query(a);
        const auto *right = Query(b);
        using Counter = int (BML_CDECL *)();
        const auto countA = a.Get<Counter>("BML_TestInstances");
        const auto countB = b.Get<Counter>("BML_TestInstances");
        Require(countA() == 0 && countB() == 0, "Query constructed a Mod");
        void *leftInstance = nullptr, *rightInstance = nullptr;
        Require(left->Create(0, &leftInstance) == 0 && leftInstance, "First Mod instance could not be created");
        Require(countA() == 1 && countB() == 0, "First Mod reused the second module's state");
        Require(right->Create(0, &rightInstance) == 0 && rightInstance, "Second Mod instance could not be created");
        Require(countA() == 1 && countB() == 1, "Second Mod reused the first module's state");
        left->Destroy(leftInstance);
        right->Destroy(rightInstance);
        Require(countA() == 0 && countB() == 0, "Module state remained after instance teardown");
    }

    void CheckMetadata(const Path &module, const std::string &release, int argc, const Path *args) {
        Require(argc == 5, "Expected id, name, version, author and dependency count");
        const Library library(module);
        const auto &fields = *Query(library)->Definition;
        Require(fields.Id == args[0].string() && fields.Name == args[1].string() &&
                fields.Version == args[2].string() && fields.Author == args[3].string() &&
                fields.MinimumLoaderVersion == release &&
                fields.DependencyCount == std::stoul(args[4].string()), "Mod metadata does not match the project");
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        Require(argc >= 5, "Expected Loader, release, dependency count and probe command");
        const std::vector<Path> args(argv + 1, argv + argc);
        const auto count = std::stoul(args[2].string());
        Require(count <= args.size() - 4, "Missing dependency paths");
#ifdef _WIN32
        Require(::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0, "DLL search setup failed");
        for (std::size_t i = 0; i < count; ++i)
            Require(::AddDllDirectory(args[3 + i].parent_path().c_str()) != nullptr, "DLL directory setup failed");
#endif
        std::vector<std::unique_ptr<Library>> dependencies;
        for (std::size_t i = 0; i < count; ++i)
            dependencies.push_back(std::make_unique<Library>(args[3 + i]));
        const Library loader(args[0]);
        const auto release = args[1].string();
        const auto &command = args[count + 3];
        const auto *parameters = args.data() + count + 4;
        const auto parameterCount = args.size() - count - 4;
        if (command == "loader") {
            CheckLoader(loader, release);
        } else if (command == "isolation") {
            Require(parameterCount == 2, "Expected both installed modules");
            CheckIsolation(parameters[0], parameters[1]);
        } else if (command == "metadata") {
            Require(parameterCount == 6, "Expected metadata arguments");
            CheckMetadata(parameters[0], release, 5, parameters + 1);
        } else {
            throw std::runtime_error("Unknown native probe command");
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
