#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "Behavior/Value.h"

namespace BML::Behavior::Internal {
namespace {

static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);

TEST(BehaviorValue, OwnsRawBytes) {
    const CKGUID type(0x11111111u, 0x22222222u);
    std::array<std::byte, 4> source{
        std::byte{0x10}, std::byte{0x20},
        std::byte{0x30}, std::byte{0x40}};

    const Value value = Value::Raw(type, source.data(), source.size());
    source.fill(std::byte{0xff});

    EXPECT_EQ(value.Kind(), ValueKind::Raw);
    EXPECT_EQ(value.Type(), type);
    EXPECT_EQ(value.Bytes(),
              (std::vector<std::byte>{
                  std::byte{0x10}, std::byte{0x20},
                  std::byte{0x30}, std::byte{0x40}}));
}

TEST(BehaviorValue, OwnsText) {
    const CKGUID type(0x33333333u, 0x44444444u);
    std::string source = "Ballance";

    const Value value = Value::Text(type, source);
    source.assign("changed");

    EXPECT_EQ(value.Kind(), ValueKind::Text);
    EXPECT_EQ(value.Type(), type);
    EXPECT_EQ(value.StringValue(), "Ballance");
    EXPECT_TRUE(value.Bytes().empty());
}

TEST(BehaviorValue, RepresentsTypedNullWithoutWorldState) {
    const CKGUID type(0x55555555u, 0x66666666u);
    const Value value = Value::Null(type);

    EXPECT_EQ(value.Kind(), ValueKind::Null);
    EXPECT_EQ(value.Type(), type);
    EXPECT_TRUE(value.IsNull());
    EXPECT_TRUE(value.Bytes().empty());
    EXPECT_TRUE(value.StringValue().empty());
}

} // namespace
} // namespace BML::Behavior::Internal
