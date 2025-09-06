#include "Commands.h"

#include "BML/IBML.h"
#include "BML/BML.h"
#include "BMLMod.h"

#include "ModContext.h"
#include "StringUtils.h"
#include "AnsiPalette.h"

void CommandBML::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->SendIngameMessage("Ballance Mod Loader Plus " BML_VERSION);
    bml->SendIngameMessage((std::to_string(bml->GetModCount()) + " Mods Installed:").data());

    int count = bml->GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = bml->GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

void CommandHelp::Execute(IBML *bml, const std::vector<std::string> &args) {
    const int cmdCount = bml->GetCommandCount();
    bml->SendIngameMessage((std::to_string(cmdCount) + " Existing Commands:").data());
    for (int i = 0; i < cmdCount; i++) {
        ICommand *cmd = bml->GetCommand(i);
        std::string str = std::string("\t") + cmd->GetName();
        if (!cmd->GetAlias().empty())
            str += "(" + cmd->GetAlias() + ")";
        if (cmd->IsCheat())
            str += "[Cheat]";
        str += ": " + cmd->GetDescription();
        bml->SendIngameMessage(str.data());
    }
}

void CommandCheat::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        bml->EnableCheat(!bml->IsCheatEnabled());
    } else {
        bml->EnableCheat(ParseBoolean(args[1]));
    }
    bml->SendIngameMessage(bml->IsCheatEnabled() ? "Cheat Mode On" : "Cheat Mode Off");
}

void CommandEcho::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml) return;

    // No args -> print newline
    if (args.size() <= 1) {
        bml->SendIngameMessage("\n");
        return;
    }

    EchoOpts opt;
    size_t idx = 1;

    // Option parsing:
    // - consume a standalone "--"
    // - consume only tokens of the form -[n,e,E]+
    // - stop (do not consume) on first non-option or unknown option
    while (idx < args.size()) {
        const std::string &tok = args[idx];

        if (tok == "--") {
            // explicit end of options, do not emit it
            ++idx;
            break;
        }

        if (tok.size() >= 2 && tok[0] == '-') {
            // Check if token is entirely composed of recognized flags
            bool recognized = true;
            for (size_t i = 1; i < tok.size(); ++i) {
                char c = tok[i];
                if (c != 'n' && c != 'e' && c != 'E') {
                    recognized = false;
                    break;
                }
            }

            if (recognized) {
                ParseEchoOptionToken(tok, opt); // update flags
                ++idx;                          // consume this option token
                continue;
            }
            // Unknown option like "-x": stop parsing and treat it as data
        }

        // Non-option token: stop parsing
        break;
    }

    std::string out = JoinArgs(args, idx);

    bool suppressNewlineViaC = false;
    if (opt.interpretEscapes) {
        // \c truncation is handled before unescaping, as in bash echo -e
        suppressNewlineViaC = ApplyBackslashCTrunc(out);
        out = utils::UnescapeString(out.c_str());
    }

    if (!(opt.noNewline || suppressNewlineViaC)) {
        out.push_back('\n');
    }

    bml->SendIngameMessage(out.c_str());
}

// Parse echo options within a single recognized token like "-neE"
void CommandEcho::ParseEchoOptionToken(const std::string &tok, EchoOpts &opt) {
    // precondition: tok.size() >= 2 && tok[0] == '-'
    for (size_t i = 1; i < tok.size(); ++i) {
        const char c = tok[i];
        switch (c) {
        case 'n': opt.noNewline = true;
            break;
        case 'e': opt.interpretEscapes = true;
            break;
        case 'E': opt.interpretEscapes = false;
            break;
        default: opt.parsingOptions = false;
            return; // unknown flag -> stop option mode
        }
    }
}

// Join args with spaces from given index
std::string CommandEcho::JoinArgs(const std::vector<std::string> &args, size_t start) {
    std::string s;
    for (size_t i = start; i < args.size(); ++i) {
        if (i > start) s.push_back(' ');
        s.append(args[i]);
    }
    return s;
}

// Handle \c (truncate output and suppress newline)
bool CommandEcho::ApplyBackslashCTrunc(std::string &s) {
    bool stop = false;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\\') {
            size_t j = i;
            while (j < s.size() && s[j] == '\\') ++j;
            bool escaped = ((j - i) % 2 == 1);
            if (escaped && j < s.size() && s[j] == 'c') {
                s.erase(i);
                stop = true;
                break;
            }
            i = j + (escaped ? 1 : 0);
        } else {
            ++i;
        }
    }
    return stop;
}

void CommandClear::Execute(IBML *bml, const std::vector<std::string> &args) {
    m_BMLMod->ClearIngameMessages();
}

void CommandHistory::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        m_BMLMod->PrintHistory();
    } else if (args.size() == 2) {
        if (args[1] == "clear") {
            m_BMLMod->ClearHistory();
        } else {
            int i = ParseInteger(args[1]);
            if (i != 0)
                m_BMLMod->ExecuteHistory(i + 1);
        }
    }
}

void CommandExit::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->ExitGame();
}

CommandHUD::CommandHUD(BMLMod *mod) : m_BMLMod(mod) {
    m_State = m_BMLMod->GetHUD();
}

