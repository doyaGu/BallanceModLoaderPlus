// The versioned struct of function pointers the loader hands out, and the one
// exported function that hands it over. This is how a capability added after the
// legacy C++ interfaces were frozen reaches a native Mod: no vtable slot moves,
// and nothing is encoded on the way.
//
// An interface is a plain C struct whose first member is named Header and is a
// BML_InterfaceHeader, and whose remaining members are function pointers. The
// loader owns the struct. It is one static instance, const for the life of the
// process, and the same pointer for every Mod, so there is nothing to release and
// no lifetime to track. BML_GetInterface only looks the struct up in a table, so
// it answers before the loader has finished initializing; the functions behind
// the pointers need loader state and answer BML_ERROR_FAIL until it is there.
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
// A Mod asks for one major version and the loader refuses any other, so a
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
// literal the loader owns, and matches the id BML_GetInterface was asked for.
typedef struct BML_InterfaceHeader {
    size_t StructSize;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    const char *InterfaceId;
} BML_InterfaceHeader;

// Fills in a header. The loader uses this; a Mod only reads what it gets back.
#define BML_IFACE_HEADER(type, id, major, minor) {sizeof(type), (major), (minor), (id)}

// Whether the running loader has this member and filled it in. Answers false for
// a null interface as well, so a single check covers both the missing interface
// and the missing member.
#define BML_IFACE_HAS(iface, type, member)                                        \
    ((iface) != NULL &&                                                           \
     (iface)->Header.StructSize >= offsetof(type, member) + sizeof(((type *) 0)->member) && \
     (iface)->member != NULL)

// Looks up one interface by id and major version. Answers BML_OK and writes the
// loader's pointer, BML_ERROR_NOT_FOUND when no interface carries that id,
// BML_ERROR_VERSION_MISMATCH when one does but not in that major version, or
// BML_ERROR_INVALID_PARAMETER for a null id or a null out. The pointer written
// belongs to the loader: never free it, and never write through it.
BML_EXPORT int BML_GetInterface(const char *interfaceId, uint16_t majorVersion, const void **out);

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
