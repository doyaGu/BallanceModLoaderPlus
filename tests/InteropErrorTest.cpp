#include "BML/Defines.h"

#include <gtest/gtest.h>

TEST(InteropErrorTest, ReturnsInteropErrorStrings) {
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_TARGET_NOT_FOUND), "Interop target mod not found");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_TARGET_FAILED), "Interop target mod failed or unavailable");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_EXPORT_NOT_FOUND), "Interop export not found");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_EXPORT_AMBIGUOUS), "Interop export name is ambiguous");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_BAD_SIGNATURE),
                 "Interop export signature is malformed or unsupported");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_SIGNATURE_MISMATCH),
                 "Interop export signature mismatch");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_BAD_CALL_FRAME), "Interop call frame is invalid");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_TYPE_MISMATCH),
                 "Interop call frame value type mismatch");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED),
                 "Interop target export execution failed");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_HANDLE_STALE), "Interop export handle is stale");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_UNSUPPORTED),
                 "Interop operation is unsupported by the current runtime");
}
