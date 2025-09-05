#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <cwchar>

#include "AnsiPalette.h"
#include "StringUtils.h"
#include "PathUtils.h"

#include "imgui.h"

// --------------------
// Minimal ModContext stubs for tests
// --------------------

static std::wstring g_TestLoaderDir; // points to a temp test directory

// --------------------
// Test fixture
// --------------------

class AnsiPaletteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique temporary directory and set it as loader dir
        std::wstring tmp = utils::CreateTempFileW(L"AnsiPaletteTest");
        if (!tmp.empty()) {
            // Turn that temp file path into a directory path
            utils::DeleteFileW(tmp);
            utils::CreateDirectoryW(tmp);
            g_TestLoaderDir = tmp;
        } else {
            // Fallback to system temp + fixed subdir
            g_TestLoaderDir = utils::GetTempPathW();
            g_TestLoaderDir.append(L"\\AnsiPaletteTest");
            if (utils::DirectoryExistsW(g_TestLoaderDir)) {
                utils::DeleteDirectoryW(g_TestLoaderDir);
            }
            utils::CreateDirectoryW(g_TestLoaderDir);
        }
        // Inject provider for loader directory
        AnsiPalette::SetLoaderDirProvider([]() -> std::wstring { return g_TestLoaderDir; });
    }

    void TearDown() override {
        if (!g_TestLoaderDir.empty()) {
            utils::DeleteDirectoryW(g_TestLoaderDir);
            g_TestLoaderDir.clear();
        }
        AnsiPalette::SetLoaderDirProvider(nullptr);
    }

    static std::wstring LoaderCfg() {
        std::wstring p = g_TestLoaderDir; p.append(L"\\palette.ini");
        return p;
    }

    static std::wstring ThemesDir() {
        std::wstring p = g_TestLoaderDir; p.append(L"\\themes");
        return p;
    }
};

// --------------------
// Tests
// --------------------

TEST_F(AnsiPaletteTest, BuildDefaultPalette) {
    // Ensure no config exists
    utils::DeleteFileW(LoaderCfg());

    AnsiPalette pal;
    pal.EnsureInitialized();
    EXPECT_TRUE(pal.IsActive());

    ImU32 col = 0;
    ASSERT_TRUE(pal.GetColor(0, col)); // standard black
    EXPECT_EQ(col, IM_COL32(0, 0, 0, 255));

    ASSERT_TRUE(pal.GetColor(15, col)); // bright white
    EXPECT_EQ(col, IM_COL32(255, 255, 255, 255));

    ASSERT_TRUE(pal.GetColor(232, col)); // gray ramp start: 8
    EXPECT_EQ(col, IM_COL32(8, 8, 8, 255));

    // Check a cube color (e.g., index 196)
    int idx = 196;
    static const int values[6] = {0, 95, 135, 175, 215, 255};
    int v = idx - 16;
    int r6 = (v / 36) % 6;
    int g6 = ((v % 36) / 6) % 6;
    int b6 = v % 6;
    ImU32 expected = IM_COL32(values[r6], values[g6], values[b6], 255);
    ASSERT_TRUE(pal.GetColor(idx, col));
    EXPECT_EQ(col, expected);

    // Invalid indices
    EXPECT_FALSE(pal.GetColor(-1, col));
    EXPECT_FALSE(pal.GetColor(256, col));
}

TEST_F(AnsiPaletteTest, HexAndRGBAHelpers) {
    EXPECT_EQ(AnsiPalette::RGBA(1, 2, 3, 4), IM_COL32(1, 2, 3, 4));
    // #RRGGBB
    EXPECT_EQ(AnsiPalette::HexToImU32("FF0000"), IM_COL32(255, 0, 0, 255));
    // #AARRGGBB
    EXPECT_EQ(AnsiPalette::HexToImU32("80FFFFFF"), IM_COL32(255, 255, 255, 128));
    // Invalid formats -> white
    EXPECT_EQ(AnsiPalette::HexToImU32("GHIJKL"), IM_COL32_WHITE);
    EXPECT_EQ(AnsiPalette::HexToImU32("123"), IM_COL32_WHITE);
}

