#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <cwchar>

#include "AnsiPalette.h"
#include "Utils/StringUtils.h"
#include "Utils/PathUtils.h"

#include "imgui.h"

// --------------------
// Minimal ModContext stubs for tests
// --------------------

static std::wstring g_TestLoaderDir; // points to a temp test directory
static std::vector<std::string> g_TestLogs;
static void TestLogSink(int level, const char *message) {
    // store level prefix for easier assertions
    char prefix = (level == 2 ? 'E' : (level == 1 ? 'W' : 'I'));
    std::string line;
    line += prefix;
    line += ':';
    if (message) line += message;
    g_TestLogs.push_back(std::move(line));
}

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
        // Inject test logger
        AnsiPalette::SetLoggerProvider(&TestLogSink);
        g_TestLogs.clear();
    }

    void TearDown() override {
        if (!g_TestLoaderDir.empty()) {
            utils::DeleteDirectoryW(g_TestLoaderDir);
            g_TestLoaderDir.clear();
        }
        AnsiPalette::SetLoaderDirProvider(nullptr);
        AnsiPalette::SetLoggerProvider(nullptr);
        g_TestLogs.clear();
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

    // Expect a warning for invalid override range '100-xx'
    bool sawWarn = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("invalid range") != std::string::npos) { sawWarn = true; break; }
    }
    EXPECT_TRUE(sawWarn);
}

TEST_F(AnsiPaletteTest, InvalidColorParsingLogsWarning) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[overrides]\n25 = NOTACOLOR\n26 = #GGHHII\n27 = 1,2\n300 = #FFFFFF\n"; // 300 is out of range -> warn
    utils::WriteTextFileW(cfg, content);

    AnsiPalette pal;
    pal.ReloadFromFile();
    // Should log warnings for invalid color values and out-of-range index
    bool sawInvalidColor = false, sawOutOfRange = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("invalid color value") != std::string::npos) sawInvalidColor = true;
        if (s.rfind("W:", 0) == 0 && s.find("override index out of range") != std::string::npos) sawOutOfRange = true;
    }
    EXPECT_TRUE(sawInvalidColor);
    EXPECT_TRUE(sawOutOfRange);
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
    // Ensure no non-comment 'base' assignment remains inside [theme]
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
        if (section == "theme") {
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

TEST_F(AnsiPaletteTest, FlexibleColorSeparators) {
    // Verify that various separators for RGB[A] are accepted
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[overrides]\n"
        L"10 = 1 2 3 4\n"      // spaces
        L"11 = 5;6;7;8\n"      // semicolons
        L"12 = 9/10/11/12\n"    // slashes
        L"13 = 13|14|15\n"     // pipes, 3-tuple
        L"14 = 16:17:18\n";    // colons
    utils::WriteTextFileW(cfg, content);

    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(10, c)); EXPECT_EQ(c, IM_COL32(1, 2, 3, 4));
    ASSERT_TRUE(pal.GetColor(11, c)); EXPECT_EQ(c, IM_COL32(5, 6, 7, 8));
    ASSERT_TRUE(pal.GetColor(12, c)); EXPECT_EQ(c, IM_COL32(9, 10, 11, 12));
    ASSERT_TRUE(pal.GetColor(13, c)); EXPECT_EQ(c, IM_COL32(13, 14, 15, 255));
    ASSERT_TRUE(pal.GetColor(14, c)); EXPECT_EQ(c, IM_COL32(16, 17, 18, 255));
}

TEST_F(AnsiPaletteTest, MissingThemeLogsWarning) {
    // Reference a non-existent theme and verify a warning is logged
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = no-such-theme\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    bool sawWarn = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("theme not found") != std::string::npos) { sawWarn = true; break; }
    }
    EXPECT_TRUE(sawWarn);
}

TEST_F(AnsiPaletteTest, LoggerProviderGetSet) {
    // Ensure getter/setter work and can be cleared
    auto prev = AnsiPalette::GetLoggerProvider();
    AnsiPalette::SetLoggerProvider(nullptr);
    EXPECT_EQ(AnsiPalette::GetLoggerProvider(), nullptr);
    AnsiPalette::SetLoggerProvider(&TestLogSink);
    EXPECT_NE(AnsiPalette::GetLoggerProvider(), nullptr);
    // restore
    AnsiPalette::SetLoggerProvider(prev);
}

