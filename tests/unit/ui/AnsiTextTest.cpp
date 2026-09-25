#include <gtest/gtest.h>

#include <cfloat>
#include <string>

#include "UI/AnsiPalette.h"
#include "UI/AnsiText.h"
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

    int CountDrawListVerticesWithColor(const ImDrawList *drawList, ImU32 color) {
        int count = 0;
        for (int i = 0; i < drawList->VtxBuffer.Size; ++i) {
            if (drawList->VtxBuffer[i].col == color)
                ++count;
        }
        return count;
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

TEST(AnsiTextTest, PlainTextRangeOmitsAnsiFormatting) {
    const std::string input = "\x1B[31mred\x1B[0mplain";
    AnsiText::AnsiString text(input);

    EXPECT_EQ(text.GetPlainText(), "redplain");
    EXPECT_EQ(text.GetPlainText(5, 8), "red");
    EXPECT_EQ(text.GetPlainText(0, 5), "");
}

TEST(AnsiTextTest, InitialForegroundPreservesPackedAlphaWithoutEscapeText) {
    const ImU32 color = IM_COL32(12, 34, 56, 78);
    AnsiText::AnsiString text(std::string("colored"), AnsiText::ConsoleColor(color));

    ASSERT_EQ(text.GetOriginalText(), "colored");
    ASSERT_EQ(text.GetSegments().size(), 1u);
    EXPECT_EQ(text.GetSegments()[0].color.foreground, color);
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

TEST(AnsiTextTest, PreparedTextInvalidatesOnlyWhenItsLayoutInputsChange) {
    ScopedImGuiContext context;
    context.BeginFrame();

    AnsiText::AnsiString text("one two three");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    EXPECT_TRUE(prepared.IsValid());
    EXPECT_TRUE(prepared.Matches(text, options));

    AnsiText::TextOptions narrower = options;
    narrower.wrapWidth = ImGui::CalcTextSize("one two").x;
    EXPECT_FALSE(prepared.Matches(text, narrower));
    AnsiText::PreparedText wrapped;
    ASSERT_TRUE(wrapped.Prepare(text, narrower));
    EXPECT_GT(wrapped.GetSize().y, prepared.GetSize().y);

    text.SetText("changed");
    EXPECT_FALSE(prepared.Matches(text, options));
    ASSERT_TRUE(prepared.Prepare(text, options));
    EXPECT_TRUE(prepared.Matches(text, options));

    prepared.Clear();
    EXPECT_FALSE(prepared.IsValid());
    EXPECT_FALSE(prepared.Matches(text, options));
    context.EndFrame();
}

TEST(AnsiTextTest, PreparedTextHitTestUsesUtf8CharacterBoundaries) {
    ScopedImGuiContext context;
    context.BeginFrame();

    AnsiText::AnsiString text("ab\xE4\xB8\xAD");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    const float a = ImGui::CalcTextSize("a").x;
    const float ab = ImGui::CalcTextSize("ab").x;
    const float ideograph = ImGui::CalcTextSize("\xE4\xB8\xAD").x;

    EXPECT_EQ(prepared.HitTest(ImVec2(a * 0.25f, 0.0f)), 0u);
    EXPECT_EQ(prepared.HitTest(ImVec2(a * 0.75f, 0.0f)), 1u);
    EXPECT_EQ(prepared.HitTest(ImVec2(ab + ideograph * 0.25f, 0.0f)), 2u);
    EXPECT_EQ(prepared.HitTest(ImVec2(ab + ideograph * 0.75f, 0.0f)), 5u);
    context.EndFrame();
}

TEST(AnsiTextTest, PreparedTextHitTestKeepsEmojiClustersTogether) {
    ScopedImGuiContext context;
    context.BeginFrame();

    const std::string emoji =
        "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"
        "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
    AnsiText::AnsiString text(emoji);
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    const float firstClusterWidth = ImGui::CalcTextSize(
        emoji.data(), emoji.data() + 8).x;
    const float secondClusterWidth = ImGui::CalcTextSize(
        emoji.data() + 8, emoji.data() + 16).x;

    EXPECT_EQ(prepared.HitTest(ImVec2(firstClusterWidth * 0.1f, 0.0f)), 0u);
    EXPECT_EQ(prepared.HitTest(ImVec2(firstClusterWidth * 0.6f, 0.0f)), 8u);
    EXPECT_EQ(prepared.HitTest(ImVec2(
        firstClusterWidth + secondClusterWidth * 0.1f, 0.0f)), 8u);
    EXPECT_EQ(prepared.HitTest(ImVec2(
        firstClusterWidth + secondClusterWidth * 0.6f, 0.0f)), 16u);
    context.EndFrame();
}

TEST(AnsiTextTest, PreparedTextHitTestTracksWrappedSourceOffsets) {
    ScopedImGuiContext context;
    context.BeginFrame();

    AnsiText::AnsiString text("ab cd");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = ImGui::CalcTextSize("ab").x + 0.1f;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    ASSERT_GT(prepared.GetSize().y, ImGui::GetTextLineHeight());

    const float secondLineY = prepared.GetSize().y - 1.0f;
    EXPECT_EQ(prepared.HitTest(ImVec2(0.0f, secondLineY)), 3u);
    EXPECT_EQ(prepared.HitTest(ImVec2(FLT_MAX, secondLineY)), 5u);
    context.EndFrame();
}

TEST(AnsiTextTest, PreparedTextDrawsSelectedRangeBehindText) {
    ScopedImGuiContext context;
    context.BeginFrame();

    AnsiText::AnsiString text("select me");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    const ImU32 selectionColor = IM_COL32(17, 34, 51, 255);
    prepared.DrawSelection(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(),
                           0, 6, selectionColor);

    EXPECT_TRUE(DrawListContainsColor(ImGui::GetWindowDrawList(), selectionColor));
    context.EndFrame();
}

TEST(AnsiTextTest, PreparedTextDrawSelectionHonorsVerticalClipping) {
    ScopedImGuiContext context;
    context.BeginFrame();

    AnsiText::AnsiString text("one\ntwo\nthree\nfour");
    AnsiText::TextOptions options;
    options.font = ImGui::GetFont();
    options.fontSize = ImGui::GetFontSize();
    options.wrapWidth = FLT_MAX;
    options.lineSpacing = 0.0f;

    AnsiText::PreparedText prepared;
    ASSERT_TRUE(prepared.Prepare(text, options));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float lineHeight = prepared.GetSize().y / 4.0f;
    const ImU32 selectionColor = IM_COL32(29, 47, 83, 255);

    drawList->PushClipRect(
        ImVec2(position.x, position.y + lineHeight),
        ImVec2(position.x + 200.0f, position.y + lineHeight * 2.0f), true);
    prepared.DrawSelection(drawList, position, 0, text.GetOriginalText().size(), selectionColor);
    drawList->PopClipRect();

    EXPECT_EQ(CountDrawListVerticesWithColor(drawList, selectionColor), 4);
    context.EndFrame();
}
