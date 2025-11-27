/**
 * @file StructSizeTest.cpp
 * @brief Tests to verify struct_size field placement and initialization
 * 
 * Task 1.2: Ensure all public structures have struct_size as the first field
 * and provide proper initialization macros.
 * 
 * @since 0.4.0
 */

#include <gtest/gtest.h>
#include <cstddef>

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_imc.h"
#include "bml_config.h"
#include "bml_logging.h"
#include "bml_sync.h"
#include "bml_capabilities.h"
#include "bml_api_tracing.h"
#include "bml_profiling.h"

/**
 * @brief Test suite for struct_size field validation (Task 1.2)
 * 
 * Verifies that:
 * 1. All structures have struct_size as the first field (offset 0)
 * 2. Initialization macros produce correct struct_size values
 */
class StructSizeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ========================================================================
 * Task 1.2: Verify struct_size is at offset 0
 * ======================================================================== */

TEST_F(StructSizeTest, BML_Version_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_Version, struct_size), 0u)
        << "BML_Version.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_ErrorInfo_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_ErrorInfo, struct_size), 0u)
        << "BML_ErrorInfo.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_ImcMessage_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_ImcMessage, struct_size), 0u)
        << "BML_ImcMessage.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_ImcBuffer_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_ImcBuffer, struct_size), 0u)
        << "BML_ImcBuffer.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_SyncCaps_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_SyncCaps, struct_size), 0u)
        << "BML_SyncCaps.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_VersionRequirement_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_VersionRequirement, struct_size), 0u)
        << "BML_VersionRequirement.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_ApiDescriptor_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_ApiDescriptor, struct_size), 0u)
        << "BML_ApiDescriptor.struct_size must be the first field";
}

TEST_F(StructSizeTest, BML_ApiStats_StructSizeOffset) {
    EXPECT_EQ(offsetof(BML_ApiStats, struct_size), 0u)
        << "BML_ApiStats.struct_size must be the first field";
}

/* ========================================================================
 * Task 1.2: Verify initialization macros set correct struct_size
 * ======================================================================== */

TEST_F(StructSizeTest, BML_VERSION_INIT_StructSize) {
    BML_Version v = BML_VERSION_INIT(1, 2, 3);
    EXPECT_EQ(v.struct_size, sizeof(BML_Version))
        << "BML_VERSION_INIT must set correct struct_size";
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
}

TEST_F(StructSizeTest, BML_ERROR_INFO_INIT_StructSize) {
    BML_ErrorInfo info = BML_ERROR_INFO_INIT;
    EXPECT_EQ(info.struct_size, sizeof(BML_ErrorInfo))
        << "BML_ERROR_INFO_INIT must set correct struct_size";
    EXPECT_EQ(info.result_code, 0);
    EXPECT_EQ(info.message, nullptr);
}

TEST_F(StructSizeTest, BML_IMC_MESSAGE_INIT_StructSize) {
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    EXPECT_EQ(msg.struct_size, sizeof(BML_ImcMessage))
        << "BML_IMC_MESSAGE_INIT must set correct struct_size";
}

TEST_F(StructSizeTest, BML_IMC_BUFFER_INIT_StructSize) {
    BML_ImcBuffer buf = BML_IMC_BUFFER_INIT;
    EXPECT_EQ(buf.struct_size, sizeof(BML_ImcBuffer))
        << "BML_IMC_BUFFER_INIT must set correct struct_size";
    EXPECT_EQ(buf.data, nullptr);
    EXPECT_EQ(buf.size, 0u);
}

TEST_F(StructSizeTest, BML_API_STATS_INIT_StructSize) {
    BML_ApiStats stats = BML_API_STATS_INIT;
    EXPECT_EQ(stats.struct_size, sizeof(BML_ApiStats))
        << "BML_API_STATS_INIT must set correct struct_size";
}

TEST_F(StructSizeTest, BML_API_DESCRIPTOR_INIT_StructSize) {
    BML_ApiDescriptor desc = BML_API_DESCRIPTOR_INIT;
    EXPECT_EQ(desc.struct_size, sizeof(BML_ApiDescriptor))
        << "BML_API_DESCRIPTOR_INIT must set correct struct_size";
}

TEST_F(StructSizeTest, BML_VERSION_REQUIREMENT_INIT_StructSize) {
    BML_VersionRequirement req = BML_VERSION_REQUIREMENT_INIT(1, 0, 0);
    EXPECT_EQ(req.struct_size, sizeof(BML_VersionRequirement))
        << "BML_VERSION_REQUIREMENT_INIT must set correct struct_size";
}

/* ========================================================================
 * Task 1.4: Verify enum sizes are 32-bit
 * ======================================================================== */

TEST_F(StructSizeTest, EnumSizes_Are32Bit) {
    // All enums with _FORCE_32BIT marker should be 4 bytes
    EXPECT_EQ(sizeof(BML_ThreadingModel), sizeof(int32_t))
        << "BML_ThreadingModel must be 32-bit";
    EXPECT_EQ(sizeof(BML_LogSeverity), sizeof(int32_t))
        << "BML_LogSeverity must be 32-bit";
    EXPECT_EQ(sizeof(BML_ConfigType), sizeof(int32_t))
        << "BML_ConfigType must be 32-bit";
    EXPECT_EQ(sizeof(BML_FutureState), sizeof(int32_t))
        << "BML_FutureState must be 32-bit";
    EXPECT_EQ(sizeof(BML_SyncCapabilityFlags), sizeof(int32_t))
        << "BML_SyncCapabilityFlags must be 32-bit";
}

/* ========================================================================
 * Test bmlMakeVersion helper function
 * ======================================================================== */

TEST_F(StructSizeTest, bmlMakeVersion_SetsStructSize) {
    BML_Version v = bmlMakeVersion(2, 5, 10);
    EXPECT_EQ(v.struct_size, sizeof(BML_Version))
        << "bmlMakeVersion must set correct struct_size";
    EXPECT_EQ(v.major, 2);
    EXPECT_EQ(v.minor, 5);
    EXPECT_EQ(v.patch, 10);
    EXPECT_EQ(v.reserved, 0);
}

TEST_F(StructSizeTest, bmlVersionToUint_Conversion) {
    BML_Version v = bmlMakeVersion(1, 2, 3);
    uint32_t packed = bmlVersionToUint(&v);
    // Expected: (1 << 16) | (2 << 8) | 3 = 0x010203
    EXPECT_EQ(packed, 0x010203u);
}

/* ========================================================================
 * Test BML_SUCCEEDED and BML_FAILED macros
 * ======================================================================== */

TEST_F(StructSizeTest, ResultMacros) {
    EXPECT_TRUE(BML_SUCCEEDED(BML_RESULT_OK));
    EXPECT_TRUE(BML_SUCCEEDED(0));
    EXPECT_TRUE(BML_SUCCEEDED(1));
    
    EXPECT_TRUE(BML_FAILED(BML_RESULT_FAIL));
    EXPECT_TRUE(BML_FAILED(BML_RESULT_INVALID_ARGUMENT));
    EXPECT_TRUE(BML_FAILED(-1));
    
    EXPECT_FALSE(BML_FAILED(BML_RESULT_OK));
    EXPECT_FALSE(BML_SUCCEEDED(BML_RESULT_FAIL));
}