TEST_F(AnsiPaletteTest, BackcompatTopLevelIndexOverride) {
    // No section: numeric key should override directly
    utils::WriteTextFileW(LoaderCfg(), L"196 = 1,2,3\n");
    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(196, c));
    EXPECT_EQ(c, IM_COL32(1, 2, 3, 255));
}

TEST_F(AnsiPaletteTest, CubeAndGraySectionRangesRespected) {
    // Values outside allowed index ranges for [cube]/[gray] are ignored
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[cube]\n15 = 1,2,3\n16 = 4,5,6\n\n"  // 15 ignored, 16 accepted
        L"[gray]\n200 = 7,8,9\n232 = 10,11,12\n"; // 200 ignored, 232 accepted
    utils::WriteTextFileW(cfg, content);

    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(15, c)); // bright white default
    EXPECT_EQ(c, IM_COL32(255, 255, 255, 255));
    ASSERT_TRUE(pal.GetColor(16, c));
    EXPECT_EQ(c, IM_COL32(4, 5, 6, 255));
    ASSERT_TRUE(pal.GetColor(232, c));
    EXPECT_EQ(c, IM_COL32(10, 11, 12, 255));
}

TEST_F(AnsiPaletteTest, OverridesRangeAppliesInclusive) {
    // Range override applies to every index inclusive
    std::wstring cfg = LoaderCfg();
    utils::WriteTextFileW(cfg, L"[overrides]\n20-22 = 1,2,3\n");
    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0;
    for (int i = 20; i <= 22; ++i) { ASSERT_TRUE(pal.GetColor(i, c)); EXPECT_EQ(c, IM_COL32(1, 2, 3, 255)); }
}

TEST_F(AnsiPaletteTest, ThemeKeySynonymsWork) {
    // Use synonyms for keys and values
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[theme]\n"
        L"cube_mode = theme\n"
        L"grey_mode = theme\n"
        L"mix_colorspace = linear\n"
        L"tone_enable = 1\n"
        L"mix_strength = 50%\n";
    utils::WriteTextFileW(cfg, content);

    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_TRUE(pal.GetCubeMixFromTheme());
    EXPECT_TRUE(pal.GetGrayMixFromTheme());
    EXPECT_TRUE(pal.GetLinearMix());
    EXPECT_TRUE(pal.GetToningEnabled());
    EXPECT_NEAR(pal.GetMixStrength(), 0.5f, 1e-3f);
}

TEST_F(AnsiPaletteTest, SetThemeOptionValidationAndReload) {
    AnsiPalette pal;
    // create minimal config and set options through API
    EXPECT_TRUE(pal.SetThemeOption("cube", "theme"));
    EXPECT_TRUE(pal.SetThemeOption("gray", "theme"));
    EXPECT_TRUE(pal.SetThemeOption("toning", "on"));
    EXPECT_TRUE(pal.SetThemeOption("mix_space", "linear"));
    EXPECT_TRUE(pal.SetThemeOption("mix_strength", "50%"));
    EXPECT_TRUE(pal.SetThemeOption("tone_brightness", "2.0")); // clamp to 1.0
    EXPECT_TRUE(pal.SetThemeOption("tone_saturation", "-2.0")); // clamp to -1.0

    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_TRUE(pal.GetCubeMixFromTheme());
    EXPECT_TRUE(pal.GetGrayMixFromTheme());
    EXPECT_TRUE(pal.GetLinearMix());
    EXPECT_TRUE(pal.GetToningEnabled());
    EXPECT_NEAR(pal.GetMixStrength(), 0.5f, 1e-3f);
    // Internal getters for tone values exist; verify clamps
    EXPECT_NEAR(pal.GetToneBrightness(), 1.0f, 1e-3f);
    EXPECT_NEAR(pal.GetToneSaturation(), -1.0f, 1e-3f);
}

TEST_F(AnsiPaletteTest, ResetThemeOptionsRemovesThemeSection) {
    // Write some theme options
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n cube = theme\n base = abc\n");
    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_TRUE(pal.ResetThemeOptions());
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    EXPECT_EQ(c.find("[theme]"), std::string::npos);
}

