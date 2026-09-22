#include "UI/PaletteCommand.h"

#include <cctype>
#include <cstdio>

#include "BML/BML.h"
#include "BML/IBML.h"
#include "Console/Shell/ShellIo.h"
#include "UI/AnsiPalette.h"
#include "UI/AnsiText.h"
#include "StringUtils.h"

void PaletteCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    AnsiPalette &palette = AnsiText::Renderer::DefaultPalette();

    if (args.size() <= 1 || args[1] == "reload") {
        const bool loaded = palette.ReloadFromFile();
        const std::wstring path = palette.GetConfigPathW();
        if (loaded) {
            char *pathAnsi = BML_Utf16ToAnsi(path.c_str());
            bml->SendIngameMessage((std::string("[palette] reloaded from ") + pathAnsi + "\n").c_str());
            delete[] pathAnsi;
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "sample") {
        const bool written = palette.ReloadFromFile();
        const std::wstring path = palette.GetConfigPathW();
        char *pathAnsi = BML_Utf16ToAnsi(path.c_str());
        if (written) {
            bml->SendIngameMessage((std::string("[palette] sample created: ") + pathAnsi + "\n").c_str());
        } else {
            bml->SendIngameMessage((std::string("[palette] sample exists: ") + pathAnsi + "\n").c_str());
        }
        delete[] pathAnsi;
    } else if (args[1] == "list") {
        // List available themes using palette API
        AnsiPalette pal;
        auto names = pal.GetAvailableThemes();
        std::string activeTheme = pal.GetActiveThemeName();
        if (names.empty()) {
            std::wstring themesDir = pal.GetThemesDirW();
            char *dirAnsi = BML_Utf16ToAnsi(themesDir.c_str());
            std::string msg = std::string("[palette] no themes found in ") + (dirAnsi ? dirAnsi : "(null)") + ".";
            if (dirAnsi) delete[] dirAnsi;
            msg += " active: ";
            msg += (activeTheme.empty() ? "none*" : activeTheme.c_str());
            msg += "\n";
            bml->SendIngameMessage(msg.c_str());
        } else {
            std::string line = "[palette] themes";
            line += " (active: ";
            line += (activeTheme.empty() ? "none*" : activeTheme.c_str());
            line += "):";
            std::string activeLower = activeTheme;
            for (char &c : activeLower) c = (char) tolower((unsigned char) c);
            for (const auto &nm : names) {
                std::string nmLower = nm;
                for (char &c : nmLower) c = (char) tolower((unsigned char) c);
                line += " ";
                line += nm;
                if (!activeLower.empty() && nmLower == activeLower) line += "*"; // highlight current
            }
            line += "\n";
            bml->SendIngameMessage(line.c_str());
        }
    } else if (args[1] == "show") {
        // Print resolved theme chain top -> parent -> ... -> root
        AnsiPalette tmp;
        const auto chain = tmp.GetResolvedThemeChain();
        if (chain.empty()) {
            bml->SendIngameMessage("[palette] chain: none*\n");
            return;
        }
        std::string line = "[palette] chain: ";
        for (size_t i = 0; i < chain.size(); ++i) {
            if (i) line += " -> ";
            line += chain[i].name;
            if (!chain[i].exists) line += " (missing)";
        }
        line += "\n";
        bml->SendIngameMessage(line.c_str());
    } else if (args[1] == "theme") {
        if (args.size() < 3) {
            BML::Shell::Fail(bml, "Usage: palette theme <name>\n");
            return;
        }
        const std::string &name = args[2];
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetActiveThemeName(name);
        // Reload
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) {
            std::string nameLower = utils::ToLower(name);
            std::string msg = (nameLower == "none")
                                  ? "[palette] theme cleared, using defaults + local overrides.\n"
                                  : (std::string("[palette] theme set to ") + name + ", reloaded.\n");
            bml->SendIngameMessage(msg.c_str());
        } else if (!ok) {
            BML::Shell::Fail(bml, "[palette] failed to update config.\n");
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "info") {
        // Print current palette options/modes
        AnsiPalette pal;
        pal.ReloadFromFile();
        std::string theme = pal.GetActiveThemeName();
        std::string cube = pal.GetCubeMixFromTheme() ? "theme" : "standard";
        std::string gray = pal.GetGrayMixFromTheme() ? "theme" : "standard";
        std::string space = pal.GetLinearMix() ? "linear" : "srgb";
        float mix = pal.GetMixStrength();
        bool toning = pal.GetToningEnabled();
        float tb = pal.GetToneBrightness();
        float ts = pal.GetToneSaturation();
        char buf[256];
        snprintf(buf, sizeof(buf), "[palette] info: theme=%s cube=%s gray=%s mix=%.2f space=%s toning=%s tb=%.2f ts=%.2f\n",
                 (theme.empty() ? "none" : theme.c_str()), cube.c_str(), gray.c_str(), mix, space.c_str(), toning ? "on" : "off", tb, ts);
        bml->SendIngameMessage(buf);
        // Explanations & tips
        bml->SendIngameMessage("[palette] cube: standard=xterm 6x6x6; theme=from bright primaries\n");
        bml->SendIngameMessage("[palette] gray: standard=xterm gray ramp; theme=black-white mix\n");
        bml->SendIngameMessage("[palette] tips: set cube gray/mix/space/toning via 'palette set'\n");
        bml->SendIngameMessage("          e.g. palette set cube theme | palette set mix_strength 0.7\n");
        bml->SendIngameMessage("          e.g. palette set mix_space linear | palette set gray standard\n");
    } else if (args[1] == "set") {
        // palette set <option> <value>
        if (args.size() < 4) {
            BML::Shell::Fail(bml, "Usage: palette set <cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation> <value>\n");
            return;
        }
        std::string opt = utils::ToLower(args[2]);
        // Join remaining words to accept values like "70 %" typed with a space.
        std::string val = utils::TrimStringCopy(utils::JoinString(args, ' ', 3));
        // Normalize some common shorthands
        if (opt == "linear") opt = "mix_space";
        if (opt == "mix") opt = "mix_strength";
        if (opt == "grey") opt = "gray";
        if (opt == "tone_enable" || opt == "enable_toning") opt = "toning";
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetThemeOption(opt, val);
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) {
            bml->SendIngameMessage("[palette] option updated.\n");
        } else if (!ok) {
            // Provide range tips for numeric keys
            if (opt == "mix_strength") {
                BML::Shell::Fail(bml, "[palette] invalid mix_strength. Expect 0..1 or percent (e.g., 70%).\n");
            } else if (opt == "tone_brightness" || opt == "tone_saturation") {
                BML::Shell::Fail(bml, "[palette] invalid value. Expect in [-1..1].\n");
            } else {
                BML::Shell::Fail(bml, "[palette] failed to update config.\n");
            }
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "get") {
        // palette get <option>
        if (args.size() < 3) {
            BML::Shell::Fail(bml, "Usage: palette get <theme|cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation>\n");
            return;
        }
        std::string key = utils::ToLower(args[2]);
        if (key == "grey") key = "gray";
        if (key == "linear") key = "mix_space";
        AnsiPalette pal;
        pal.ReloadFromFile();
        char buf[192]; buf[0] = 0;
        if (key == "theme" || key == "base") {
            std::string theme = pal.GetActiveThemeName();
            snprintf(buf, sizeof(buf), "[palette] theme = %s\n", theme.empty() ? "none" : theme.c_str());
        } else if (key == "cube") {
            snprintf(buf, sizeof(buf), "[palette] cube = %s\n", pal.GetCubeMixFromTheme() ? "theme" : "standard");
        } else if (key == "gray") {
            snprintf(buf, sizeof(buf), "[palette] gray = %s\n", pal.GetGrayMixFromTheme() ? "theme" : "standard");
        } else if (key == "mix_strength" || key == "mix") {
            snprintf(buf, sizeof(buf), "[palette] mix_strength = %.2f\n", pal.GetMixStrength());
        } else if (key == "mix_space") {
            snprintf(buf, sizeof(buf), "[palette] mix_space = %s\n", pal.GetLinearMix() ? "linear" : "srgb");
        } else if (key == "toning") {
            snprintf(buf, sizeof(buf), "[palette] toning = %s\n", pal.GetToningEnabled() ? "on" : "off");
        } else if (key == "tone_brightness") {
            snprintf(buf, sizeof(buf), "[palette] tone_brightness = %.2f\n", pal.GetToneBrightness());
        } else if (key == "tone_saturation") {
            snprintf(buf, sizeof(buf), "[palette] tone_saturation = %.2f\n", pal.GetToneSaturation());
        } else {
            BML::Shell::Fail(bml, "[palette] unknown option.\n");
            return;
        }
        if (buf[0]) bml->SendIngameMessage(buf);
    } else if (args[1] == "reset") {
        // Remove [theme] section and reload -> default behavior
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.ResetThemeOptions();
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) bml->SendIngameMessage("[palette] theme reset. Using defaults.\n");
        else if (!ok) BML::Shell::Fail(bml, "[palette] failed to update config.\n");
        else bml->SendIngameMessage("[palette] no config found, using default.\n");
    } else {
        BML::Shell::Fail(bml, "Unknown palette action: " + args[1]);
    }
}

