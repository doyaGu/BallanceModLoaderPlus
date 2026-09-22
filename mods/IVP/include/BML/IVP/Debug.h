#ifndef BML_IVP_DEBUG_H
#define BML_IVP_DEBUG_H

#include "BML/IVP/Calls.h"
#include "BML/IVP/Types.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// Values 0..1023 are reserved by IVP. Applications may use 1024..2047.
enum IVP_DEBUG_CLASS : std::int32_t {
    IVP_DM_DUMMY,
    IVP_DM_SURBUILD_POINTSOUP,
    IVP_DM_SURBUILD_HALFSPACESOUP,
    IVP_DM_SURBUILD_LEDGESOUP,
    IVP_DM_SURBUILD_Q12,
    IVP_DM_CONVEX_DECOMPOSITOR,
    IVP_DM_QHULL,
    IVP_DM_GEOMPACK_LEVEL1,
    IVP_DM_GEOMPACK_LEVEL2,
    IVP_DM_GEOMPACK_LEVEL3,
    IVP_DM_CLUSTERING_SHORTRANGE_VISUALIZER,

    IVP_DEBUG_IPION_ERROR_MSG = 1024,
    IVP_DEBUG_MAX_N_CLASSES = 2048
};

class IVP_BetterDebugmanager {
    friend struct BML_IvpBetterDebugmanagerLayoutCheck;

private:
    int initialized;
    int flag_list[IVP_DEBUG_MAX_N_CLASSES];

public:
    // The retained complete constructor installs physics_RT's vtable, whose
    // scalar deleting destructor releases through the retail operator delete.
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    void enable_debug_output(IVP_DEBUG_CLASS class_id) {
        if (class_id >= IVP_DEBUG_MAX_N_CLASSES)
            return;
        flag_list[class_id] = 1;
    }

    void disable_debug_output(IVP_DEBUG_CLASS class_id) {
        if (class_id >= IVP_DEBUG_MAX_N_CLASSES)
            return;
        flag_list[class_id] = 0;
    }

    IVP_BOOL is_debug_enabled(IVP_DEBUG_CLASS class_id) {
        return BML::IVP::ABI::InvokeThisOr<IVP_BOOL>(
            BML::IVP::ABI::Address::BetterDebugIsEnabled, this,
            [this, class_id]() {
                if (class_id >= IVP_DEBUG_MAX_N_CLASSES)
                    return IVP_FALSE;
                return flag_list[class_id] != 0 && initialized != 0
                    ? IVP_TRUE
                    : IVP_FALSE;
            },
            class_id);
    }

    void dprint(IVP_DEBUG_CLASS class_id, const char *formatstring, ...) {
        char buffer[4096];
        va_list arguments;
        va_start(arguments, formatstring);
        std::vsnprintf(buffer, sizeof(buffer), formatstring, arguments);
        va_end(arguments);
        buffer[sizeof(buffer) - 1] = '\0';

        // A variadic x86 member function uses __cdecl and receives this on
        // the stack. Passing the already formatted text through "%s" keeps
        // the retained DLL body (and its virtual output dispatch) in use.
        using Function = void (__cdecl *)(
            IVP_BetterDebugmanager *, IVP_DEBUG_CLASS, const char *, ...);
        Function function = BML::IVP::ABI::Resolve<Function>(
            BML::IVP::ABI::Address::BetterDebugPrint);
        if (function)
            function(this, class_id, "%s", buffer);
        else
            output_function(class_id, buffer);
    }

    virtual void output_function(
        IVP_DEBUG_CLASS class_id, const char *string) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::BetterDebugOutput, this,
            [string]() { std::printf("%s", string); }, class_id, string);
    }

    IVP_BetterDebugmanager() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::BetterDebugCtor, this,
            [this]() {
                initialized = 1;
                std::memset(flag_list, 0, sizeof(flag_list));
            });
    }

    virtual ~IVP_BetterDebugmanager() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::BetterDebugDestruct, this, [] {});
    }
};

struct BML_IvpBetterDebugmanagerLayoutCheck {
    static constexpr std::size_t initialized =
        offsetof(IVP_BetterDebugmanager, initialized);
    static constexpr std::size_t flags =
        offsetof(IVP_BetterDebugmanager, flag_list);
};

static_assert(sizeof(IVP_BetterDebugmanager) == 0x2008);
static_assert(BML_IvpBetterDebugmanagerLayoutCheck::initialized == 0x04);
static_assert(BML_IvpBetterDebugmanagerLayoutCheck::flags == 0x08);

// Resolves the process-global instance retained by physics_RT.dll. It is null
// until the verified retail DLL is available through the BML IVP interface.
[[nodiscard]] inline IVP_BetterDebugmanager *IVP_Get_Debugmanager() noexcept {
    using Constructor = void (__thiscall *)(IVP_BetterDebugmanager *);
    Constructor constructor = BML::IVP::ABI::Resolve<Constructor>(
        BML::IVP::ABI::Address::BetterDebugCtor);
    if (!constructor)
        return nullptr;
    const std::uintptr_t imageBase =
        reinterpret_cast<std::uintptr_t>(constructor) -
        static_cast<std::uint32_t>(BML::IVP::ABI::Address::BetterDebugCtor);
    return reinterpret_cast<IVP_BetterDebugmanager *>(
        imageBase + BML::IVP::ABI::BetterDebugmanagerGlobalRva);
}

// Source-compatible spelling for IVP's global. As with the original extern,
// it may only be used while physics_RT.dll is resident and initialized.
#define ivp_debugmanager (*IVP_Get_Debugmanager())
#define IVP_IFDEBUG(dci) if (ivp_debugmanager.is_debug_enabled(dci))

#endif // BML_IVP_DEBUG_H