TEST_F(AnsiPaletteTest, AvailableThemesDeduplicatesExtensions) {
    utils::CreateDirectoryW(ThemesDir());
    // same base with two extensions
    utils::WriteTextFileW(ThemesDir() + L"\\dup.ini", L"# dup\n");
    utils::WriteTextFileW(ThemesDir() + L"\\dup.theme", L"# dup\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    auto names = pal.GetAvailableThemes();
    int count = 0; for (auto &n : names) if (n == "dup") ++count;
    EXPECT_EQ(count, 1);
}

TEST_F(AnsiPaletteTest, ResolveThemePathExtensionCaseInsensitive) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\Case.ini", L"# x\n");
    AnsiPalette pal;
    auto p = pal.ResolveThemePathW(L"Case.INI");
    EXPECT_TRUE(utils::FileExistsW(p));
}

TEST_F(AnsiPaletteTest, ExtractThemeNameSupportsThemeKey) {
    std::wstring cfg = L"[theme]\n  THEME =  Parent  \n";
    auto s = utils::Utf16ToAnsi(cfg);
    EXPECT_EQ(AnsiPalette::ExtractThemeName(s), std::string("parent"));
}

TEST_F(AnsiPaletteTest, ChainIncludesMissingParentWithExistsFalse) {
    utils::CreateDirectoryW(ThemesDir());
    // child exists, parent missing
    utils::WriteTextFileW(ThemesDir() + L"\\child.ini", L"[theme]\nbase = parent\n");
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = child\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    auto chain = pal.GetResolvedThemeChain();
    ASSERT_GE(chain.size(), 2u);
    EXPECT_EQ(chain[0].name, std::string("child"));
    EXPECT_TRUE(chain[0].exists);
    EXPECT_EQ(chain[1].name, std::string("parent"));
    EXPECT_FALSE(chain[1].exists);
}

TEST_F(AnsiPaletteTest, ActiveThemeNoneYieldsEmpty) {
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = none\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    EXPECT_TRUE(pal.GetActiveThemeName().empty());
}

TEST_F(AnsiPaletteTest, CubeLinearVsSrgbMixHalf) {
    // Prepare mix theme for primaries and endpoints
    utils::CreateDirectoryW(ThemesDir());
    std::wstring mixThemeP = ThemesDir(); mixThemeP.append(L"\\mix.ini");
    std::wstring mixTheme =
        L"# theme: mix\n[theme]\n\n[bright]\nred = #FF8080\ngreen = #80FF80\nblue = #8080FF\n\n[standard]\nblack = #101010\nwhite = #F0F0F0\n";
    utils::WriteTextFileW(mixThemeP, mixTheme);

    // sRGB mixing, half strength -> g/b should be 64
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n base = mix\n cube = theme\n gray = theme\n mix_strength = 0.5\n mix_space = srgb\n");
    {
        AnsiPalette pal;
        ASSERT_TRUE(pal.ReloadFromFile());
        ImU32 c = 0; ASSERT_TRUE(pal.GetColor(196, c));
        EXPECT_EQ(c, IM_COL32(255, 64, 64, 255));
    }

    // linear mixing, half strength -> g/b should be greater than 64
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n base = mix\n cube = theme\n gray = theme\n mix_strength = 0.5\n mix_space = linear\n");
    {
        AnsiPalette pal;
        ASSERT_TRUE(pal.ReloadFromFile());
        ImU32 c = 0; ASSERT_TRUE(pal.GetColor(196, c));
        int r = (c >> IM_COL32_R_SHIFT) & 0xFF;
        int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
        int b = (c >> IM_COL32_B_SHIFT) & 0xFF;
        EXPECT_EQ(r, 255);
        EXPECT_GT(g, 64);
        EXPECT_GT(b, 64);
    }
}

TEST_F(AnsiPaletteTest, GrayMixHalfIncreasesMidGray) {
    // With mix 0.5, mid gray should be slightly above standard
    utils::CreateDirectoryW(ThemesDir());
    std::wstring mixThemeP = ThemesDir(); mixThemeP.append(L"\\mix.ini");
    std::wstring mixTheme =
        L"# theme: mix\n[theme]\n\n[standard]\nblack = #101010\nwhite = #F0F0F0\n";
    utils::WriteTextFileW(mixThemeP, mixTheme);

    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n base = mix\n gray = theme\n mix_strength = 0.5\n mix_space = srgb\n");
    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0; ASSERT_TRUE(pal.GetColor(244, c));
    int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
    EXPECT_GT(g, 128); // standard mid is 128
}

