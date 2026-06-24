#include "AngelScript/ScriptStateBag.h"

#include <gtest/gtest.h>

using BML::ScriptStateBag;
using BML::ScriptStateValueType;

TEST(ScriptStateBagTest, StoresTypedValuesAndDefaults) {
    ScriptStateBag bag;
    bag.SetBool("ready", true);
    bag.SetInt("score", 42);
    bag.SetFloat("speed", 2.5f);
    bag.SetString("level", "Level_01");

    EXPECT_TRUE(bag.Has("ready"));
    EXPECT_EQ(ScriptStateValueType::Bool, bag.GetType("ready"));
    EXPECT_EQ(ScriptStateValueType::Int, bag.GetType("score"));
    EXPECT_EQ(ScriptStateValueType::Float, bag.GetType("speed"));
    EXPECT_EQ(ScriptStateValueType::String, bag.GetType("level"));
    EXPECT_EQ(ScriptStateValueType::Empty, bag.GetType("missing"));
    EXPECT_TRUE(bag.GetBool("ready", false));
    EXPECT_EQ(42, bag.GetInt("score", 0));
    EXPECT_FLOAT_EQ(2.5f, bag.GetFloat("speed", 0.0f));
    EXPECT_EQ("Level_01", bag.GetString("level", ""));

    EXPECT_FALSE(bag.GetBool("score", false));
    EXPECT_EQ(7, bag.GetInt("ready", 7));
    EXPECT_FLOAT_EQ(1.25f, bag.GetFloat("level", 1.25f));
    EXPECT_EQ("fallback", bag.GetString("missing", "fallback"));
}

TEST(ScriptStateBagTest, EnumeratesRemovesClearsAndClones) {
    ScriptStateBag bag;
    bag.SetString("b", "two");
    bag.SetInt("a", 1);

    EXPECT_EQ(2, bag.GetCount());
    EXPECT_EQ("a", bag.GetKey(0));
    EXPECT_EQ("b", bag.GetKey(1));
    EXPECT_EQ("", bag.GetKey(-1));
    EXPECT_EQ("", bag.GetKey(2));

    BML::ScriptStateBagHandle clone(bag.Clone());
    ASSERT_TRUE(clone);
    EXPECT_EQ(2, clone->GetCount());
    EXPECT_EQ(1, clone->GetInt("a", 0));
    EXPECT_EQ("two", clone->GetString("b", ""));

    EXPECT_TRUE(bag.Remove("a"));
    EXPECT_FALSE(bag.Remove("a"));
    EXPECT_FALSE(bag.Has("a"));
    EXPECT_EQ(1, bag.GetCount());

    bag.Clear();
    EXPECT_EQ(0, bag.GetCount());
    EXPECT_EQ(2, clone->GetCount());
}

TEST(ScriptStateBagTest, DisabledScriptAccessHidesValuesAndRejectsMutations) {
    ScriptStateBag bag;
    bag.SetInt("counter", 7);
    bag.SetString("name", "before");
    EXPECT_EQ(2, bag.GetStoredCount());

    bag.SetScriptAccessEnabled(false);
    EXPECT_FALSE(bag.Has("counter"));
    EXPECT_EQ(ScriptStateValueType::Empty, bag.GetType("counter"));
    EXPECT_EQ(0, bag.GetCount());
    EXPECT_EQ("", bag.GetKey(0));
    EXPECT_EQ(99, bag.GetInt("counter", 99));
    EXPECT_EQ("fallback", bag.GetString("name", "fallback"));
    EXPECT_FALSE(bag.Remove("counter"));

    bag.SetInt("counter", 8);
    bag.SetString("new", "ignored");
    bag.Clear();
    EXPECT_EQ(2, bag.GetStoredCount());

    BML::ScriptStateBagHandle clone(bag.Clone());
    ASSERT_TRUE(clone);
    EXPECT_FALSE(clone->IsScriptAccessEnabled());
    EXPECT_EQ(2, clone->GetStoredCount());
    EXPECT_EQ(0, clone->GetCount());

    bag.SetScriptAccessEnabled(true);
    EXPECT_EQ(2, bag.GetCount());
    EXPECT_EQ(7, bag.GetInt("counter", 0));
    EXPECT_EQ("before", bag.GetString("name", ""));
}
