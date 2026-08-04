#include "BML/Defines.h"

#include <gtest/gtest.h>

TEST(ImcErrorTest, ReturnsImcErrorStrings) {
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_ENDPOINT_NOT_FOUND), "IMC route not found");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_HANDLE_STALE), "IMC handle is stale");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_UNSUPPORTED),
                 "IMC operation is unsupported by the current runtime");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_API_MISMATCH),
                 "IMC payload type or layout is incompatible");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_PROVIDER_UNLOADED), "IMC provider was unloaded");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_OBJECT_INVALID),
                 "IMC object reference is invalid or expired");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_SCHEMA_MISMATCH),
                 "IMC payload schema does not match the route");
    EXPECT_STREQ(BML_GetErrorString(BML_ERROR_IMC_TARGET_EXECUTION_FAILED),
                 "IMC provider callback execution failed");
}