TEST_F(AnsiPaletteTest, ToningOnBrightnessMaxTurnsBlackToWhite) {
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\n toning = on\n tone_brightness = 1.0\n");
    AnsiPalette pal;
    ASSERT_TRUE(pal.ReloadFromFile());
    ImU32 c = 0; ASSERT_TRUE(pal.GetColor(0, c));
    EXPECT_EQ(c, IM_COL32(255, 255, 255, 255));
}

TEST_F(AnsiPaletteTest, ThemeCycleLogsWarning) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\a.ini", L"[theme]\nbase = b\n");
    utils::WriteTextFileW(ThemesDir() + L"\\b.ini", L"[theme]\nbase = a\n");
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = a\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    bool sawCycle = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("theme cycle detected") != std::string::npos) { sawCycle = true; break; }
    }
    EXPECT_TRUE(sawCycle);
}

TEST_F(AnsiPaletteTest, StandardAndBrightNamedKeys) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[standard]\nred=#010203\nblue=#040506\n\n"
        L"[bright]\nred=#0A0B0C\nwhite=#0D0E0F\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(1, c)); EXPECT_EQ(c, IM_COL32(1,2,3,255));
    ASSERT_TRUE(pal.GetColor(4, c)); EXPECT_EQ(c, IM_COL32(4,5,6,255));
    ASSERT_TRUE(pal.GetColor(9, c)); EXPECT_EQ(c, IM_COL32(10,11,12,255));
    ASSERT_TRUE(pal.GetColor(15, c)); EXPECT_EQ(c, IM_COL32(13,14,15,255));
}

TEST_F(AnsiPaletteTest, UnknownSectionNumericFallback) {
    std::wstring cfg = LoaderCfg();
    std::wstring content = L"[foo]\n200 = 1,2,3\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0; ASSERT_TRUE(pal.GetColor(200, c)); EXPECT_EQ(c, IM_COL32(1,2,3,255));
}

TEST_F(AnsiPaletteTest, BrightNumericBoundariesIgnored) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[bright]\n7 = #111213\n8 = #141516\n16 = #171819\n"; // 7 and 16 ignored for [bright]
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(7, c));   EXPECT_EQ(c, IM_COL32(192,192,192,255));
    ASSERT_TRUE(pal.GetColor(8, c));   EXPECT_EQ(c, IM_COL32(20,21,22,255));
    ASSERT_TRUE(pal.GetColor(16, c));  EXPECT_EQ(c, IM_COL32(0,0,0,255));
}

TEST_F(AnsiPaletteTest, StandardNumericBoundariesIgnored) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[standard]\n8 = #111213\n0 = #010203\n"; // 8 ignored for [standard]
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(8, c));  EXPECT_EQ(c, IM_COL32(128,128,128,255));
    ASSERT_TRUE(pal.GetColor(0, c));  EXPECT_EQ(c, IM_COL32(1,2,3,255));
}

TEST_F(AnsiPaletteTest, MainConfigOverridesThemeChain) {
    utils::CreateDirectoryW(ThemesDir());
    utils::WriteTextFileW(ThemesDir() + L"\\parent.ini", L"[standard]\nred=#010203\n");
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = parent\n\n[standard]\nred=#0F0E0D\n");
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c=0; ASSERT_TRUE(pal.GetColor(1, c)); EXPECT_EQ(c, IM_COL32(15,14,13,255));
}

TEST_F(AnsiPaletteTest, ToningPreservesAlpha) {
    std::wstring cfg = LoaderCfg();
    std::wstring content = L"[theme]\n toning=on\n tone_brightness=0.1\n\n[gray]\n 240 = 10,20,30,40\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c=0; ASSERT_TRUE(pal.GetColor(240, c));
    EXPECT_EQ((c >> IM_COL32_A_SHIFT) & 0xFF, 40);
}

TEST_F(AnsiPaletteTest, ReloadFromFileMissingReturnsFalse) {
    utils::DeleteFileW(LoaderCfg());
    AnsiPalette pal; EXPECT_FALSE(pal.ReloadFromFile());
}

TEST_F(AnsiPaletteTest, EnsureInitializedWithEmptyFileKeepsDefaults) {
    utils::WriteTextFileW(LoaderCfg(), L"\n");
    AnsiPalette pal; pal.EnsureInitialized();
    ImU32 c=0; ASSERT_TRUE(pal.GetColor(0, c)); EXPECT_EQ(c, IM_COL32(0,0,0,255));
}

