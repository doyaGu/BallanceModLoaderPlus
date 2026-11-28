#ifndef BML_LOADER_NATIVEMOD_H
#define BML_LOADER_NATIVEMOD_H

#include "Loader/ModRecord.h"
#include "BML/Mod.h"
#include "Loader/ModState.h"
#include <memory>
#include <utility>

// Owns the native C definition, instance, and failure-cleanup state.
// The loader registry borrows Record(); no C++ object crosses the module boundary.
class NativeMod final {
public:
    ModRecord &Registration() noexcept { return m_Record; }

    const ModRecord &Registration() const noexcept { return m_Record; }

    static std::unique_ptr<NativeMod> Query(ModContext *host, const BML_ModInterface *api,
                                            std::string &diagnostic);

    const char *GetID() { return m_Id.c_str(); }

    const char *GetName() { return m_Name.c_str(); }

    const char *GetVersion() { return m_Version.c_str(); }

    const char *GetAuthor() { return m_Author.c_str(); }

    const char *GetDescription() { return m_Description.c_str(); }

    BMLVersion GetBMLVersion() { return m_MinimumVersion; }

    const BML_ModDefinition &Definition() const noexcept { return m_Definition; }

    const std::vector<ModDependency> &Dependencies() const noexcept { return m_Dependencies; }

    int Create(BML_ModContextHandle context);
    void Destroy() noexcept;
    int Load();
    int Unload();

    bool NeedsFailureCleanup() const noexcept {
        return m_State.IsFailed() && !m_FailureCleanupAttempted;
    }

    bool BeginFailureCleanup() noexcept {
        return m_State.IsFailed() && !std::exchange(m_FailureCleanupAttempted, true);
    }

    bool HasInstance() const noexcept { return m_Instance != nullptr; }

    bool CanDestroy() const noexcept { return !m_Instance || m_Unloaded; }

    bool IsFailed() const noexcept { return m_State.IsFailed(); }

    bool IsLoading() const noexcept { return m_Loading && !m_State.IsFailed(); }

    bool IsLoaded() const noexcept {
        return m_State.IsLoaded() && !m_Unloaded && !m_Unloading && !m_State.IsFailed();
    }

    void Fail(int code, const char *operation);

    const std::string &Diagnostic() const noexcept { return m_Diagnostic; }

    BML_ModContextHandle Context() const noexcept { return m_Context; }

    const BML_ModCallbacks &Callbacks() const noexcept { return m_Callbacks; }

    void *Instance() const noexcept { return m_Instance; }

private:
    explicit NativeMod(ModContext *host) : m_Record(host) {}

    std::string m_Id;
    std::string m_Name;
    std::string m_Version;
    std::string m_Author;
    std::string m_Description;
    std::string m_MinimumText;
    std::vector<ModDependency> m_Dependencies;
    std::vector<std::string> m_DependencyVersions;
    std::vector<BML_ModDependency> m_DependencyViews;
    BML_ModDefinition m_Definition{};
    BMLVersion m_MinimumVersion{0, 0, 0};
    BML_ModCallbacks m_Callbacks{};
    int(BML_CDECL *m_Create)(BML_ModContextHandle, void **) = nullptr;
    void(BML_CDECL *m_Destroy)(void *) = nullptr;
    BML_ModContextHandle m_Context = 0;
    void *m_Instance = nullptr;
    BML::ModState m_State;
    std::atomic<bool> m_Loading{false};
    std::atomic<bool> m_Unloaded{false};
    std::atomic<bool> m_Unloading{false};
    bool m_FailureCleanupAttempted = false;
    std::string m_Diagnostic;
    ModRecord m_Record;
};
#endif
