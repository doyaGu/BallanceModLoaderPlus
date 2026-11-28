#include <string>

#include <gtest/gtest.h>

#include "Api/Behavior/Text.h"

namespace BML::Api::Behavior {
namespace {

constexpr unsigned Western = 1252;
constexpr unsigned Chinese = 936;

TEST(BehaviorText, KeepsAsciiUnchanged) {
    std::string storage;
    std::string native;
    EXPECT_TRUE(EncodeNativeText("Level_01.NMO", native, Chinese));
    EXPECT_EQ(native, "Level_01.NMO");
    EXPECT_EQ(Utf8Text("Level_01.NMO", storage, Chinese), "Level_01.NMO");
}

TEST(BehaviorText, RoundTripsInTheSelectedRuntime) {
    const std::string level = "\xe5\x85\xb3\xe5\x8d\xa1";
    std::string native;
    ASSERT_TRUE(EncodeNativeText(level, native, Chinese));
#if BML_RUNTIME_SDL
    EXPECT_EQ(native, level);
#else
    EXPECT_EQ(native, "\xb9\xd8\xbf\xa8");
#endif
    std::string storage;
    EXPECT_EQ(Utf8Text(native, storage, Chinese), level);

    const std::string cafe = "caf\xc3\xa9";
    ASSERT_TRUE(EncodeNativeText(cafe, native, Western));
#if BML_RUNTIME_SDL
    EXPECT_EQ(native, cafe);
#else
    EXPECT_EQ(native, "caf\xe9");
#endif
    EXPECT_EQ(Utf8Text(native, storage, Western), cafe);
}

TEST(BehaviorText, RejectsMalformedUtf8InsteadOfDecodingItAsAcp) {
    std::string storage = "stale conversion";
#if BML_RUNTIME_SDL
    EXPECT_TRUE(Utf8Text("caf\xe9", storage, Western).empty());
    EXPECT_TRUE(storage.empty());
    EXPECT_TRUE(Utf8Text("\xb9\xd8\xbf\xa8", storage, Chinese).empty());
#else
    EXPECT_EQ(Utf8Text("caf\xe9", storage, Western), "caf\xc3\xa9");
    EXPECT_EQ(Utf8Text("\xb9\xd8\xbf\xa8", storage, Chinese), "\xe5\x85\xb3\xe5\x8d\xa1");
#endif
    EXPECT_FALSE(IsUtf8("\xc0\x80", 2));
    EXPECT_FALSE(IsUtf8("\xed\xa0\x80", 3));
    EXPECT_FALSE(IsUtf8("\xf4\x90\x80\x80", 4));
}

TEST(BehaviorText, RejectsInputTheEngineCannotRepresent) {
    const std::string text = "\xe5\x85\xb3";
    std::string native = "previous result";
#if BML_RUNTIME_SDL
    EXPECT_TRUE(EncodeNativeText(text, native, Western));
    EXPECT_EQ(native, text);
#else
    EXPECT_FALSE(EncodeNativeText(text, native, Western));
    EXPECT_EQ(native, "previous result");
#endif
    EXPECT_FALSE(EncodeNativeText("\xc0\x80", native, Western));
}

TEST(BehaviorText, InterpretsOverlappingSequencesByRuntime) {
    std::string storage;
    const std::string bytes = "\xc3\xa9";
#if BML_RUNTIME_SDL
    EXPECT_EQ(Utf8Text(bytes, storage, Chinese), bytes);
#else
    EXPECT_EQ(Utf8Text(bytes, storage, Chinese), "\xe8\x8c\x85");
    std::string native;
    ASSERT_TRUE(EncodeNativeText("\xe8\x8c\x85", native, Chinese));
    EXPECT_EQ(native, bytes);
#endif
}

} // namespace
} // namespace BML::Api::Behavior
