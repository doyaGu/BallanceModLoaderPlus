#include <gtest/gtest.h>

#include "imgui.h"

#include "UI/FontRuntime.h"

namespace {
class ImGuiContextFixture : public testing::Test {
protected:
    void SetUp() override {
        m_Context = ImGui::CreateContext();
        ASSERT_NE(m_Context, nullptr);
        ImGuiIO &io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.DisplaySize = ImVec2(1280.0f, 720.0f);
        io.DeltaTime = 1.0f / 60.0f;
    }

    void TearDown() override {
        if (m_Context)
            ImGui::DestroyContext(m_Context);
    }

    ImGuiContext *m_Context = nullptr;
};

bool ContainsSourceOrigin(
    const BML::UI::FontRuntimeSnapshot &snapshot,
    BML::UI::FontSourceOrigin origin) {
    for (const BML::UI::FontSourceStatus &source :
         snapshot.Sources) {
        if (source.Origin == origin)
            return true;
    }
    return false;
}
}

TEST(FontRuntimeTest, ImGuiStoresSupplementaryPlaneCodepoints) {
    EXPECT_EQ(sizeof(ImWchar), sizeof(ImWchar32));
    EXPECT_EQ(static_cast<unsigned int>(static_cast<ImWchar>(0x1F600)), 0x1F600u);
}

TEST_F(ImGuiContextFixture, WindowsCatalogProvidesCommonEmojiGlyphs) {
    BML::UI::FontRuntime runtime("missing-loader-font-directory");
    BML::UI::FontProfile profile;
    profile.PrimaryFace = "missing-primary.ttf";
    profile.UseWindowsFallbacks = true;

    runtime.Configure(profile);
    ASSERT_EQ(runtime.Inspect().State, BML::UI::FontRuntimeState::Pending);
    runtime.Synchronize(*m_Context, 1200.0f);

    const auto &snapshot = runtime.Inspect();
    EXPECT_EQ(snapshot.State, BML::UI::FontRuntimeState::Degraded);
    EXPECT_TRUE(snapshot.SupportsUnicodeScalars);
    EXPECT_TRUE(snapshot.SupportsCommonEmoji);
    EXPECT_FALSE(snapshot.SupportsColorEmoji);

    ImGuiIO &io = ImGui::GetIO();
    ASSERT_NE(io.FontDefault, nullptr);
    io.Fonts->Build();
    ImFontBaked *baked = io.FontDefault->GetFontBaked(32.0f);
    ASSERT_NE(baked, nullptr);
    const ImFontGlyph *glyph = baked->FindGlyphNoFallback(
        static_cast<ImWchar>(0x1F600));
    ASSERT_NE(glyph, nullptr);
    EXPECT_TRUE(glyph->Visible);
    EXPECT_GT(glyph->X1, glyph->X0);
    EXPECT_GT(glyph->Y1, glyph->Y0);
}

TEST_F(ImGuiContextFixture, DisabledWindowsFallbacksDoNotEnterTheAtlas) {
    BML::UI::FontRuntime runtime("missing-loader-font-directory");
    BML::UI::FontProfile profile;
    profile.PrimaryFace = "missing-primary.ttf";
    profile.UseWindowsFallbacks = false;

    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    const auto &snapshot = runtime.Inspect();
    EXPECT_FALSE(snapshot.SupportsCommonEmoji);
    ASSERT_FALSE(snapshot.Sources.empty());
    EXPECT_EQ(snapshot.Sources.front().Origin,
              BML::UI::FontSourceOrigin::LoaderCatalog);
    EXPECT_FALSE(ContainsSourceOrigin(
        snapshot, BML::UI::FontSourceOrigin::WindowsCatalog));
}