TEST_F(AnsiPaletteTest, MixStrengthNormalizationVariants) {
    AnsiPalette pal;
    // 150% -> 1.000
    ASSERT_TRUE(pal.SetThemeOption("mix_strength", "150%"));
    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_NEAR(pal.GetMixStrength(), 1.0f, 1e-3f);
    {
        std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
        size_t cnt = 0, pos = 0; while ((pos = c.find("mix_strength =", pos)) != std::string::npos) { ++cnt; ++pos; }
        EXPECT_EQ(cnt, 1u);
        EXPECT_NE(c.find("mix_strength = 1.000"), std::string::npos);
    }

    // 1.5 -> 0.015 (treated as percent)
    ASSERT_TRUE(pal.SetThemeOption("mix_strength", "1.5"));
    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_NEAR(pal.GetMixStrength(), 0.015f, 1e-3f);
    {
        std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
        size_t cnt = 0, pos = 0; while ((pos = c.find("mix_strength =", pos)) != std::string::npos) { ++cnt; ++pos; }
        EXPECT_EQ(cnt, 1u);
        EXPECT_NE(c.find("mix_strength = 0.015"), std::string::npos);
    }

    // 0.5 -> 0.500
    ASSERT_TRUE(pal.SetThemeOption("mix_strength", "0.5"));
    ASSERT_TRUE(pal.ReloadFromFile());
    EXPECT_NEAR(pal.GetMixStrength(), 0.5f, 1e-3f);
    {
        std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
        size_t cnt = 0, pos = 0; while ((pos = c.find("mix_strength =", pos)) != std::string::npos) { ++cnt; ++pos; }
        EXPECT_EQ(cnt, 1u);
        EXPECT_NE(c.find("mix_strength = 0.500"), std::string::npos);
    }
}

TEST_F(AnsiPaletteTest, LinearMixPrecedenceWithMixSpace) {
    std::wstring cfg = LoaderCfg();
    std::wstring content = L"[theme]\n linear_mix = on\n mix_space = srgb\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    EXPECT_FALSE(pal.GetLinearMix()); // last writer wins -> srgb
}

TEST_F(AnsiPaletteTest, InvalidNamedColorLogsWarning) {
    std::wstring cfg = LoaderCfg();
    std::wstring content = L"[standard]\nred = not-a-color\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    bool saw = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("invalid color value") != std::string::npos) { saw = true; break; }
    }
    EXPECT_TRUE(saw);
}

TEST_F(AnsiPaletteTest, NoneInMainConfigYieldsEmptyChain) {
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = none\n");
    AnsiPalette pal;
    pal.EnsureInitialized();
    auto chain = pal.GetResolvedThemeChain();
    EXPECT_TRUE(chain.empty());
}

TEST_F(AnsiPaletteTest, NoneInThemeStopsChain) {
    utils::CreateDirectoryW(ThemesDir());
    // child exists and declares base = none
    utils::WriteTextFileW(ThemesDir() + L"\\child.ini", L"[theme]\nbase = none\n");
    // main config references child
    utils::WriteTextFileW(LoaderCfg(), L"[theme]\nbase = child\n");
    AnsiPalette pal; pal.EnsureInitialized();
    auto chain = pal.GetResolvedThemeChain();
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0].name, std::string("child"));
    EXPECT_TRUE(chain[0].exists);
}

TEST_F(AnsiPaletteTest, ExtractThemeNameThemeNone) {
    std::wstring cfg = L"[theme]\n  THEME =   NONE  \n";
    auto s = utils::Utf16ToAnsi(cfg);
    EXPECT_TRUE(AnsiPalette::ExtractThemeName(s).empty());
}

TEST_F(AnsiPaletteTest, LoaderDirProviderGetSet) {
    auto prev = AnsiPalette::GetLoaderDirProvider();
    EXPECT_NE(prev, nullptr);
    AnsiPalette::SetLoaderDirProvider(nullptr);
    EXPECT_EQ(AnsiPalette::GetLoaderDirProvider(), nullptr);
    // restore and confirm
    AnsiPalette::SetLoaderDirProvider(prev);
    EXPECT_EQ(AnsiPalette::GetLoaderDirProvider(), prev);
}

