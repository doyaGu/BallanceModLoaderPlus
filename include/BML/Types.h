// The few values that mean the same thing on both sides of a boundary, spelled so that the
// bytes are the same whatever built them. A vector, a matrix, or a reference to one of the
// game's objects comes up in more than one interface, so they are written down once here
// instead of once per interface, and both the loader's interface structs and an IMC payload
// use these. Everything is plain C with a fixed layout and no padding to guess at, which is
// what makes it usable from C, from C++, and from a language binding.
//
// Vectors and matrices are numbers and nothing more, so TypeConvert.h converts between these
// and the Virtools types, VxVector and VxMatrix, for a Mod on the C++ side.
//
// A BML_ObjectRef is how a loader interface names one of the game's objects without exposing
// a CK pointer or a recycled bare CK_ID. The interface that issued it is its only resolver and
// invalidates it on the normal CK deletion and world-reset lifecycle. Treat it as opaque: a
// zero Domain is null, all three fields participate in equality, and an invalid reference
// answers BML_ERROR_OBJECT_INVALID. It is good for this process and current world only, so do
// not persist it or hold it across a level change. CK objects deliberately destroyed with
// CK_DESTROY_NONOTIFY are not eligible for this cross-interface lifetime contract.
#ifndef BML_TYPES_H
#define BML_TYPES_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

#define BML_OBJECT_DOMAIN_VIRTOOLS 1u

/* An interface-issued object reference. A zero Domain denotes null. Slot and
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

typedef struct BML_Quaternion {
    float x;
    float y;
    float z;
    float w;
} BML_Quaternion;

typedef struct BML_Euler {
    float x;
    float y;
    float z;
} BML_Euler;

typedef struct BML_Rect {
    float left;
    float top;
    float right;
    float bottom;
} BML_Rect;

typedef struct BML_Color {
    float r;
    float g;
    float b;
    float a;
} BML_Color;

typedef struct BML_Box {
    BML_Vec3 Min;
    BML_Vec3 Max;
} BML_Box;

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