TEST_F(AnsiPaletteTest, SaveSampleCreatesFiles) {
    utils::DeleteFileW(LoaderCfg());
    utils::DeleteDirectoryW(ThemesDir());

    AnsiPalette pal;
    bool wrote = pal.SaveSampleIfMissing();
    EXPECT_TRUE(utils::FileExistsW(LoaderCfg()));
    // Themes directory and nord theme should exist
    EXPECT_TRUE(utils::DirectoryExistsW(ThemesDir()));
    std::wstring nord = ThemesDir(); nord.append(L"\\nord.ini");
    EXPECT_TRUE(utils::FileExistsW(nord));
    // Second call should not overwrite
    bool wrote2 = pal.SaveSampleIfMissing();
    EXPECT_FALSE(wrote2);
}

TEST_F(AnsiPaletteTest, OverridesAndSectionsParse) {
    // Write a config with various sections and overrides (including invalid keys)
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[standard]\nred = #112233\n\n"
        L"[bright]\n8 = #ABCDEF\n\n"
        L"[cube]\n20 = 1,2,3\n\n"
        L"[gray]\n240 = 10,20,30,40\n\n"
        L"[overrides]\n196 = #FF0000\nfoo = #123456\n100-xx = #FFFFFF\n";
    utils::WriteTextFileW(cfg, content);

    AnsiPalette pal;
    EXPECT_TRUE(pal.ReloadFromFile());

    ImU32 col = 0;
    ASSERT_TRUE(pal.GetColor(1, col)); // standard red overridden to #112233
    EXPECT_EQ(col, IM_COL32(0x11, 0x22, 0x33, 255));

    ASSERT_TRUE(pal.GetColor(8, col)); // bright[8] to #ABCDEF
    EXPECT_EQ(col, IM_COL32(0xAB, 0xCD, 0xEF, 255));

    ASSERT_TRUE(pal.GetColor(20, col)); // cube index 20 to 1,2,3
    EXPECT_EQ(col, IM_COL32(1, 2, 3, 255));

    ASSERT_TRUE(pal.GetColor(240, col)); // gray index 240 with alpha 40
    EXPECT_EQ(col, IM_COL32(10, 20, 30, 40));

    ASSERT_TRUE(pal.GetColor(196, col)); // override any index
    EXPECT_EQ(col, IM_COL32(255, 0, 0, 255));
}

TEST_F(AnsiPaletteTest, ThemeChainResolution) {
    // Create themes dir and parent/child themes
    utils::CreateDirectoryW(ThemesDir());
    std::wstring parentP = ThemesDir(); parentP.append(L"\\parent.ini");
    std::wstring childP = ThemesDir(); childP.append(L"\\child.ini");

    std::wstring parent =
        L"# theme: parent\n[standard]\nblue = #010203\n";
    std::wstring child =
        L"# theme: child\n[theme]\nbase = parent\n\n[bright]\nred = #040506\n";
    utils::WriteTextFileW(parentP, parent);
    utils::WriteTextFileW(childP, child);

    // Main config referencing child
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = child\n");

    AnsiPalette pal;
    EXPECT_TRUE(pal.ReloadFromFile());

    ImU32 col = 0;
    // bright red (index 9) from child
    ASSERT_TRUE(pal.GetColor(9, col));
    EXPECT_EQ(col, IM_COL32(0x04, 0x05, 0x06, 255));

    // standard blue (index 4) from parent
    ASSERT_TRUE(pal.GetColor(4, col));
    EXPECT_EQ(col, IM_COL32(0x01, 0x02, 0x03, 255));

    // Chain inspection
    auto chain = pal.GetResolvedThemeChain();
    ASSERT_GE(chain.size(), 2u);
    EXPECT_EQ(chain[0].name, std::string("child"));
    EXPECT_TRUE(chain[0].exists);
    EXPECT_EQ(chain[1].name, std::string("parent"));
    EXPECT_TRUE(chain[1].exists);
}

