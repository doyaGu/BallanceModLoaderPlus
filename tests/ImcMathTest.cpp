#include "BML/ImcMath.h"

#include <gtest/gtest.h>

TEST(ImcMathTest, MatrixCopiesRowMajorElementsExplicitly) {
    VxMatrix native;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column)
            native[row][column] = static_cast<float>(row * 10 + column);
    }

    const BML_Mat4 abi = BML::Imc::ToMat4(native);
    EXPECT_FLOAT_EQ(0.0f, abi.m00);
    EXPECT_FLOAT_EQ(13.0f, abi.m13);
    EXPECT_FLOAT_EQ(21.0f, abi.m21);
    EXPECT_FLOAT_EQ(33.0f, abi.m33);

    const VxMatrix restored = BML::Imc::ToVxMatrix(abi);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column)
            EXPECT_FLOAT_EQ(native[row][column], restored[row][column]);
    }
}
