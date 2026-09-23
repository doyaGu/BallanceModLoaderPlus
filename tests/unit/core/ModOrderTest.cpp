#include <gtest/gtest.h>

#include "Loader/ModOrder.h"

TEST(ModOrderTest, PublishesSortedModsWithMatchingIndices) {
    int first = 0;
    int second = 0;
    std::vector<int *> mods = {&first, &second};
    std::unordered_map<std::string, std::size_t> index = {{"first", 0}, {"second", 1}};
    const std::unordered_map<int *, std::string> ids = {{&first, "first"}, {&second, "second"}};
    std::shared_mutex registryMutex;

    PublishModOrder(mods, index, std::vector<int *>{&second, &first}, ids, registryMutex);

    EXPECT_EQ(mods, (std::vector<int *>{&second, &first}));
    EXPECT_EQ(index.at("second"), 0u);
    EXPECT_EQ(index.at("first"), 1u);
}

TEST(ModOrderTest, IndexBuildFailurePreservesPublishedRegistry) {
    int first = 0;
    int second = 0;
    std::vector<int *> mods = {&first, &second};
    std::unordered_map<std::string, std::size_t> index = {{"first", 0}, {"second", 1}};
    const std::unordered_map<int *, std::string> incompleteIds = {{&second, "second"}};
    std::shared_mutex registryMutex;

    EXPECT_THROW(PublishModOrder(mods, index, std::vector<int *>{&second, &first}, incompleteIds, registryMutex),
                 std::out_of_range);

    EXPECT_EQ(mods, (std::vector<int *>{&first, &second}));
    EXPECT_EQ(index.size(), 2u);
    EXPECT_EQ(index.at("first"), 0u);
    EXPECT_EQ(index.at("second"), 1u);
}

TEST(ModOrderTest, DuplicateIdPreservesPublishedRegistry) {
    int first = 0;
    int second = 0;
    std::vector<int *> mods = {&first, &second};
    std::unordered_map<std::string, std::size_t> index = {{"first", 0}, {"second", 1}};
    const std::unordered_map<int *, std::string> duplicateIds = {{&first, "same"}, {&second, "same"}};
    std::shared_mutex registryMutex;

    EXPECT_THROW(PublishModOrder(mods, index, std::vector<int *>{&second, &first}, duplicateIds, registryMutex),
                 std::logic_error);

    EXPECT_EQ(mods, (std::vector<int *>{&first, &second}));
    EXPECT_EQ(index.size(), 2u);
    EXPECT_EQ(index.at("first"), 0u);
    EXPECT_EQ(index.at("second"), 1u);
}