void CommandHUD::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2) {
        if (ParseBoolean(args[1])) {
            m_BMLMod->SetHUD(m_State);
        } else {
            m_State = m_BMLMod->GetHUD();
            m_BMLMod->SetHUD(0);
        }
    } else if (args.size() == 3) {
        int state = m_BMLMod->GetHUD();
        if (args[1] == "title") {
            if (ParseBoolean(args[2])) {
                state |= HUD_TITLE;
            } else {
                state &= ~HUD_TITLE;
            }
        } else if (args[1] == "fps") {
            if (ParseBoolean(args[2])) {
                state |= HUD_FPS;
            } else {
                state &= ~HUD_FPS;
            }
        } else if (args[1] == "sr") {
            if (ParseBoolean(args[2])) {
                state |= HUD_SR;
            } else {
                state &= ~HUD_SR;
            }
        }
        m_BMLMod->SetHUD(state);
    }
}

void CommandPalette::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!m_BMLMod) return;
    auto &mb = m_BMLMod->GetMessageBoard();
    if (args.size() <= 1 || args[1] == "reload") {
        const bool loaded = mb.ReloadPaletteFromFile();
        const std::wstring path = mb.GetPaletteConfigPathW();
        if (loaded) {
            char *pathAnsi = BML_Utf16ToAnsi(path.c_str());
            bml->SendIngameMessage((std::string("[palette] reloaded from ") + pathAnsi + "\n").c_str());
            delete[] pathAnsi;
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "sample") {
        const bool written = mb.SavePaletteSampleIfMissing();
        const std::wstring path = mb.GetPaletteConfigPathW();
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
            bml->SendIngameMessage("Usage: palette theme <name>\n");
            return;
        }
        const std::string &name = args[2];
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetActiveThemeName(name);
        // Reload
        const bool loaded = mb.ReloadPaletteFromFile();
        if (ok && loaded) {
            std::string nameLower = utils::ToLower(name);
            std::string msg = (nameLower == "none")
                                  ? "[palette] theme cleared, using defaults + local overrides.\n"
                                  : (std::string("[palette] theme set to ") + name + ", reloaded.\n");
            bml->SendIngameMessage(msg.c_str());
        } else if (!ok) {
            bml->SendIngameMessage("[palette] failed to update config.\n");
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
                 (theme.empty() ? "none" : theme.c_str()), cube.c_str(), gray.c_str(), mix, space.c_str(),
                 toning ? "on" : "off", tb, ts);
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
            bml->SendIngameMessage("Usage: palette set <cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation> <value>\n");
            return;
        }
        std::string opt = utils::ToLower(args[2]);
        // Join remaining tokens to accept values like "70 %" with space or quoted
        std::string val;
        for (size_t i = 3; i < args.size(); ++i) {
            if (i > 3) val.push_back(' ');
            val += args[i];
        }
        // Strip surrounding quotes and trim
        val = utils::TrimStringCopy(val);
        if (!val.empty() && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\'')) && val.size() >= 2) {
            val = val.substr(1, val.size() - 2);
            val = utils::TrimStringCopy(val);
        }
        // Normalize some common shorthands
        if (opt == "linear") opt = "mix_space";
        if (opt == "mix") opt = "mix_strength";
        if (opt == "grey") opt = "gray";
        if (opt == "tone_enable" || opt == "enable_toning") opt = "toning";
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetThemeOption(opt, val);
        const bool loaded = mb.ReloadPaletteFromFile();
        if (ok && loaded) {
            bml->SendIngameMessage("[palette] option updated.\n");
        } else if (!ok) {
            // Provide range tips for numeric keys
            if (opt == "mix_strength") {
                bml->SendIngameMessage("[palette] invalid mix_strength. Expect 0..1 or percent (e.g., 70%).\n");
            } else if (opt == "tone_brightness" || opt == "tone_saturation") {
                bml->SendIngameMessage("[palette] invalid value. Expect in [-1..1].\n");
            } else {
                bml->SendIngameMessage("[palette] failed to update config.\n");
            }
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "get") {
        // palette get <option>
        if (args.size() < 3) {
            bml->SendIngameMessage("Usage: palette get <theme|cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation>\n");
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
            bml->SendIngameMessage("[palette] unknown option.\n");
            return;
        }
        if (buf[0]) bml->SendIngameMessage(buf);
    } else if (args[1] == "reset") {
        // Remove [theme] section and reload -> default behavior
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.ResetThemeOptions();
        const bool loaded = mb.ReloadPaletteFromFile();
        if (ok && loaded) bml->SendIngameMessage("[palette] theme reset. Using defaults.\n");
        else if (!ok) bml->SendIngameMessage("[palette] failed to update config.\n");
        else bml->SendIngameMessage("[palette] no config found, using default.\n");
    }
}

const std::vector<std::string> CommandPalette::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2) return {"reload", "sample", "list", "theme", "show", "info", "set", "get", "reset"};
    if (args.size() == 3 && args[1] == std::string("theme")) {
        // Dynamic theme names + 'none' using palette API
        std::vector<std::string> out;
        out.emplace_back("none");
        AnsiPalette pal;
        auto names = pal.GetAvailableThemes();
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
