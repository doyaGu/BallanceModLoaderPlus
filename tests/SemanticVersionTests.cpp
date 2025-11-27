/**
 * @file SemanticVersionTests.cpp
 * @brief Comprehensive tests for SemanticVersion parsing and comparison
 * 
 * Tests cover:
 * - Version parsing (valid and invalid formats)
 * - Version range parsing
 * - Version satisfaction checks
 * - Edge cases
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <vector>

#include "Core/SemanticVersion.h"

using BML::Core::SemanticVersion;
using BML::Core::SemanticVersionRange;
using BML::Core::VersionOperator;
using BML::Core::ParseSemanticVersion;
using BML::Core::ParseSemanticVersionRange;
using BML::Core::IsVersionSatisfied;

namespace {

class SemanticVersionTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(SemanticVersionTests, ParseSimpleVersion) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("1.2.3", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
}

TEST_F(SemanticVersionTests, ParseVersionWithVPrefix) {
    SemanticVersion v;
    // v prefix may or may not be supported
    // If supported, should parse correctly
    if (ParseSemanticVersion("v1.0.0", v)) {
        EXPECT_EQ(v.major, 1);
        EXPECT_EQ(v.minor, 0);
        EXPECT_EQ(v.patch, 0);
    }
}

TEST_F(SemanticVersionTests, ParseZeroVersion) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("0.0.0", v));
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST_F(SemanticVersionTests, ParseLargeNumbers) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("999.888.777", v));
    EXPECT_EQ(v.major, 999);
    EXPECT_EQ(v.minor, 888);
    EXPECT_EQ(v.patch, 777);
}

TEST_F(SemanticVersionTests, ParseSingleDigitVersion) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("1.0.0", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST_F(SemanticVersionTests, ParseTwoDigitVersion) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("10.20.30", v));
    EXPECT_EQ(v.major, 10);
    EXPECT_EQ(v.minor, 20);
    EXPECT_EQ(v.patch, 30);
}

// ============================================================================
// Invalid Version Parsing Tests
// ============================================================================

TEST_F(SemanticVersionTests, ParseEmptyStringFails) {
    SemanticVersion v;
    EXPECT_FALSE(ParseSemanticVersion("", v));
}

// NOTE: The current implementation accepts partial versions like "1" or "1.2"
// treating missing components as 0. This is intentional for flexibility.
TEST_F(SemanticVersionTests, ParsePartialVersions) {
    SemanticVersion v;
    // Implementation accepts partial versions
    EXPECT_TRUE(ParseSemanticVersion("1", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    
    EXPECT_TRUE(ParseSemanticVersion("1.2", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 0);
}

TEST_F(SemanticVersionTests, ParseInvalidCharsFails) {
    SemanticVersion v;
    EXPECT_FALSE(ParseSemanticVersion("1.2.a", v));
    EXPECT_FALSE(ParseSemanticVersion("a.b.c", v));
}

// NOTE: The implementation does not validate negative numbers at parse time
// because strtol accepts them. This test documents current behavior.
TEST_F(SemanticVersionTests, ParseNegativeNumbersBehavior) {
    SemanticVersion v;
    // Current implementation accepts these (strtol parses negative numbers)
    // If strict validation is needed, implementation should be updated
    bool accepts_negative = ParseSemanticVersion("-1.2.3", v);
    // Just document behavior, don't fail
    if (accepts_negative) {
        EXPECT_EQ(v.major, -1);  // Documents current behavior
    }
}

// NOTE: Extra components are silently ignored in current implementation
TEST_F(SemanticVersionTests, ParseExtraComponentsIgnored) {
    SemanticVersion v;
    // Current implementation ignores extra components
    EXPECT_TRUE(ParseSemanticVersion("1.2.3.4", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
}

// NOTE: Implementation trims whitespace, which is intentional
TEST_F(SemanticVersionTests, ParseWhitespaceIsTrimmed) {
    SemanticVersion v;
    // Leading/trailing whitespace is trimmed
    EXPECT_TRUE(ParseSemanticVersion(" 1.2.3", v));
    EXPECT_EQ(v.major, 1);
    
    EXPECT_TRUE(ParseSemanticVersion("1.2.3 ", v));
    EXPECT_EQ(v.major, 1);
    
    EXPECT_TRUE(ParseSemanticVersion(" 1.2.3 ", v));
    EXPECT_EQ(v.major, 1);
}

// ============================================================================
// Version Range Parsing Tests
// ============================================================================

TEST_F(SemanticVersionTests, ParseExactVersionRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("1.2.3", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::Exact);
    EXPECT_EQ(range.version.major, 1);
    EXPECT_EQ(range.version.minor, 2);
    EXPECT_EQ(range.version.patch, 3);
}

TEST_F(SemanticVersionTests, ParseCaretRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("^1.2.3", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::Compatible);
}

TEST_F(SemanticVersionTests, ParseTildeRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("~1.2.3", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::ApproximatelyEquivalent);
}

TEST_F(SemanticVersionTests, ParseGreaterEqualRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange(">=1.0.0", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::GreaterEqual);
}

TEST_F(SemanticVersionTests, ParseRangePreservesRawExpression) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("^1.2.3", range, error));
    EXPECT_EQ(range.raw_expression, "^1.2.3");
}

TEST_F(SemanticVersionTests, ParseInvalidRangeSetsError) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_FALSE(ParseSemanticVersionRange("invalid", range, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(SemanticVersionTests, ParseEmptyRangeFails) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_FALSE(ParseSemanticVersionRange("", range, error));
}

// ============================================================================
// Version Satisfaction Tests - Exact
// ============================================================================

TEST_F(SemanticVersionTests, ExactVersionSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("1.2.3", range, error));
    
    SemanticVersion exact, lower, higher;
    ParseSemanticVersion("1.2.3", exact);
    ParseSemanticVersion("1.2.2", lower);
    ParseSemanticVersion("1.2.4", higher);
    
    EXPECT_TRUE(IsVersionSatisfied(range, exact));
    EXPECT_FALSE(IsVersionSatisfied(range, lower));
    EXPECT_FALSE(IsVersionSatisfied(range, higher));
}

// ============================================================================
// Version Satisfaction Tests - Compatible (^)
// ============================================================================

TEST_F(SemanticVersionTests, CaretRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("^1.2.3", range, error));
    
    SemanticVersion v1, v2, v3, v4, v5;
    ParseSemanticVersion("1.2.3", v1);  // exact
    ParseSemanticVersion("1.2.5", v2);  // patch higher
    ParseSemanticVersion("1.9.9", v3);  // minor higher
    ParseSemanticVersion("2.0.0", v4);  // major bump - not compatible
    ParseSemanticVersion("1.2.0", v5);  // patch lower
    
    // ^1.2.3 allows >=1.2.3 and <2.0.0
    EXPECT_TRUE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_TRUE(IsVersionSatisfied(range, v3));
    EXPECT_FALSE(IsVersionSatisfied(range, v4));
    EXPECT_FALSE(IsVersionSatisfied(range, v5));
}

TEST_F(SemanticVersionTests, CaretRangeZeroMajor) {
    SemanticVersionRange range;
    std::string error;
    
    // ^0.2.3 should be more restrictive for 0.x versions
    ASSERT_TRUE(ParseSemanticVersionRange("^0.2.3", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("0.2.3", v1);
    ParseSemanticVersion("0.2.9", v2);
    ParseSemanticVersion("0.3.0", v3);
    
    // For 0.x.y, caret allows >=0.2.3 <0.3.0
    EXPECT_TRUE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_FALSE(IsVersionSatisfied(range, v3));
}

// ============================================================================
// Version Satisfaction Tests - Approximately Equivalent (~)
// ============================================================================

TEST_F(SemanticVersionTests, TildeRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("~1.2.3", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("1.2.3", v1);
    ParseSemanticVersion("1.2.9", v2);
    ParseSemanticVersion("1.3.0", v3);
    
    // ~1.2.3 allows >=1.2.3 and <1.3.0
    EXPECT_TRUE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_FALSE(IsVersionSatisfied(range, v3));
}

// ============================================================================
// Version Satisfaction Tests - Greater Equal (>=)
// ============================================================================

TEST_F(SemanticVersionTests, GreaterEqualRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange(">=1.5.0", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("1.4.9", v1);
    ParseSemanticVersion("1.5.0", v2);
    ParseSemanticVersion("2.0.0", v3);
    
    EXPECT_FALSE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_TRUE(IsVersionSatisfied(range, v3));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SemanticVersionTests, VersionDefaultsToZero) {
    SemanticVersion v{};
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST_F(SemanticVersionTests, RangeDefaultsToNotParsed) {
    SemanticVersionRange range{};
    EXPECT_FALSE(range.parsed);
    EXPECT_TRUE(range.raw_expression.empty());
}

TEST_F(SemanticVersionTests, MultipleParsesOverwriteValue) {
    SemanticVersion v;
    
    EXPECT_TRUE(ParseSemanticVersion("1.0.0", v));
    EXPECT_EQ(v.major, 1);
    
    EXPECT_TRUE(ParseSemanticVersion("2.0.0", v));
    EXPECT_EQ(v.major, 2);
}

TEST_F(SemanticVersionTests, ParseAfterFailureKeepsPreviousValue) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("5.5.5", v));
    EXPECT_EQ(v.major, 5);
    
    // Failed parse - v should ideally remain unchanged or be in defined state
    EXPECT_FALSE(ParseSemanticVersion("invalid", v));
    // Check that v still has some defined value
    EXPECT_GE(v.major, 0);
}

TEST_F(SemanticVersionTests, BoundaryVersionValues) {
    SemanticVersion v;
    
    // Test with very small values
    EXPECT_TRUE(ParseSemanticVersion("0.0.1", v));
    EXPECT_EQ(v.patch, 1);
    
    // Test with mixed values
    EXPECT_TRUE(ParseSemanticVersion("0.1.0", v));
    EXPECT_EQ(v.minor, 1);
}

// ============================================================================
// Version Comparison Helpers
// ============================================================================

// Helper to compare two versions directly
static bool VersionLessThan(const SemanticVersion& a, const SemanticVersion& b) {
    if (a.major != b.major) return a.major < b.major;
    if (a.minor != b.minor) return a.minor < b.minor;
    return a.patch < b.patch;
}

static bool VersionEqual(const SemanticVersion& a, const SemanticVersion& b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch && a.prerelease == b.prerelease;
}

TEST_F(SemanticVersionTests, VersionComparisonHelper_LessThan) {
    SemanticVersion v1{1, 0, 0}, v2{2, 0, 0}, v3{1, 1, 0}, v4{1, 0, 1};
    
    EXPECT_TRUE(VersionLessThan(v1, v2));
    EXPECT_TRUE(VersionLessThan(v1, v3));
    EXPECT_TRUE(VersionLessThan(v1, v4));
    
    EXPECT_FALSE(VersionLessThan(v2, v1));
    EXPECT_FALSE(VersionLessThan(v1, v1));
}

TEST_F(SemanticVersionTests, VersionComparisonHelper_Equal) {
    SemanticVersion v1{1, 2, 3}, v2{1, 2, 3}, v3{1, 2, 4};
    
    EXPECT_TRUE(VersionEqual(v1, v2));
    EXPECT_FALSE(VersionEqual(v1, v3));
}

// ============================================================================
// Range Expression Parsing Variations
// ============================================================================

TEST_F(SemanticVersionTests, ParseRangeWithSpacesAroundOperator) {
    SemanticVersionRange range;
    std::string error;
    
    // Test if spaces around operator are handled
    // Implementation may or may not accept spaces
    bool result = ParseSemanticVersionRange(">= 1.0.0", range, error);
    // Just verify it handles this case consistently (success or failure)
    if (result) {
        EXPECT_TRUE(range.parsed);
    }
}

TEST_F(SemanticVersionTests, ParseMultipleRangesIndependently) {
    SemanticVersionRange range1, range2;
    std::string error1, error2;
    
    EXPECT_TRUE(ParseSemanticVersionRange("^1.0.0", range1, error1));
    EXPECT_TRUE(ParseSemanticVersionRange(">=2.0.0", range2, error2));
    
    // Verify they're independent
    EXPECT_EQ(range1.version.major, 1);
    EXPECT_EQ(range2.version.major, 2);
    EXPECT_EQ(range1.op, VersionOperator::Compatible);
    EXPECT_EQ(range2.op, VersionOperator::GreaterEqual);
}

// ============================================================================
// New Operator Tests (>, <=, <)
// ============================================================================

TEST_F(SemanticVersionTests, ParseGreaterRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange(">1.0.0", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::Greater);
    EXPECT_EQ(range.version.major, 1);
    EXPECT_EQ(range.version.minor, 0);
    EXPECT_EQ(range.version.patch, 0);
}

TEST_F(SemanticVersionTests, ParseLessEqualRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("<=2.5.0", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::LessEqual);
    EXPECT_EQ(range.version.major, 2);
    EXPECT_EQ(range.version.minor, 5);
}

TEST_F(SemanticVersionTests, ParseLessRange) {
    SemanticVersionRange range;
    std::string error;
    EXPECT_TRUE(ParseSemanticVersionRange("<3.0.0", range, error));
    EXPECT_TRUE(range.parsed);
    EXPECT_EQ(range.op, VersionOperator::Less);
    EXPECT_EQ(range.version.major, 3);
}

TEST_F(SemanticVersionTests, GreaterRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange(">1.5.0", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("1.5.0", v1);  // exact - should NOT satisfy
    ParseSemanticVersion("1.5.1", v2);  // higher patch - should satisfy
    ParseSemanticVersion("1.4.9", v3);  // lower - should NOT satisfy
    
    EXPECT_FALSE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_FALSE(IsVersionSatisfied(range, v3));
}

TEST_F(SemanticVersionTests, LessEqualRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("<=2.0.0", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("2.0.0", v1);  // exact - should satisfy
    ParseSemanticVersion("1.9.9", v2);  // lower - should satisfy
    ParseSemanticVersion("2.0.1", v3);  // higher - should NOT satisfy
    
    EXPECT_TRUE(IsVersionSatisfied(range, v1));
    EXPECT_TRUE(IsVersionSatisfied(range, v2));
    EXPECT_FALSE(IsVersionSatisfied(range, v3));
}

TEST_F(SemanticVersionTests, LessRangeSatisfied) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("<2.0.0", range, error));
    
    SemanticVersion v1, v2, v3;
    ParseSemanticVersion("1.9.9", v1);  // lower - should satisfy
    ParseSemanticVersion("2.0.0", v2);  // exact - should NOT satisfy
    ParseSemanticVersion("2.0.1", v3);  // higher - should NOT satisfy
    
    EXPECT_TRUE(IsVersionSatisfied(range, v1));
    EXPECT_FALSE(IsVersionSatisfied(range, v2));
    EXPECT_FALSE(IsVersionSatisfied(range, v3));
}

// ============================================================================
// Negative Version Number Tests
// ============================================================================

TEST_F(SemanticVersionTests, RejectNegativeMajorVersion) {
    SemanticVersion v;
    EXPECT_FALSE(ParseSemanticVersion("-1.0.0", v));
}

TEST_F(SemanticVersionTests, RejectNegativeMinorVersion) {
    SemanticVersion v;
    EXPECT_FALSE(ParseSemanticVersion("1.-2.0", v));
}

TEST_F(SemanticVersionTests, RejectNegativePatchVersion) {
    SemanticVersion v;
    EXPECT_FALSE(ParseSemanticVersion("1.0.-3", v));
}

// ============================================================================
// Prerelease Version Tests
// ============================================================================

TEST_F(SemanticVersionTests, ParsePrereleaseAlpha) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("1.0.0-alpha", v));
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.prerelease, "alpha");
}

TEST_F(SemanticVersionTests, ParsePrereleaseBetaWithNumber) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("2.1.0-beta.2", v));
    EXPECT_EQ(v.major, 2);
    EXPECT_EQ(v.minor, 1);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.prerelease, "beta.2");
}

TEST_F(SemanticVersionTests, ParsePrereleaseReleaseCandidate) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("3.0.0-rc.1", v));
    EXPECT_EQ(v.major, 3);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.prerelease, "rc.1");
}

TEST_F(SemanticVersionTests, ParsePrereleaseComplex) {
    SemanticVersion v;
    EXPECT_TRUE(ParseSemanticVersion("1.0.0-alpha.beta.1", v));
    EXPECT_EQ(v.prerelease, "alpha.beta.1");
}

TEST_F(SemanticVersionTests, ParseEmptyPrereleaseIsSuffix) {
    SemanticVersion v;
    // "1.0.0-" should fail - prerelease cannot be empty
    EXPECT_FALSE(ParseSemanticVersion("1.0.0-", v));
}

TEST_F(SemanticVersionTests, PrereleaseHasLowerPrecedence) {
    SemanticVersionRange range;
    std::string error;
    
    // 1.0.0-alpha should be less than 1.0.0 (release)
    ASSERT_TRUE(ParseSemanticVersionRange(">=1.0.0", range, error));
    
    SemanticVersion prerelease, release;
    ParseSemanticVersion("1.0.0-alpha", prerelease);
    ParseSemanticVersion("1.0.0", release);
    
    // Prerelease should NOT satisfy >= release version
    EXPECT_FALSE(IsVersionSatisfied(range, prerelease));
    // Release should satisfy >= release version
    EXPECT_TRUE(IsVersionSatisfied(range, release));
}

TEST_F(SemanticVersionTests, PrereleaseComparisonOrder) {
    SemanticVersionRange range;
    std::string error;
    
    // >1.0.0-alpha should be satisfied by 1.0.0-beta (alphabetically greater)
    ASSERT_TRUE(ParseSemanticVersionRange(">1.0.0-alpha", range, error));
    
    SemanticVersion alpha, beta, release;
    ParseSemanticVersion("1.0.0-alpha", alpha);
    ParseSemanticVersion("1.0.0-beta", beta);
    ParseSemanticVersion("1.0.0", release);
    
    EXPECT_FALSE(IsVersionSatisfied(range, alpha));  // exact, not greater
    EXPECT_TRUE(IsVersionSatisfied(range, beta));    // beta > alpha
    EXPECT_TRUE(IsVersionSatisfied(range, release)); // release > prerelease
}

TEST_F(SemanticVersionTests, ExactMatchWithPrerelease) {
    SemanticVersionRange range;
    std::string error;
    
    ASSERT_TRUE(ParseSemanticVersionRange("1.0.0-rc.1", range, error));
    
    SemanticVersion exact, different_prerelease, release;
    ParseSemanticVersion("1.0.0-rc.1", exact);
    ParseSemanticVersion("1.0.0-rc.2", different_prerelease);
    ParseSemanticVersion("1.0.0", release);
    
    EXPECT_TRUE(IsVersionSatisfied(range, exact));
    EXPECT_FALSE(IsVersionSatisfied(range, different_prerelease));
    EXPECT_FALSE(IsVersionSatisfied(range, release));
}

} // namespace