TEST_F(ImGuiContextFixture, PackagedPrimaryPrecedesWindowsFallbacks) {
    BML::UI::FontRuntime runtime(BML_TEST_FONT_DIRECTORY);
    EXPECT_EQ(runtime.ListLoaderFaces(),
              std::vector<std::string>({"unifont.otf"}));

    BML::UI::FontProfile profile;
    profile.PrimaryFace = "unifont.otf";
    profile.UseWindowsFallbacks = true;

    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    const auto &snapshot = runtime.Inspect();
    ASSERT_EQ(snapshot.State, BML::UI::FontRuntimeState::Ready);
    ASSERT_GE(snapshot.Sources.size(), 3u);
    EXPECT_EQ(snapshot.Sources[0].Origin,
              BML::UI::FontSourceOrigin::LoaderCatalog);
    EXPECT_TRUE(snapshot.Sources[0].Loaded);
    EXPECT_EQ(snapshot.Sources[1].Origin,
              BML::UI::FontSourceOrigin::WindowsCatalog);
    EXPECT_TRUE(snapshot.SupportsCommonEmoji);

    ImFont *font = ImGui::GetIO().FontDefault;
    ASSERT_NE(font, nullptr);
    EXPECT_TRUE(font->IsGlyphInFont(static_cast<ImWchar>(0x4F60)));
    EXPECT_TRUE(font->IsGlyphInFont(static_cast<ImWchar>(0x1F600)));
}

TEST_F(ImGuiContextFixture, SameProfileIsIdempotentAndReplacementDoesNotDuplicateFonts) {
    BML::UI::FontRuntime runtime("missing-loader-font-directory");
    BML::UI::FontProfile profile;
    profile.PrimaryFace = "missing-primary.ttf";

    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);
    const std::uint64_t firstGeneration = runtime.Inspect().Generation;
    ASSERT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 1);

    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 600.0f);
    EXPECT_EQ(runtime.Inspect().Generation, firstGeneration);
    EXPECT_FLOAT_EQ(ImGui::GetStyle().FontScaleMain, 0.5f);
    EXPECT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 1);

    profile.ReferenceSize = 40.0f;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 600.0f);
    EXPECT_EQ(runtime.Inspect().Generation, firstGeneration + 1);
    EXPECT_FLOAT_EQ(runtime.Inspect().ReferenceSize, 40.0f);
    EXPECT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 1);

    ImGui::NewFrame();
    ImGui::TextUnformatted("replacement font is bound");
    ImGui::EndFrame();
}

TEST_F(ImGuiContextFixture, ReplacementPreservesFontsOwnedByOtherConsumers) {
    ImFontConfig externalConfig;
    externalConfig.SizePixels = 18.0f;
    ImFont *externalFont = ImGui::GetIO().Fonts->AddFontDefaultVector(
        &externalConfig);
    ASSERT_NE(externalFont, nullptr);

    BML::UI::FontRuntime runtime("missing-loader-font-directory");
    BML::UI::FontProfile profile;
    profile.PrimaryFace = "missing-primary.ttf";
    profile.UseWindowsFallbacks = false;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);
    ASSERT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 2);

    profile.ReferenceSize = 40.0f;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    EXPECT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 2);
    EXPECT_TRUE(ImGui::GetIO().Fonts->Fonts.contains(externalFont));
    EXPECT_NE(ImGui::GetIO().FontDefault, externalFont);
}

TEST_F(ImGuiContextFixture, LockedAtlasDefersReplacementUntilSafeSeam) {
    BML::UI::FontRuntime runtime("missing-loader-font-directory");
    BML::UI::FontProfile profile;
    profile.PrimaryFace = "missing-primary.ttf";

    ImGui::GetIO().Fonts->Locked = true;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);
    EXPECT_EQ(runtime.Inspect().State, BML::UI::FontRuntimeState::Pending);
    EXPECT_EQ(runtime.Inspect().Generation, 0u);

    ImGui::GetIO().Fonts->Locked = false;
    runtime.Synchronize(*m_Context, 1200.0f);
    EXPECT_GT(runtime.Inspect().Generation, 0u);
    EXPECT_NE(runtime.Inspect().State, BML::UI::FontRuntimeState::Pending);
}