TEST_F(AnsiPaletteTest, AvailableThemesAndActiveName) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\t1.ini", L"# t1\n");
    utils::WriteTextFileW(ThemesDir() + L"\\t2.ini", L"# t2\n");
    utils::WriteTextFileW(ThemesDir() + L"\\t3.theme", L"# t3\n");

    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = t2\n");

    AnsiPalette pal;
    pal.EnsureInitialized();

    auto names = pal.GetAvailableThemes();
    // Expect at least these names present
    std::vector<std::string> required = {"t1", "t2", "t3"};
    for (const auto &n : required) {
        auto it = std::find(names.begin(), names.end(), n);
        EXPECT_NE(it, names.end());
    }

    EXPECT_EQ(pal.GetActiveThemeName(), std::string("t2"));
}

TEST_F(AnsiPaletteTest, SetActiveThemeNameWritesConfig) {
    AnsiPalette pal;
    EXPECT_TRUE(pal.SetActiveThemeName("nord"));

    std::wstring cfg = utils::ReadTextFileW(LoaderCfg());
    std::string c = utils::Utf16ToAnsi(cfg);
    EXPECT_NE(c.find("[theme]"), std::string::npos);
    EXPECT_NE(c.find("base = nord"), std::string::npos);

    // Clear base
    EXPECT_TRUE(pal.SetActiveThemeName("none"));
    cfg = utils::ReadTextFileW(LoaderCfg());
    c = utils::Utf16ToAnsi(cfg);
    // Verify ExtractThemeName returns none/empty
    EXPECT_TRUE(AnsiPalette::ExtractThemeName(c).empty());
    // Ensure no non-comment 'base' assignment remains inside [theme] or [meta]
    bool found_active = false;
    std::string section;
    size_t pos = 0;
    auto trim = [&](std::string s){ return utils::TrimStringCopy(s); };
    while (pos <= c.size()) {
        size_t e = c.find_first_of("\r\n", pos);
        size_t n = (e == std::string::npos) ? (c.size() - pos) : (e - pos);
        std::string line = c.substr(pos, n);
        pos = (e == std::string::npos) ? c.size() + 1 : (e + 1);
        std::string t = trim(line);
        if (t.empty()) continue;
        if (t.front() == '[' && t.back() == ']') { section = utils::ToLower(t.substr(1, t.size()-2)); continue; }
        if (t[0] == '#' || t[0] == ';') continue;
        if (section == "theme" || section == "meta") {
            std::string l = utils::ToLower(t);
            if (utils::StartsWith(l, std::string("base")) || utils::StartsWith(l, std::string("theme"))) {
                found_active = true; break;
            }
        }
    }
    EXPECT_FALSE(found_active);
}

TEST_F(AnsiPaletteTest, SetActiveThemeNameAppendsWhenMissingThemeSection) {
    // Start with empty file
    utils::WriteTextFileW(LoaderCfg(), L"# empty\n");
    AnsiPalette pal;
    EXPECT_TRUE(pal.SetActiveThemeName("alpha"));
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    EXPECT_NE(c.find("[theme]"), std::string::npos);
    EXPECT_NE(c.find("base = alpha"), std::string::npos);
}

TEST_F(AnsiPaletteTest, SetActiveThemeNameIdempotentAndRewrites) {
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = beta\n# comment\n");
    AnsiPalette pal;
    // Set to same name twice
    EXPECT_TRUE(pal.SetActiveThemeName("beta"));
    EXPECT_TRUE(pal.SetActiveThemeName("beta"));
    // Expect a single base line directly after header
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    int themePos = (int)c.find("[theme]");
    ASSERT_NE(themePos, -1);
    // Count base lines
    int count = 0; size_t pos = 0;
    while ((pos = c.find("base =", pos)) != std::string::npos) { ++count; pos += 6; }
    EXPECT_EQ(count, 1);
}

TEST_F(AnsiPaletteTest, ResolveThemePathRecognizesExtensions) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\y.ini", L"# y\n");
    utils::WriteTextFileW(ThemesDir() + L"\\z.theme", L"# z\n");
    AnsiPalette pal;
    auto pathY = pal.ResolveThemePathW(L"y");
    auto pathZ = pal.ResolveThemePathW(L"z");
    EXPECT_TRUE(utils::FileExistsW(pathY));
    EXPECT_TRUE(utils::FileExistsW(pathZ));
    // .cfg is not supported (implicit or explicit)
    utils::WriteTextFileW(ThemesDir() + L"\\x.cfg", L"# x\n");
    auto pathX = pal.ResolveThemePathW(L"x");
    auto pathX2 = pal.ResolveThemePathW(L"x.cfg");
    EXPECT_FALSE(utils::FileExistsW(pathX));
    EXPECT_TRUE(pathX.empty());
    EXPECT_TRUE(pathX2.empty());
}