TEST_F(AnsiPaletteTest, SetThemeOptionInvalidValuesRejected) {
    AnsiPalette pal;
    // Ensure file exists with minimal header via first failing set
    EXPECT_FALSE(pal.SetThemeOption("cube", "invalid-mode"));
    EXPECT_FALSE(pal.SetThemeOption("gray", "???"));
    EXPECT_FALSE(pal.SetThemeOption("mix_space", "unknown"));
    EXPECT_FALSE(pal.SetThemeOption("mix_strength", "abc"));
    EXPECT_FALSE(pal.SetThemeOption("tone_brightness", "nan"));
    EXPECT_FALSE(pal.SetThemeOption("tone_saturation", "nan"));

    // Verify no invalid assignments were written (ignore comments and defaults in sample)
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    bool hasInvalid = false;
    size_t pos = 0;
    auto trim = [](const std::string &s){ return utils::TrimStringCopy(s); };
    while (pos <= c.size()) {
        size_t e = c.find_first_of("\r\n", pos);
        size_t n = (e == std::string::npos) ? (c.size() - pos) : (e - pos);
        std::string line = c.substr(pos, n);
        pos = (e == std::string::npos) ? c.size() + 1 : (e + 1);
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        // these exact invalid writes must not be present
        if (t == "cube = invalid-mode" || t == "gray = ???" || t == "mix_space = unknown" ||
            t == "mix_strength = abc" || t == "tone_brightness = nan" || t == "tone_saturation = nan") {
            hasInvalid = true; break;
        }
    }
    EXPECT_FALSE(hasInvalid);
}

TEST_F(AnsiPaletteTest, SetThemeOptionUnknownKeyPassthrough) {
    AnsiPalette pal;
    EXPECT_TRUE(pal.SetThemeOption("custom_key", "custom value"));
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    // Must appear in [theme] with the same spelling/value (canonicalization only applies to known keys)
    EXPECT_NE(c.find("custom_key = custom value"), std::string::npos);
}

TEST_F(AnsiPaletteTest, ToneClampsAndFormatsWrittenValues) {
    AnsiPalette pal;
    ASSERT_TRUE(pal.SetThemeOption("tone_brightness", "2.0"));
    ASSERT_TRUE(pal.SetThemeOption("tone_saturation", "-2.0"));
    ASSERT_TRUE(pal.ReloadFromFile());
    // Values clamped and formatted to 3 decimals in file
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    size_t cntB = 0, posB = 0; while ((posB = c.find("tone_brightness =", posB)) != std::string::npos) { ++cntB; ++posB; }
    size_t cntS = 0, posS = 0; while ((posS = c.find("tone_saturation =", posS)) != std::string::npos) { ++cntS; ++posS; }
    EXPECT_EQ(cntB, 1u);
    EXPECT_EQ(cntS, 1u);
    EXPECT_NE(c.find("tone_brightness = 1.000"), std::string::npos);
    EXPECT_NE(c.find("tone_saturation = -1.000"), std::string::npos);
}

TEST_F(AnsiPaletteTest, ToneRewriteDoesNotDuplicateKeys) {
    AnsiPalette pal;
    // First write
    ASSERT_TRUE(pal.SetThemeOption("tone_brightness", "0.1"));
    ASSERT_TRUE(pal.SetThemeOption("tone_saturation", "0.3"));
    // Rewrite with different values
    ASSERT_TRUE(pal.SetThemeOption("tone_brightness", "0.2"));
    ASSERT_TRUE(pal.SetThemeOption("tone_saturation", "-0.4"));
    ASSERT_TRUE(pal.ReloadFromFile());

    // Only one line per key and value normalized to 3 decimals
    std::string c = utils::Utf16ToAnsi(utils::ReadTextFileW(LoaderCfg()));
    size_t cntB = 0, posB = 0; while ((posB = c.find("tone_brightness =", posB)) != std::string::npos) { ++cntB; ++posB; }
    size_t cntS = 0, posS = 0; while ((posS = c.find("tone_saturation =", posS)) != std::string::npos) { ++cntS; ++posS; }
    EXPECT_EQ(cntB, 1u);
    EXPECT_EQ(cntS, 1u);
    EXPECT_NE(c.find("tone_brightness = 0.200"), std::string::npos);
    EXPECT_NE(c.find("tone_saturation = -0.400"), std::string::npos);
}

