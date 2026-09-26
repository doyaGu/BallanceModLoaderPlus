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
bool ImGui_ImplCK2_TestBuildDrawSegment(const ImDrawCmd *commands, int command_count,
                                        int first_command, int vertex_count,
                                        unsigned int *segment_vertex_offset,
                                        unsigned int *segment_vertex_count,
                                        int *end_command);
bool ImGui_ImplCK2_TestShouldUploadDrawSegment(const ImDrawCmd *commands, int command_count,
                                               const ImDrawIdx *indices, int index_count,
                                               int first_command, int vertex_count);
unsigned int ImGui_ImplCK2_TestCountTextureBindings(const ImTextureID *textures,
                                                     const bool *invalidate_before,
                                                     int texture_count);
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

TEST(ImGuiBackendTest, GroupsCommandsByTheirVertexOffset) {
    ImDrawCmd commands[4];
    commands[0].VtxOffset = 0;
    commands[1].VtxOffset = 0;
    commands[2].VtxOffset = 60000;
    commands[3].VtxOffset = 60000;

    unsigned int vertexOffset = 0;
    unsigned int vertexCount = 0;
    int endCommand = 0;
    ASSERT_TRUE(ImGui_ImplCK2_TestBuildDrawSegment(
        commands, 4, 0, 90000, &vertexOffset, &vertexCount, &endCommand));
    EXPECT_EQ(vertexOffset, 0u);
    EXPECT_EQ(vertexCount, 60000u);
    EXPECT_EQ(endCommand, 2);

    ASSERT_TRUE(ImGui_ImplCK2_TestBuildDrawSegment(
        commands, 4, 2, 90000, &vertexOffset, &vertexCount, &endCommand));
    EXPECT_EQ(vertexOffset, 60000u);
    EXPECT_EQ(vertexCount, 30000u);
    EXPECT_EQ(endCommand, 4);
}

TEST(ImGuiBackendTest, AcceptsTheLargest16BitVertexSegment) {
    ImDrawCmd command;
    command.VtxOffset = 0;
    unsigned int vertexOffset = 0;
    unsigned int vertexCount = 0;
    int endCommand = 0;

    ASSERT_TRUE(ImGui_ImplCK2_TestBuildDrawSegment(
        &command, 1, 0, 65536, &vertexOffset, &vertexCount, &endCommand));
    EXPECT_EQ(vertexCount, 65536u);
}

TEST(ImGuiBackendTest, RejectsOversizedOrBackwardVertexSegments) {
    ImDrawCmd oversized;
    oversized.VtxOffset = 0;
    unsigned int vertexOffset = 0;
    unsigned int vertexCount = 0;
    int endCommand = 0;
    EXPECT_FALSE(ImGui_ImplCK2_TestBuildDrawSegment(
        &oversized, 1, 0, 65537, &vertexOffset, &vertexCount, &endCommand));

    ImDrawCmd backward[2];
    backward[0].VtxOffset = 40000;
    backward[1].VtxOffset = 20000;
    EXPECT_FALSE(ImGui_ImplCK2_TestBuildDrawSegment(
        backward, 2, 0, 60000, &vertexOffset, &vertexCount, &endCommand));
}

TEST(ImGuiBackendTest, UploadsDenseLargeSegmentsOnce) {
    ImDrawCmd commands[2];
    commands[0].VtxOffset = 0;
    commands[0].IdxOffset = 0;
    commands[0].ElemCount = 4;
    commands[1].VtxOffset = 0;
    commands[1].IdxOffset = 4;
    commands[1].ElemCount = 4;
    const ImDrawIdx indices[] = {0, 1, 2, 3, 4, 5, 6, 7};

    EXPECT_TRUE(ImGui_ImplCK2_TestShouldUploadDrawSegment(
        commands, 2, indices, 8, 0, 8));
}

TEST(ImGuiBackendTest, SlicesSparseLargeSegmentsPerCommand) {
    ImDrawCmd commands[2];
    commands[0].VtxOffset = 0;
    commands[0].IdxOffset = 0;
    commands[0].ElemCount = 3;
    commands[1].VtxOffset = 0;
    commands[1].IdxOffset = 3;
    commands[1].ElemCount = 3;
    const ImDrawIdx indices[] = {0, 1, 2, 97, 98, 99};

    EXPECT_FALSE(ImGui_ImplCK2_TestShouldUploadDrawSegment(
        commands, 2, indices, 6, 0, 100));
}

TEST(ImGuiBackendTest, RejectsSegmentIndicesOutsideItsVertexRange) {
    ImDrawCmd command;
    command.VtxOffset = 0;
    command.IdxOffset = 0;
    command.ElemCount = 3;
    const ImDrawIdx indices[] = {0, 42, 60};

    EXPECT_FALSE(ImGui_ImplCK2_TestShouldUploadDrawSegment(
        &command, 1, indices, 3, 0, 60));
}

TEST(ImGuiBackendTest, ReusesConsecutiveTextureBindings) {
    const ImTextureID textures[] = {1, 1, 1, 2, 2, 1};

    EXPECT_EQ(ImGui_ImplCK2_TestCountTextureBindings(textures, nullptr, 6), 3u);
}

TEST(ImGuiBackendTest, RebindsTextureAfterUntrustedRendering) {
    const ImTextureID textures[] = {1, 1, 1, 1};
    const bool invalidateBefore[] = {false, false, true, false};

    EXPECT_EQ(ImGui_ImplCK2_TestCountTextureBindings(textures, invalidateBefore, 4), 2u);
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
