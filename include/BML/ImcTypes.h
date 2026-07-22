#ifndef BML_IMC_TYPES_H
#define BML_IMC_TYPES_H

#include "BML/Defines.h"

/*
 * Fixed-layout values shared by typed IMC payloads.
 *
 * This header intentionally has no C++ declarations.  It is safe to include
 * from C, C++, and foreign-function bindings; every cross-DLL value is a POD
 * value with an explicitly documented representation.
 */
BML_BEGIN_CDECLS

#define BML_IMC_OBJECT_DOMAIN_VIRTOOLS 1u

/* A provider-owned object identity.  A zero Domain denotes null.  Slot and
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

#endif // BML_IMC_TYPES_H