TEST_F(AnsiPaletteTest, OverridesRangeClampingAppliesToBoundaries) {
    std::wstring cfg = LoaderCfg();
    std::wstring content = L"[overrides]\n -5-300 = 1,2,3 \n"; // clamps to 0..255
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c=0;
    ASSERT_TRUE(pal.GetColor(0, c));   EXPECT_EQ(c, IM_COL32(1,2,3,255));
    ASSERT_TRUE(pal.GetColor(255, c)); EXPECT_EQ(c, IM_COL32(1,2,3,255));
}

TEST_F(AnsiPaletteTest, OverridesInvalidDoubleDashRangesWarnAndNotApplied) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[overrides]\n300--5 = #FF0000\n5--3 = 1,2,3\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();

    bool sawWarn = false;
    for (const auto &s : g_TestLogs) {
        if (s.rfind("W:", 0) == 0 && s.find("invalid range") != std::string::npos) { sawWarn = true; break; }
    }
    EXPECT_TRUE(sawWarn);

    // Ensure not applied: indices 3,4,5 remain defaults
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(3, c)); EXPECT_EQ(c, IM_COL32(128,128,0,255));
    ASSERT_TRUE(pal.GetColor(4, c)); EXPECT_EQ(c, IM_COL32(0,0,128,255));
    ASSERT_TRUE(pal.GetColor(5, c)); EXPECT_EQ(c, IM_COL32(128,0,128,255));
}

TEST_F(AnsiPaletteTest, ParseCRLFCommentsWhitespaceConsistency) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[standard]\r\n"
        L"  # comment above red\r\n"
        L"  red   =   1 ,  2 , 3   \r\n"
        L"; another comment\r\n\r\n"
        L"[overrides]\r\n"
        L"   100   =   4 ,5 ,  6  \r\n";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(1, c)); EXPECT_EQ(c, IM_COL32(1,2,3,255));
    ASSERT_TRUE(pal.GetColor(100, c)); EXPECT_EQ(c, IM_COL32(4,5,6,255));
}

TEST_F(AnsiPaletteTest, ParseOnlyCRNewlinesConsistency) {
    std::wstring cfg = LoaderCfg();
    // Only CR separators, include comments and extra spaces
    std::wstring content =
        L"[standard]\r"
        L"  green=  10 , 20 ,30 \r"
        L"; comment\r"
        L"[overrides]\r"
        L"  101 =  7 , 8 ,9 \r";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(2, c));   EXPECT_EQ(c, IM_COL32(10,20,30,255));
    ASSERT_TRUE(pal.GetColor(101, c)); EXPECT_EQ(c, IM_COL32(7,8,9,255));
}

TEST_F(AnsiPaletteTest, ParseMixedCRAndLFNewlinesConsistency) {
    std::wstring cfg = LoaderCfg();
    // Mixed LF and CR separators (note some lines end with LF, some with CR)
    std::wstring content =
        L"[standard]\n"
        L"  yellow = 1,2,3\r"
        L"[bright]\n"
        L"  cyan = 4,5,6\r"
        L"[overrides]\n"
        L"  120 = 7,8,9\r";
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c = 0;
    ASSERT_TRUE(pal.GetColor(3, c));   EXPECT_EQ(c, IM_COL32(1,2,3,255));   // yellow
    ASSERT_TRUE(pal.GetColor(14, c));  EXPECT_EQ(c, IM_COL32(4,5,6,255));   // bright cyan index 14
    ASSERT_TRUE(pal.GetColor(120, c)); EXPECT_EQ(c, IM_COL32(7,8,9,255));
}

TEST_F(AnsiPaletteTest, CubeAndGrayNamedKeysIgnored) {
    std::wstring cfg = LoaderCfg();
    std::wstring content =
        L"[cube]\nred = #010203\n\n" // ignored
        L"[gray]\nwhite = #0A0B0C\n";   // ignored
    utils::WriteTextFileW(cfg, content);
    AnsiPalette pal; pal.ReloadFromFile();
    ImU32 c=0;
    // cube default at 196 = (255,0,0)
    ASSERT_TRUE(pal.GetColor(196, c)); EXPECT_EQ(c, IM_COL32(255,0,0,255));
    // gray ramp start 232 = (8,8,8)
    ASSERT_TRUE(pal.GetColor(232, c)); EXPECT_EQ(c, IM_COL32(8,8,8,255));
}