const std::vector<std::string> PaletteCommand::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2) return {"reload", "sample", "list", "theme", "show", "info", "set", "get", "reset"};
    if (args.size() == 3 && args[1] == std::string("theme")) {
        // Dynamic theme names + 'none' using palette API
        std::vector<std::string> out;
        out.emplace_back("none");
        AnsiPalette pal;
        const auto names = pal.GetAvailableThemes();
        const std::string &prefix = args[2];
        std::string prefixLower = utils::ToLower(prefix);
        for (auto &name : names) {
            std::string low = utils::ToLower(name);
            if (prefix.empty() || utils::StartsWith(low, prefixLower)) out.push_back(name);
        }
        return out;
    }
    if (args.size() == 3 && args[1] == std::string("set")) {
        return {"cube", "gray", "mix_strength", "mix_space", "toning", "tone_brightness", "tone_saturation"};
    }
    if (args.size() == 3 && args[1] == std::string("get")) {
        return {"theme", "cube", "gray", "mix_strength", "mix_space", "toning", "tone_brightness", "tone_saturation"};
    }
    if (args.size() == 4 && args[1] == std::string("set")) {
        std::string opt = utils::ToLower(args[2]);
        if (opt == "cube" || opt == "gray") return {"standard", "theme", "on", "off"};
        if (opt == "mix_space") return {"linear", "srgb", "on", "off"};
        if (opt == "toning") return {"on", "off"};
        // For numeric options, no static suggestions
    }
    return {};
}