TEST_F(ImGuiContextFixture, ExplicitReloadRebuildsTheAppliedProfile) {
    BML::UI::FontRuntime runtime(BML_TEST_FONT_DIRECTORY);
    BML::UI::FontProfile profile;
    profile.UseWindowsFallbacks = false;

    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);
    const std::uint64_t generation = runtime.Inspect().Generation;
    const BML::UI::FontRuntimeState appliedState =
        runtime.Inspect().State;

    ASSERT_TRUE(runtime.RequestReload());
    EXPECT_EQ(runtime.Inspect().State,
              BML::UI::FontRuntimeState::Pending);
    runtime.Synchronize(*m_Context, 1200.0f);

    EXPECT_EQ(runtime.Inspect().Generation, generation + 1);
    EXPECT_EQ(runtime.Inspect().State, appliedState);
    EXPECT_EQ(ImGui::GetIO().Fonts->Fonts.Size, 1);
}

TEST_F(ImGuiContextFixture, TextInspectionReportsRealMergedFontCoverage) {
    BML::UI::FontRuntime runtime(BML_TEST_FONT_DIRECTORY);
    BML::UI::FontProfile profile;
    profile.UseWindowsFallbacks = true;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    const std::string covered =
        "A\xE4\xB8\xAD\xE6\x96\x87\xF0\x9F\x98\x80";
    const BML::UI::FontCoverage coverage =
        runtime.InspectText(covered);
    EXPECT_TRUE(coverage.FontAvailable);
    EXPECT_TRUE(coverage.ValidUtf8);
    EXPECT_EQ(coverage.CodepointCount, 4u);
    EXPECT_TRUE(coverage.MissingCodepoints.empty());

    const std::string invalid(1, static_cast<char>(0xFF));
    const BML::UI::FontCoverage invalidCoverage =
        runtime.InspectText(invalid);
    EXPECT_TRUE(invalidCoverage.FontAvailable);
    EXPECT_FALSE(invalidCoverage.ValidUtf8);
    EXPECT_EQ(invalidCoverage.CodepointCount, 0u);
    EXPECT_TRUE(invalidCoverage.MissingCodepoints.empty());
}

TEST_F(ImGuiContextFixture, FallbackSizeChangesMergedGlyphsWithoutResizingPrimary) {
    BML::UI::FontRuntime runtime(BML_TEST_FONT_DIRECTORY);
    BML::UI::FontProfile profile;
    profile.ReferenceSize = 32.0f;
    profile.FallbackReferenceSize = 24.0f;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);
    EXPECT_FLOAT_EQ(runtime.Inspect().ReferenceSize, profile.ReferenceSize);
    EXPECT_FLOAT_EQ(runtime.Inspect().FallbackReferenceSize, profile.FallbackReferenceSize);

    ImFont *font = ImGui::GetIO().FontDefault;
    ASSERT_NE(font, nullptr);
    ASSERT_GE(font->Sources.Size, 2);
    EXPECT_FLOAT_EQ(font->Sources[0]->SizePixels, profile.ReferenceSize);
    EXPECT_FLOAT_EQ(font->Sources[1]->SizePixels, profile.FallbackReferenceSize);

    ASSERT_TRUE(ImGui::GetIO().Fonts->Build());
    ImFontBaked *baked = font->GetFontBaked(profile.ReferenceSize);
    ASSERT_NE(baked, nullptr);
    const ImFontGlyph *primary = baked->FindGlyphNoFallback('A');
    const ImFontGlyph *fallback = baked->FindGlyphNoFallback(static_cast<ImWchar>(0x1F600));
    ASSERT_NE(primary, nullptr);
    ASSERT_NE(fallback, nullptr);
    ASSERT_NE(fallback->SourceIdx, 0u);
    const float primaryHeight = primary->Y1 - primary->Y0;
    const float fallbackHeight = fallback->Y1 - fallback->Y0;
    const std::uint64_t generation = runtime.Inspect().Generation;

    profile.FallbackReferenceSize = 40.0f;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    EXPECT_EQ(runtime.Inspect().Generation, generation + 1);
    EXPECT_FLOAT_EQ(runtime.Inspect().ReferenceSize, profile.ReferenceSize);
    EXPECT_FLOAT_EQ(runtime.Inspect().FallbackReferenceSize, profile.FallbackReferenceSize);
    font = ImGui::GetIO().FontDefault;
    ASSERT_NE(font, nullptr);
    ASSERT_GE(font->Sources.Size, 2);
    EXPECT_FLOAT_EQ(font->Sources[0]->SizePixels, profile.ReferenceSize);
    EXPECT_FLOAT_EQ(font->Sources[1]->SizePixels, profile.FallbackReferenceSize);

    ASSERT_TRUE(ImGui::GetIO().Fonts->Build());
    baked = font->GetFontBaked(profile.ReferenceSize);
    ASSERT_NE(baked, nullptr);
    primary = baked->FindGlyphNoFallback('A');
    fallback = baked->FindGlyphNoFallback(static_cast<ImWchar>(0x1F600));
    ASSERT_NE(primary, nullptr);
    ASSERT_NE(fallback, nullptr);
    EXPECT_FLOAT_EQ(primary->Y1 - primary->Y0, primaryHeight);
    EXPECT_GT(fallback->Y1 - fallback->Y0, fallbackHeight);
}

