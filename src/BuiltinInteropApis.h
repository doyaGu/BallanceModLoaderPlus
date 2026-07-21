#ifndef BML_BUILTININTEROPAPIS_H
#define BML_BUILTININTEROPAPIS_H

#include "BML/InteropTypes.h"
#include "CKTypes.h"

#include <utility>

#include "InteropEventSnapshot.h"

class BMLMod;
class ILogger;
class ModContext;
class CKObject;

/* Registers BML-owned generated APIs.  This is private composition code:
 * the public cross-DLL boundary remains the C ABI in InteropApi.h. */
void RegisterBuiltinInteropApis(BMLMod &mod, ILogger *logger);
void UnregisterBuiltinInteropApis(BMLMod &mod);
void PublishBuiltinInteropEvent(ModContext &context, const BML::InteropEventSnapshot &event);
bool HasBuiltinInteropEventConsumers(ModContext &context) noexcept;

/* Event telemetry is strictly observational.  Construct the potentially
 * allocating snapshot inside this boundary so an OOM or a provider failure
 * never changes the outcome of an original game hook. */
template <typename Capture>
void CaptureBuiltinInteropEventNoexcept(ModContext &context, Capture &&capture) noexcept {
    try {
        if (!HasBuiltinInteropEventConsumers(context))
            return;
        BML::InteropEventSnapshot event;
        std::forward<Capture>(capture)(event);
        PublishBuiltinInteropEvent(context, event);
    } catch (...) {
        // Observability must never escape into the original game callback.
    }
}

/* Called only by the loader's Virtools-manager callbacks.  This stays private
 * because ObjectRef never exposes CK lifetime mechanics across the ABI. */
void InvalidateBuiltinObjectRefs(ModContext &context, const CK_ID *ids, int count);
void InvalidateAllBuiltinObjectRefs(ModContext &context);

/* Script bindings are allowed to translate an already-borrowed CKObject to
 * the built-in scene provider's opaque identity, and back again at the final
 * host boundary.  These are private loader helpers: neither CKObject nor this
 * mapping appears in the public Interop ABI. */
BML_ObjectRef MakeBuiltinObjectRef(ModContext &context, CKObject *object);
CKObject *ResolveBuiltinObjectRef(ModContext &context, BML_ObjectRef reference);

#endif // BML_BUILTININTEROPAPIS_H