TEST_F(AnsiPaletteTest, ThemeChainCycleStops) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\a.ini", L"[theme]\nbase = b\n");
    utils::WriteTextFileW(ThemesDir() + L"\\b.ini", L"[theme]\nbase = a\n");
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = a\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    auto chain = pal.GetResolvedThemeChain();
    // Should contain a and b, and then stop
    ASSERT_GE(chain.size(), 2u);
}

TEST_F(AnsiPaletteTest, ExtractThemeNameTrimsAndCaseInsensitive) {
    std::wstring cfg = L"[theme]\n  BASE =  Child  \n";
    auto s = utils::Utf16ToAnsi(cfg);
    EXPECT_EQ(AnsiPalette::ExtractThemeName(s), std::string("child"));
}

TEST_F(AnsiPaletteTest, ToningSaturationChangesColor) {
    // toning with saturation modification should alter palette colors
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n toning = on\n tone_saturation = 0.2\n");
    AnsiPalette pal;
    EXPECT_TRUE(pal.ReloadFromFile());
    ImU32 col = 0;
    ASSERT_TRUE(pal.GetColor(7, col)); // default white (192,192,192) gains saturation -> changes
    EXPECT_NE(col, IM_COL32(192, 192, 192, 255));
}

TEST_F(AnsiPaletteTest, MixStrengthAffectsCubeAndGray) {
    // Prepare theme with custom primaries and endpoints
    utils::CreateDirectoryW(ThemesDir());
    std::wstring mixThemeP = ThemesDir(); mixThemeP.append(L"\\mix.ini");
    std::wstring mixTheme =
        L"# theme: mix\n[theme]\n\n[bright]\nred = #FF8080\ngreen = #80FF80\nblue = #8080FF\n\n[standard]\nblack = #101010\nwhite = #F0F0F0\n";
    utils::WriteTextFileW(mixThemeP, mixTheme);

    // mix_strength = 1 -> full theme mix
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n base = mix\n cube = theme\n gray = theme\n mix_strength = 1\n");
    {
        AnsiPalette pal;
        EXPECT_TRUE(pal.ReloadFromFile());
        ImU32 c = 0;
        // Cube index with full red (r6=5,g6=0,b6=0) -> should equal theme bright red (#FF8080)
        ASSERT_TRUE(pal.GetColor(196, c));
        EXPECT_EQ(c, IM_COL32(0xFF, 0x80, 0x80, 255));
        // Gray start index 232 (gray=8) should differ from standard 8 gray when fully themed
        ASSERT_TRUE(pal.GetColor(232, c));
        EXPECT_NE(c, IM_COL32(8, 8, 8, 255));
    }

    // mix_strength = 0 -> fall back to pure standard values even if cube/gray=theme
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n base = mix\n cube = theme\n gray = theme\n mix_strength = 0\n");
    {
        AnsiPalette pal;
        EXPECT_TRUE(pal.ReloadFromFile());
        ImU32 c = 0;
        ASSERT_TRUE(pal.GetColor(196, c));
        EXPECT_EQ(c, IM_COL32(255, 0, 0, 255));
        ASSERT_TRUE(pal.GetColor(244, c));
        EXPECT_EQ(c, IM_COL32(128, 128, 128, 255));
        ASSERT_TRUE(pal.GetColor(232, c));
        EXPECT_EQ(c, IM_COL32(8, 8, 8, 255));
    }
}

TEST_F(AnsiPaletteTest, ToningBrightnessChangesColor) {
    // toning with brightness modification should alter palette colors
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n toning = on\n tone_brightness = 0.2\n");
    AnsiPalette pal;
    EXPECT_TRUE(pal.ReloadFromFile());
    ImU32 col = 0;
    ASSERT_TRUE(pal.GetColor(4, col)); // default blue (0,0,128) becomes lighter
    EXPECT_NE(col, IM_COL32(0, 0, 128, 255));
}
