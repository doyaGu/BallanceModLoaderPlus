// Versioned structs of function pointers handed out through BML. This is how a
// capability added after the legacy C++ interfaces were frozen reaches a native
// Mod: no vtable slot moves, and nothing is encoded on the way. Interfaces may
// be built into the loader or registered by a native base Mod.
//
// An interface is a plain C struct whose first member is named Header and is a
// BML_InterfaceHeader, and whose remaining members are function pointers. The
// provider owns the struct. It must be one static const instance which stays at
// the same address while registered, so there is nothing for a consumer to
// release. Built-in interfaces exist for the loader process lifetime. A Mod
// interface exists from its successful BML_RegisterInterface call until it is
// unregistered or the provider Mod unloads. Consumers declare that provider as
// a required Mod dependency and do their first lookup from OnLoad.
//
// Growing an interface is the only part that needs care:
//
// - A new function pointer is appended at the end, never inserted between
//   existing members and never removed, and the minor version goes up.
// - StructSize says how much of the struct the running loader actually has.
//   BML_IFACE_HAS is the check: it compares StructSize against the end of the
//   member being asked about and then checks that the pointer is not null, so a
//   Mod compiled against a newer header keeps working against an older loader
//   for as long as it asks before it calls.
// - Moving a member, or changing what one of them takes or returns, is a new
//   major version and therefore a new struct and a new id, not an edit.
//
// A consumer asks for one major version and the registry refuses any other, so a
// successful BML_GetInterface means every member the Mod's own header declares
// either exists or is covered by a BML_IFACE_HAS check.
//
// Text the loader writes back goes into a fixed-capacity char array inside the out
// struct, always terminated, next to a separate member saying how long the whole
// string is. Nothing is allocated and there is nothing to free; a length larger
// than the array means the answer was cut short, which is the one thing worth
// checking. Text passed in is a plain null-terminated const char *, borrowed only
// for the duration of the call.
//
// Every function reached through an interface runs on the calling thread, with no
// queue in between, so like the legacy C++ interfaces they belong on the game
// thread unless their own header says otherwise.
#ifndef BML_INTERFACE_H
#define BML_INTERFACE_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

// The first member of every interface struct. InterfaceId points at a string
// literal the provider owns, and matches the id BML_GetInterface was asked for.
typedef struct BML_InterfaceHeader {
    size_t StructSize;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    const char *InterfaceId;
} BML_InterfaceHeader;

// Fills in a header. Both the loader and a provider Mod use this; consumers only
// read what they get back.
#define BML_IFACE_HEADER(type, id, major, minor) {sizeof(type), (major), (minor), (id)}

// Whether the running loader has this member and filled it in. Answers false for
// a null interface as well, so a single check covers both the missing interface
// and the missing member.
#define BML_IFACE_HAS(iface, type, member)                                        \
    ((iface) != NULL &&                                                           \
     (iface)->Header.StructSize >= offsetof(type, member) + sizeof(((type *) 0)->member) && \
     (iface)->member != NULL)

// Looks up one interface by id and major version. Answers BML_OK and writes the
// provider's pointer, BML_ERROR_NOT_FOUND when no interface carries that id,
// BML_ERROR_VERSION_MISMATCH when one does but not in that major version, or
// BML_ERROR_INVALID_PARAMETER for a null id or a null out. The pointer written
// belongs to its provider: never free it, and never write through it.
BML_EXPORT int BML_CDECL BML_GetInterface(
    const char *interfaceId, uint16_t majorVersion, const void **out);

// Publishes a provider-owned interface through BML_GetInterface. interfacePtr
// points at a static const struct whose first member is BML_InterfaceHeader;
// both that struct and Header.InterfaceId must reside in the provider DLL.
// Registration is game-thread-only and is accepted only from the native DLL
// that owns ownerId; pass NULL to infer the unique Mod owned by the caller DLL.
// Built-in ids and an existing id/major pair cannot be replaced. The provider
// should unregister in OnUnload; BML also removes every remaining registration
// before releasing the provider DLL.
BML_EXPORT int BML_CDECL BML_RegisterInterface(
    const char *ownerId, const void *interfacePtr);

// Removes one interface owned by the calling native Mod. A different provider's
// registration answers BML_ERROR_ACCESS_DENIED. This is game-thread-only.
BML_EXPORT int BML_CDECL BML_UnregisterInterface(
    const char *ownerId, const char *interfaceId, uint16_t majorVersion);

BML_END_CDECLS

#ifdef __cplusplus

namespace BML {

// The same lookup with the cast folded in, for a facade header that only wants
// the pointer or nothing. The status is available from BML_GetInterface itself
// when the reason matters.
template <typename Interface>
[[nodiscard]] inline const Interface *FindInterface(const char *interfaceId, uint16_t majorVersion) {
    const void *found = nullptr;
    if (BML_GetInterface(interfaceId, majorVersion, &found) != BML_OK)
        return nullptr;
    return static_cast<const Interface *>(found);
}

} // namespace BML

#endif // __cplusplus

#endif // BML_INTERFACE_H
