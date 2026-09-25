#include "UI/imgui_impl_ck2.h"

#include <gtest/gtest.h>

TEST(ImGuiBackendTest, UsesConservativeLimitsWhenDriverCapsAreUnavailable) {
    const ImGui_ImplCK2_TextureLimits limits = ImGui_ImplCK2_SelectTextureLimits(0, 0);

    EXPECT_EQ(limits.Width, ImGui_ImplCK2_TextureLimits::ConservativeFallback);
    EXPECT_EQ(limits.Height, ImGui_ImplCK2_TextureLimits::ConservativeFallback);
    EXPECT_TRUE(limits.UsedFallback);
}

TEST(ImGuiBackendTest, TreatsIncompleteDriverCapsAsUnavailable) {
    const ImGui_ImplCK2_TextureLimits limits = ImGui_ImplCK2_SelectTextureLimits(4096, 0);

    EXPECT_EQ(limits.Width, ImGui_ImplCK2_TextureLimits::ConservativeFallback);
    EXPECT_EQ(limits.Height, ImGui_ImplCK2_TextureLimits::ConservativeFallback);
    EXPECT_TRUE(limits.UsedFallback);
}

TEST(ImGuiBackendTest, PreservesSmallDriverLimits) {
    const ImGui_ImplCK2_TextureLimits limits = ImGui_ImplCK2_SelectTextureLimits(512, 256);

    EXPECT_EQ(limits.Width, 512);
    EXPECT_EQ(limits.Height, 256);
    EXPECT_FALSE(limits.UsedFallback);
}

TEST(ImGuiBackendTest, ClampsDriverLimitsToProjectMaximum) {
    const unsigned int excessive = (unsigned int)ImGui_ImplCK2_TextureLimits::ProjectMaximum * 2;
    const ImGui_ImplCK2_TextureLimits limits = ImGui_ImplCK2_SelectTextureLimits(excessive, excessive);

    EXPECT_EQ(limits.Width, ImGui_ImplCK2_TextureLimits::ProjectMaximum);
    EXPECT_EQ(limits.Height, ImGui_ImplCK2_TextureLimits::ProjectMaximum);
    EXPECT_FALSE(limits.UsedFallback);
}
