#include "Commands.h"

#include "BML/IBML.h"
#include "BML/BML.h"
#include "BMLMod.h"

#include "StringUtils.h"

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
                ++idx; // consume this option token
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
std::string CommandEcho::JoinArgs(const std::vector<std::string>& args, size_t start) {
    std::string s;
    for (size_t i = start; i < args.size(); ++i) {
        if (i > start) s.push_back(' ');
        s.append(args[i]);
    }
    return s;
}

// Handle \c (truncate output and suppress newline)
bool CommandEcho::ApplyBackslashCTrunc(std::string& s) {
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
    }
}
