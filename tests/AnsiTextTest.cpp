#include <gtest/gtest.h>

#include <cfloat>
#include <string>

#include "AnsiPalette.h"
#include "AnsiText.h"
#include "PathUtils.h"

namespace {
    std::wstring g_TestLoaderDir;

    std::string SegmentText(const AnsiText::TextSegment &segment) {
        return std::string(segment.begin, segment.end);
    }

    class ScopedPaletteLoaderDir {
    public:
        ScopedPaletteLoaderDir() : m_Previous(AnsiPalette::GetLoaderDirProvider()) {
            std::wstring tempPath = utils::CreateTempFileW(L"AnsiTextTest");
            if (!tempPath.empty()) {
                utils::DeleteFileW(tempPath);
                utils::CreateDirectoryW(tempPath);
                g_TestLoaderDir = tempPath;
            } else {
                g_TestLoaderDir = utils::GetTempPathW();
                g_TestLoaderDir.append(L"\\AnsiTextTest");
                if (utils::DirectoryExistsW(g_TestLoaderDir)) {
                    utils::DeleteDirectoryW(g_TestLoaderDir);
                }
                utils::CreateDirectoryW(g_TestLoaderDir);
            }
            AnsiPalette::SetLoaderDirProvider([]() -> std::wstring { return g_TestLoaderDir; });
        }

        ~ScopedPaletteLoaderDir() {
            AnsiPalette::SetLoaderDirProvider(m_Previous);
            if (!g_TestLoaderDir.empty()) {
                utils::DeleteDirectoryW(g_TestLoaderDir);
                g_TestLoaderDir.clear();
            }
        }

    private:
        AnsiPalette::LoaderDirProvider m_Previous = nullptr;
    };

    class ScopedImGuiContext {
    public:
        ScopedImGuiContext() : m_Previous(ImGui::GetCurrentContext()) {
            m_Context = ImGui::CreateContext();
            ImGui::SetCurrentContext(m_Context);
            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize = ImVec2(800.0f, 600.0f);
            io.Fonts->AddFontDefault();

            unsigned char *pixels = nullptr;
            int width = 0;
            int height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }

        ~ScopedImGuiContext() {
            ImGui::SetCurrentContext(m_Context);
            ImGui::DestroyContext(m_Context);
            ImGui::SetCurrentContext(m_Previous);
        }

        void BeginFrame() const {
            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize = ImVec2(800.0f, 600.0f);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f));
            ImGui::Begin("AnsiTextTestWindow", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        }

        void EndFrame() const {
            ImGui::End();
            ImGui::Render();
        }

    private:
        ImGuiContext *m_Previous = nullptr;
        ImGuiContext *m_Context = nullptr;
    };

    bool DrawListContainsColor(const ImDrawList *drawList, ImU32 color) {
        for (int i = 0; i < drawList->VtxBuffer.Size; ++i) {
            if (drawList->VtxBuffer[i].col == color) {
                return true;
            }
        }
        return false;
    }
}

TEST(AnsiTextTest, Utf8ContinuationByte9BIsNotTreatedAsCsi) {
    const std::string input = "\xE6\xB2\x9B\x6D"; // UTF-8 for U+6C9B followed by m

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(SegmentText(segments[0]), input);
    EXPECT_EQ(segments[0].color, AnsiText::ConsoleColor());
}

TEST(AnsiTextTest, EscSgrSequencesStillSplitSegments) {
    const std::string input = "\x1B[31mred\x1B[0mplain";

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(SegmentText(segments[0]), "red");
    EXPECT_EQ(SegmentText(segments[1]), "plain");
    EXPECT_NE(segments[0].color, AnsiText::ConsoleColor());
    EXPECT_EQ(segments[1].color, AnsiText::ConsoleColor());
}

TEST(AnsiTextTest, MoveAssignmentRebindsShortStringSegmentsToDestinationBuffer) {
    AnsiText::AnsiString source("Connecting...");
    AnsiText::AnsiString dest("placeholder");

    dest = std::move(source);

    const std::string &stored = dest.GetOriginalText();
    const auto &segments = dest.GetSegments();

    ASSERT_EQ(stored, "Connecting...");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(SegmentText(segments[0]), stored);
    EXPECT_EQ(segments[0].begin, stored.c_str());
    EXPECT_EQ(segments[0].end, stored.c_str() + stored.size());
}

TEST(AnsiTextTest, EmptySgrResetsFormatting) {
    const std::string input = "\x1B[31mred\x1B[mplain";

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(SegmentText(segments[0]), "red");
    EXPECT_EQ(SegmentText(segments[1]), "plain");
    EXPECT_NE(segments[0].color, AnsiText::ConsoleColor());
    EXPECT_EQ(segments[1].color, AnsiText::ConsoleColor());
}

TEST(AnsiTextTest, Sgr39ResetsToDefaultForegroundInsteadOfCurrentStyleColor) {
    ScopedImGuiContext context;
    ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(0.25f, 0.5f, 0.75f, 1.0f);
    const std::string input = "\x1B[31mred\x1B[39mplain";

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(SegmentText(segments[0]), "red");
    EXPECT_EQ(SegmentText(segments[1]), "plain");
    EXPECT_NE(segments[0].color, AnsiText::ConsoleColor());
    EXPECT_EQ(segments[1].color, AnsiText::ConsoleColor());
}

TEST(AnsiTextTest, IndexedBackgroundDrawsBeforeDefaultPaletteWasInitialized) {
    ScopedPaletteLoaderDir paletteLoaderDir;
    ScopedImGuiContext context;
    context.BeginFrame();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 startPos = ImGui::GetCursorScreenPos();
    AnsiText::AnsiString text("\x1B[41mX");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;
    AnsiPalette palette;
    options.palette = &palette;

    AnsiText::Renderer::DrawText(drawList, text, startPos, options);

    EXPECT_TRUE(DrawListContainsColor(drawList, IM_COL32(0x80, 0x00, 0x00, 0xFF)));
    context.EndFrame();
}

TEST(AnsiTextTest, DrawTextClippingUsesStartPositionNotCurrentCursor) {
    ScopedImGuiContext context;
    context.BeginFrame();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 visiblePos = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(visiblePos.x, visiblePos.y + 10000.0f));
    ImGui::Dummy(ImVec2(1.0f, 1.0f));

    AnsiText::AnsiString text("line1\nline2");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;
    const int beforeVertexCount = drawList->VtxBuffer.Size;

    AnsiText::Renderer::DrawText(drawList, text, visiblePos, options);

    EXPECT_GT(drawList->VtxBuffer.Size, beforeVertexCount);
    context.EndFrame();
}
