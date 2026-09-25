#include "imgui.h"

#include <gtest/gtest.h>

bool ImGui_ImplCK2_TestSelectTextureLimits(unsigned int reported_width,
                                           unsigned int reported_height,
                                           int *width, int *height, bool *used_fallback);
bool ImGui_ImplCK2_TestBuildDrawSlice(const ImDrawIdx *indices, unsigned int index_count,
                                      unsigned int vertex_offset, int vertex_count,
                                      unsigned int *slice_vertex_offset,
                                      unsigned int *slice_vertex_count,
                                      ImDrawIdx *rebased_indices,
                                      unsigned int rebased_capacity);
unsigned int ImGui_ImplCK2_TestGetTextureRetryDelay(unsigned int failure_count);
void ImGui_ImplCK2_TestCopyTextureRegion(bool use_colors, const ImU32 *src, int src_pitch,
                                         ImU32 *dst, int dst_pitch, int width, int height);

TEST(ImGuiBackendTest, UsesConservativeLimitsWhenDriverCapsAreUnavailable) {
    int width = 0;
    int height = 0;
    bool usedFallback = false;

    ASSERT_TRUE(ImGui_ImplCK2_TestSelectTextureLimits(0, 0, &width, &height, &usedFallback));
    EXPECT_EQ(width, 2048);
    EXPECT_EQ(height, 2048);
    EXPECT_TRUE(usedFallback);
}

TEST(ImGuiBackendTest, PreservesAndClampsDriverLimits) {
    int width = 0;
    int height = 0;
    bool usedFallback = true;

    ASSERT_TRUE(ImGui_ImplCK2_TestSelectTextureLimits(512, 8192, &width, &height, &usedFallback));
    EXPECT_EQ(width, 512);
    EXPECT_EQ(height, 4096);
    EXPECT_FALSE(usedFallback);
}

TEST(ImGuiBackendTest, RebaseLargeMeshCommandToItsReferencedVertices) {
    const ImDrawIdx indices[] = {100, 102, 101};
    ImDrawIdx rebased[3] = {};
    unsigned int vertexOffset = 0;
    unsigned int vertexCount = 0;

    ASSERT_TRUE(ImGui_ImplCK2_TestBuildDrawSlice(
        indices, 3, 70000, 80000, &vertexOffset, &vertexCount, rebased, 3));
    EXPECT_EQ(vertexOffset, 70100u);
    EXPECT_EQ(vertexCount, 3u);
    EXPECT_EQ(rebased[0], 0);
    EXPECT_EQ(rebased[1], 2);
    EXPECT_EQ(rebased[2], 1);
}

TEST(ImGuiBackendTest, RejectsDrawSliceOutsideTheVertexBuffer) {
    const ImDrawIdx indices[] = {0, 4};
    ImDrawIdx rebased[2] = {};
    unsigned int vertexOffset = 0;
    unsigned int vertexCount = 0;

    EXPECT_FALSE(ImGui_ImplCK2_TestBuildDrawSlice(
        indices, 2, 8, 12, &vertexOffset, &vertexCount, rebased, 2));
}

TEST(ImGuiBackendTest, TextureFailureBackoffIsBounded) {
    EXPECT_EQ(ImGui_ImplCK2_TestGetTextureRetryDelay(0), 0u);
    EXPECT_EQ(ImGui_ImplCK2_TestGetTextureRetryDelay(1), 1u);
    EXPECT_EQ(ImGui_ImplCK2_TestGetTextureRetryDelay(2), 2u);
    EXPECT_EQ(ImGui_ImplCK2_TestGetTextureRetryDelay(7), 64u);
    EXPECT_EQ(ImGui_ImplCK2_TestGetTextureRetryDelay(20), 64u);
}

TEST(ImGuiBackendTest, CopiesRowsUsingTheActualSourceAndDestinationPitch) {
    const ImU32 source[] = {1, 2, 99, 3, 4, 99};
    ImU32 destination[] = {0, 0, 77, 77, 0, 0, 77, 77};

    ImGui_ImplCK2_TestCopyTextureRegion(
        false, source, 3 * sizeof(ImU32), destination, 4 * sizeof(ImU32), 2, 2);

    EXPECT_EQ(destination[0], 1u);
    EXPECT_EQ(destination[1], 2u);
    EXPECT_EQ(destination[2], 77u);
    EXPECT_EQ(destination[3], 77u);
    EXPECT_EQ(destination[4], 3u);
    EXPECT_EQ(destination[5], 4u);
    EXPECT_EQ(destination[6], 77u);
    EXPECT_EQ(destination[7], 77u);
}
