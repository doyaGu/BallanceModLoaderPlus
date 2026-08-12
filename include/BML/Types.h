// The few values that mean the same thing on both sides of a boundary, spelled so that the
// bytes are the same whatever built them. A vector, a matrix, or a reference to one of the
// game's objects comes up in more than one interface, so they are written down once here
// instead of once per interface, and both the loader's interface structs and an IMC payload
// use these. Everything is plain C with a fixed layout and no padding to guess at, which is
// what makes it usable from C, from C++, and from a language binding.
//
// Vectors and matrices are numbers and nothing more, so ImcMath.h converts between these
// and the Virtools types, VxVector and VxMatrix, for a Mod on the C++ side.
//
// A BML_ObjectRef is how one of the game's objects is named across the boundary, and it is
// deliberately not a pointer or a bare CK_ID: those are recycled, so an old one would come
// back pointing at whatever took its place. Only whoever made a reference can resolve it,
// which for the loader's own interfaces means turning it back into the CKObject it stood
// for, or into nothing if that object is gone. Treat it as opaque: a zero Domain is null,
// two references are the same object when all three fields match, and resolving one that
// has gone stale answers BML_ERROR_OBJECT_INVALID rather than something wrong. They are
// good for this run of the process only, so a reference is not something to write to a file
// or to hold across a level change.
#ifndef BML_TYPES_H
#define BML_TYPES_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

#define BML_OBJECT_DOMAIN_VIRTOOLS 1u

/* An owner-issued object identity.  A zero Domain denotes null.  Slot and
 * Generation are opaque to consumers and never encode a CK pointer. */
typedef struct BML_ObjectRef {
    uint32_t Domain;
    uint32_t Slot;
    uint32_t Generation;
} BML_ObjectRef;

typedef struct BML_Vec2 {
    float x;
    float y;
} BML_Vec2;

typedef struct BML_Vec3 {
    float x;
    float y;
    float z;
} BML_Vec3;

/* Always row-major m[row][column].  Matrix conversion is element-wise; the
 * object representation of a host math type is never part of this ABI. */
typedef struct BML_Mat4 {
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
} BML_Mat4;

BML_END_CDECLS

#endif // BML_TYPES_H
