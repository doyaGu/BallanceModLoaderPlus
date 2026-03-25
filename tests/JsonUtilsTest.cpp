#include <gtest/gtest.h>

#include <string>

#include "JsonUtils.h"

namespace {
    TEST(JsonUtilsTest, ParsesObjectsArraysAndNulls) {
        const std::string payload = R"JSON({
  "name": "foo",
  "enabled": true,
  "count": 3,
  "items": ["a", "b"],
  "package_id": null
})JSON";

        std::string error;
        auto document = utils::JsonDocument::Parse(payload, error);
        ASSERT_TRUE(document.IsValid()) << error;

        yyjson_val *root = document.Root();
        ASSERT_NE(root, nullptr);
        EXPECT_TRUE(yyjson_is_obj(root));
        EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "name")), "foo");
        EXPECT_TRUE(yyjson_get_bool(yyjson_obj_get(root, "enabled")));
        EXPECT_EQ(yyjson_get_int(yyjson_obj_get(root, "count")), 3);

        yyjson_val *items = yyjson_obj_get(root, "items");
        ASSERT_TRUE(yyjson_is_arr(items));
        EXPECT_EQ(yyjson_arr_size(items), 2u);
        EXPECT_TRUE(yyjson_is_null(yyjson_obj_get(root, "package_id")));
    }

    TEST(JsonUtilsTest, BuildsPrettyPrintedJson) {
        utils::MutableJsonDocument document;
        yyjson_mut_val *root = document.CreateObject();
        ASSERT_NE(root, nullptr);
        document.SetRoot(root);
        ASSERT_TRUE(document.AddString(root, "name", "foo"));
        ASSERT_TRUE(document.AddInt(root, "count", 7));
        ASSERT_TRUE(document.AddNull(root, "package_id"));

        yyjson_mut_val *items = document.CreateArray();
        ASSERT_NE(items, nullptr);
        ASSERT_TRUE(document.AddString(items, "first"));
        ASSERT_TRUE(document.AddString(items, "second"));
        ASSERT_TRUE(document.AddValue(root, "items", items));

        std::string error;
        const std::string json = document.Write(true, error);
        ASSERT_FALSE(json.empty()) << error;
        EXPECT_NE(json.find("\n"), std::string::npos);
        EXPECT_NE(json.find("\"package_id\": null"), std::string::npos);

        auto roundTrip = utils::JsonDocument::Parse(json, error);
        ASSERT_TRUE(roundTrip.IsValid()) << error;
        EXPECT_EQ(yyjson_arr_size(yyjson_obj_get(roundTrip.Root(), "items")), 2u);
    }

    TEST(JsonUtilsTest, ReportsParseErrors) {
        std::string error;
        auto document = utils::JsonDocument::Parse("{ broken json", error);
        EXPECT_FALSE(document.IsValid());
        EXPECT_FALSE(error.empty());
    }
} // namespace
