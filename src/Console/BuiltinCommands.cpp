#include "Console/BuiltinCommands.h"

#include <algorithm>
#include <cstdlib>

#include "BML/IBML.h"
#include "Console/Console.h"
#include "Console/Shell/ShellIo.h"
#include "Loader/ModContext.h"
#include "StringUtils.h"

void CommandHelp::Execute(IBML *bml, const std::vector<std::string> &args) {
    const auto commands = BML_GetModContext()->GetCommandSnapshot();
    const std::size_t visibleCount = static_cast<std::size_t>(std::count_if(
        commands.begin(), commands.end(),
        [](const BML::CommandContext::CommandInfo &command) {
            return !command.Hidden;
        }));
    bml->SendIngameMessage((std::to_string(visibleCount) + " Existing Commands:").data());
    for (const auto &command : commands) {
        if (command.Hidden)
            continue;
        std::string str = std::string("\t") + command.Name;
        if (!command.Alias.empty())
            str += "(" + command.Alias + ")";
        if (command.Cheat)
            str += "[Cheat]";
        if (!command.Enabled)
            str += "[Disabled]";
        str += ": " + command.Description;
        bml->SendIngameMessage(str.data());
    }
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

    std::string out = utils::JoinString(args, ' ', idx);

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
    m_Console->ClearMessages();
}

// history            print every entry, oldest first, numbered for !n
// history N          print the last N entries
// history clear | -c forget everything
// history -d N       forget entry N
void CommandHistory::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        m_Console->PrintHistory();
        return;
    }
    if (args.size() == 2 && (args[1] == "clear" || args[1] == "-c")) {
        m_Console->ClearHistory();
        return;
    }
    if (args.size() == 3 && args[1] == "-d") {
        char *end = nullptr;
        const long number = std::strtol(args[2].c_str(), &end, 10);
        if (!end || *end != '\0' || number <= 0 || !m_Console->EraseHistory(static_cast<std::size_t>(number))) {
            BML::Shell::Fail(bml, "history: no entry " + args[2]);
        }
        return;
    }
    if (args.size() == 2) {
        char *end = nullptr;
        const long count = std::strtol(args[1].c_str(), &end, 10);
        if (end && *end == '\0' && count > 0) {
            m_Console->PrintHistory(static_cast<std::size_t>(count));
            return;
        }
    }
    BML::Shell::Fail(bml, "usage: history [N] | history clear | history -d N");
}

void CommandExit::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->ExitGame();
}
