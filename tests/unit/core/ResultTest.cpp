#include "BML/Result.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace {

struct Diagnostic {
    std::string message;
};

struct CopyOnly {
    explicit CopyOnly(int value) : value(value) {}
    CopyOnly(const CopyOnly &) = default;
    CopyOnly(CopyOnly &&) = delete;

    int value;
};

static_assert(std::is_same_v<decltype(BML::Result<int>().GetStatus()),
                             const std::monostate &>);

TEST(Result, PlainValueAndFailure) {
    auto success = BML::Result<int>::Success(42);
    ASSERT_TRUE(success);
    EXPECT_TRUE(success.HasValue());
    EXPECT_EQ(success.Code(), BML_OK);
    EXPECT_EQ(success.Value(), 42);

    auto failure = BML::Result<int>::Failure(BML_ERROR_NOT_FOUND);
    EXPECT_FALSE(failure);
    EXPECT_FALSE(failure.HasValue());
    EXPECT_EQ(failure.Code(), BML_ERROR_NOT_FOUND);
    EXPECT_THROW((void) failure.Value(), std::bad_optional_access);
}

TEST(Result, DiagnosticSurvivesValueConsumption) {
    using DetailedResult = BML::Result<std::unique_ptr<int>, Diagnostic>;
    auto result = DetailedResult::Success(std::make_unique<int>(42), {"retained"});

    ASSERT_TRUE(result);
    auto value = result.Take();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Code(), BML_OK);
    EXPECT_EQ(result.GetStatus().message, "retained");
    EXPECT_THROW((void) result.Take(), std::bad_optional_access);

    auto failure = DetailedResult::Failure(BML_ERROR_FAIL, {"failed"});
    EXPECT_EQ(failure.Code(), BML_ERROR_FAIL);
    EXPECT_EQ(failure.GetStatus().message, "failed");
}

TEST(Result, CopyOnlyValueCanBeConsumed) {
    auto result = BML::Result<CopyOnly>::Success(CopyOnly{42});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.Value().value, 42);

    CopyOnly value = result.Take();
    EXPECT_EQ(value.value, 42);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Code(), BML_OK);
}

TEST(Result, VoidUsesTheSameStatusRules) {
    auto success = BML::Result<void>::Success();
    EXPECT_TRUE(success);
    EXPECT_EQ(success.Code(), BML_OK);

    auto failure = BML::Result<void, Diagnostic>::Failure(
        BML_ERROR_INVALID_PARAMETER, {"invalid"});
    EXPECT_FALSE(failure);
    EXPECT_EQ(failure.Code(), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(failure.GetStatus().message, "invalid");
}

TEST(Result, FailureRequiresAnErrorCode) {
    auto value = BML::Result<int>::Failure(BML_OK);
    auto empty = BML::Result<void>::Failure(BML_OK);
    auto positive = BML::Result<void>::Failure(BML_OK + 1);

    EXPECT_FALSE(value);
    EXPECT_FALSE(empty);
    EXPECT_FALSE(positive);
    EXPECT_EQ(value.Code(), BML_ERROR_FAIL);
    EXPECT_EQ(empty.Code(), BML_ERROR_FAIL);
    EXPECT_EQ(positive.Code(), BML_ERROR_FAIL);
}

} // namespace