TEST_F(ImGuiContextFixture, MergedFallbackKeepsItsStandaloneBaseline) {
    BML::UI::FontRuntime runtime(BML_TEST_FONT_DIRECTORY);
    BML::UI::FontProfile profile;
    profile.FallbackReferenceSize = 24.0f;
    profile.UseWindowsFallbacks = true;
    runtime.Configure(profile);
    runtime.Synchronize(*m_Context, 1200.0f);

    std::string fallbackPath;
    for (const BML::UI::FontSourceStatus &source :
         runtime.Inspect().Sources) {
        if (source.Origin ==
                BML::UI::FontSourceOrigin::WindowsCatalog &&
            source.RequestedFace == "Segoe UI Emoji" && source.Loaded) {
            fallbackPath = source.ResolvedPath;
            break;
        }
    }
    ASSERT_FALSE(fallbackPath.empty());

    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig standaloneConfig;
    standaloneConfig.SizePixels = profile.FallbackReferenceSize;
    ImFont *standaloneFallback = io.Fonts->AddFontFromFileTTF(
        fallbackPath.c_str(), profile.FallbackReferenceSize, &standaloneConfig,
        nullptr);
    ASSERT_NE(standaloneFallback, nullptr);
    ASSERT_TRUE(io.Fonts->Build());
    ASSERT_NE(io.FontDefault, nullptr);
    ImFontBaked *merged = io.FontDefault->GetFontBaked(
        profile.ReferenceSize);
    ImFontBaked *standalone = standaloneFallback->GetFontBaked(
        profile.FallbackReferenceSize);
    ASSERT_NE(merged, nullptr);
    ASSERT_NE(standalone, nullptr);

    constexpr ImWchar Probe = static_cast<ImWchar>(0x1F600);
    const ImFontGlyph *mergedGlyph = merged->FindGlyphNoFallback(Probe);
    const ImFontGlyph *standaloneGlyph =
        standalone->FindGlyphNoFallback(Probe);
    ASSERT_NE(mergedGlyph, nullptr);
    ASSERT_NE(standaloneGlyph, nullptr);
    ASSERT_NE(mergedGlyph->SourceIdx, 0u);
    EXPECT_FLOAT_EQ(mergedGlyph->Y0, standaloneGlyph->Y0);
    EXPECT_FLOAT_EQ(mergedGlyph->Y1, standaloneGlyph->Y1);
}
