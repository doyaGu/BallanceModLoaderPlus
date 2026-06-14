#ifndef BML_EXPORTREGISTRY_H
#define BML_EXPORTREGISTRY_H

#include "BML/Interop.h"

#include <atomic>
#include <cstdint>
#include <string>

#if BML_ENABLE_ANGELSCRIPT
namespace BML {
class ScriptMod;
struct ScriptExportBinding;
}
#endif

struct BML_ExportKey {
    std::string ModId;
    std::string Name;
    std::string Signature;
};

struct BML_NativeExportEntry {
    BML_NativeExportCallback Callback = nullptr;
    void *UserData = nullptr;
};

enum class BML_ResolvedExportKind {
    None,
    Native,
    Script
};

struct BML_ModExport {
    std::atomic<uint32_t> RefCount;
    BML_ExportKey Key;
    BML_ResolvedExportKind Kind = BML_ResolvedExportKind::None;
    uint64_t NativeGeneration = 0;
    uint64_t ScriptGeneration = 0;
    BML_NativeExportEntry Native;
#if BML_ENABLE_ANGELSCRIPT
    BML::ScriptMod *Script = nullptr;
    const BML::ScriptExportBinding *ScriptBinding = nullptr;
#endif

    explicit BML_ModExport(BML_ExportKey key);
};

namespace BML {

struct ExportInfo {
    std::string Name;
    std::string Signature;
};

class ExportRegistry {
public:
    static int RegisterNative(const char *modId,
                              const char *name,
                              const char *signature,
                              BML_NativeExportCallback callback,
                              void *userData);
    static int UnregisterNative(const char *modId, const char *name, const char *signature);
    static int UnregisterNativeForMod(const char *modId);

    static BML_ModExport *Find(const char *modId, const char *name, const char *signature);
    static int FindEx(const char *modId, const char *name, const char *signature, BML_ModExport **outHandle);
    static int Call(BML_ModExport *handle, BML_CallFrame *frame);
    static bool IsValid(BML_ModExport *handle);
    static int GetCount(const char *modId);
    static int GetInfo(const char *modId, size_t index, ExportInfo &info);

    static void NotifyScriptExportsChanged();
    static uint64_t GetScriptGeneration();

private:
    ExportRegistry() = delete;
};

} // namespace BML

#endif
