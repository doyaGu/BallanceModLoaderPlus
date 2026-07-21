#include "BML/Defines.h"

#include <gtest/gtest.h>

TEST(InteropErrorTest, ReturnsInteropErrorStrings) {
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND), "Interop API endpoint not found");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH),
                 "Interop record value type mismatch");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_HANDLE_STALE), "Interop handle is stale");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_UNSUPPORTED),
                 "Interop operation is unsupported by the current runtime");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_PROVIDER_UNLOADED), "Interop provider was unloaded");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_SCHEMA_MISMATCH),
                 "Interop record schema does not match the API endpoint");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED),
                 "Interop provider callback execution failed");
}
