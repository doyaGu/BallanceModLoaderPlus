// Turning the game's vectors and matrices into the ones an IMC payload carries, and back.
// A Mod holding a VxVector has to hand a BML_Vec3 to an IMC call, and the two are the same
// three floats, so these do the copy and there is nothing more to it.
//
// They are inline and header-only, so this is the one IMC header that needs the Virtools
// SDK and C++: a Mod that only speaks the C ABI includes Types.h and leaves this alone.
// The copies are written out element by element on purpose, since VxMatrix keeps its
// storage to itself and its layout is not something to assume; the IMC matrix is row-major
// m[row][column].
#ifndef BML_IMCMATH_H
#define BML_IMCMATH_H

#include "BML/Types.h"

#include "Vx2dVector.h"
#include "VxMatrix.h"
#include "VxVector.h"

namespace BML::Imc {

inline BML_Vec2 ToVec2(const Vx2DVector &value) {
    return {value.x, value.y};
}

inline Vx2DVector ToVxVector(const BML_Vec2 &value) {
    return Vx2DVector(value.x, value.y);
}

inline BML_Vec3 ToVec3(const VxVector &value) {
    return {value.x, value.y, value.z};
}

inline VxVector ToVxVector(const BML_Vec3 &value) {
    return VxVector(value.x, value.y, value.z);
}

/* Explicit element copies deliberately avoid VxMatrix's private storage and
 * any ABI-dependent padding.  The IMC matrix is always row-major m[row][col]. */
inline BML_Mat4 ToMat4(const VxMatrix &value) {
    return {value[0][0], value[0][1], value[0][2], value[0][3],
            value[1][0], value[1][1], value[1][2], value[1][3],
            value[2][0], value[2][1], value[2][2], value[2][3],
            value[3][0], value[3][1], value[3][2], value[3][3]};
}

inline VxMatrix ToVxMatrix(const BML_Mat4 &value) {
    VxMatrix result;
    result[0][0] = value.m00; result[0][1] = value.m01; result[0][2] = value.m02; result[0][3] = value.m03;
    result[1][0] = value.m10; result[1][1] = value.m11; result[1][2] = value.m12; result[1][3] = value.m13;
    result[2][0] = value.m20; result[2][1] = value.m21; result[2][2] = value.m22; result[2][3] = value.m23;
    result[3][0] = value.m30; result[3][1] = value.m31; result[3][2] = value.m32; result[3][3] = value.m33;
    return result;
}

} // namespace BML::Imc

#endif // BML_IMCMATH_H
