#include "Console/Shell/ShellBuiltins.h"

#include <algorithm>
#include <cstdlib>

#include "BML/IBML.h"
#include "Console/CommandContext.h"
#include "Console/Shell/ShellEnvironment.h"
#include "Console/Shell/ShellIo.h"
#include "Console/Shell/ShellQuoting.h"
#include "Console/Shell/ShellTypes.h"
#include "StringUtils.h"

namespace BML::Shell {
    namespace {
        void Print(IBML *bml, const std::string &text) {
            if (bml)
                bml->SendIngameMessage(text.c_str());
        }

        void Warn(IBML *bml, const std::string &text) {
            Print(bml, "\x1b[33m" + text + "\x1b[0m");
        }

        std::vector<std::string> FilterByPrefix(std::vector<std::string> names, const std::string &prefix) {
            names.erase(std::remove_if(names.begin(), names.end(),
                                       [&](const std::string &name) {
                                           return name.compare(0, prefix.size(), prefix) != 0;
                                       }),
                        names.end());
            return names;
        }
    }

    // set                       list every variable
    // set NAME VALUE            session variable
    // set -U NAME VALUE         universal variable, kept across sessions
    // set -e NAME...            erase
    // set -q NAME...            succeed only when every NAME is set, print nothing
    void CommandSet::Execute(IBML *bml, const std::vector<std::string> &args) {
        if (args.size() == 1) {
            for (const auto &[name, value] : m_Environment.Variables(Environment::Scope::Universal))
                Print(bml, "set -U " + name + " " + SingleQuoted(value));
            for (const auto &[name, value] : m_Environment.Variables(Environment::Scope::Session))
                Print(bml, "set " + name + " " + SingleQuoted(value));
            return;
        }

        std::size_t index = 1;
        bool universal = false;
        bool erase = false;
        bool query = false;
        while (index < args.size() && args[index].size() >= 2 && args[index][0] == '-') {
            const std::string &option = args[index];
            if (option == "--") {
                ++index;
                break;
            }
            for (std::size_t i = 1; i < option.size(); ++i) {
                switch (option[i]) {
                    case 'U': universal = true; break;
                    case 'e': erase = true; break;
                    case 'q': query = true; break;
                    default:
                        Fail(bml, "set: unknown option '" + option + "'");
                        return;
                }
            }
            ++index;
        }

        if ((erase && query) || (erase && universal) || (query && universal)) {
            Fail(bml, "set: -U, -e, and -q cannot be combined");
            return;
        }

        if (query) {
            if (index >= args.size()) {
                Fail(bml, "usage: set -q NAME...");
                return;
            }
            for (; index < args.size(); ++index) {
                if (!m_Environment.HasVariable(args[index])) {
                    SetStatus(Status::Failure);
                    return;
                }
            }
            return;
        }

        if (erase) {
            if (index >= args.size()) {
                Fail(bml, "usage: set -e NAME...");
                return;
            }
            bool all = true;
            for (; index < args.size(); ++index) {
                if (!m_Environment.EraseVariable(args[index])) {
                    Warn(bml, "set: '" + args[index] + "' is not set");
                    all = false;
                }
            }
            if (!all)
                SetStatus(Status::Failure);
            return;
        }

        if (index >= args.size()) {
            Fail(bml, "usage: set [-U] NAME VALUE | set -e NAME... | set -q NAME...");
            return;
        }
        const std::string &name = args[index];
        if (!Environment::IsValidName(name)) {
            Fail(bml, "set: '" + name + "' is not a valid variable name");
            return;
        }
        if (args.size() - index > 2) {
            Fail(bml, "set: expected one value; quote a value that contains spaces");
            return;
        }
        const std::string value = index + 1 < args.size() ? args[index + 1] : std::string();
        m_Environment.SetVariable(name, value,
                                  universal ? Environment::Scope::Universal : Environment::Scope::Session);
    }

    const std::vector<std::string> CommandSet::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        if (args.size() < 2)
            return {};
        const std::string &current = args.back();
        if (args.size() == 2 && !current.empty() && current[0] == '-')
            return {"-U", "-e", "-q"};
        // Name position: right after the command or after its options.
        std::size_t index = 1;
        while (index < args.size() - 1 && !args[index].empty() && args[index][0] == '-')
            ++index;
        if (index == args.size() - 1)
            return FilterByPrefix(m_Environment.VariableNames(), current);
        return {};
    }

    // alias                     list
    // alias NAME                show one
    // alias NAME=BODY           define
    // alias NAME BODY...        define, body joined by spaces
    void CommandAlias::Execute(IBML *bml, const std::vector<std::string> &args) {
        if (args.size() == 1) {
            for (const auto &[name, body] : m_Environment.Aliases())
                Print(bml, "alias " + name + "=" + SingleQuoted(body));
            return;
        }

        std::string name = args[1];
        std::string body;
        bool define = false;
        if (const std::size_t equals = name.find('='); equals != std::string::npos) {
            body = name.substr(equals + 1);
            name.erase(equals);
            define = true;
            if (args.size() > 2)
                body += " " + utils::JoinString(args, ' ', 2);
        } else if (args.size() > 2) {
            body = utils::JoinString(args, ' ', 2);
            define = true;
        }

        if (!define) {
            std::string existing;
            if (!m_Environment.LookupAlias(name, existing)) {
                Fail(bml, "alias: '" + name + "' is not defined");
                return;
            }
            Print(bml, "alias " + name + "=" + SingleQuoted(existing));
            return;
        }

        if (!CommandContext::IsValidCommandName(name.c_str())) {
            Fail(bml, "alias: '" + name + "' is not a valid alias name");
            return;
        }
        utils::TrimString(body);
        if (body.empty()) {
            Fail(bml, "alias: the body of '" + name + "' is empty");
            return;
        }
        if (bml && bml->FindCommand(name.c_str()))
            Warn(bml, "alias: '" + name + "' now shadows a command of the same name; quote it to reach the command");
        m_Environment.SetAlias(name, body);
    }

    const std::vector<std::string> CommandAlias::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        if (args.size() == 2)
            return FilterByPrefix(m_Environment.AliasNames(), args.back());
        return {};
    }

    void CommandUnalias::Execute(IBML *bml, const std::vector<std::string> &args) {
        if (args.size() < 2) {
            Fail(bml, "usage: unalias NAME... | unalias -a");
            return;
        }
        if (args[1] == "-a") {
            m_Environment.ClearAliases();
            return;
        }
        bool all = true;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (!m_Environment.RemoveAlias(args[i])) {
                Warn(bml, "unalias: '" + args[i] + "' is not defined");
                all = false;
            }
        }
        if (!all)
            SetStatus(Status::Failure);
    }

    const std::vector<std::string> CommandUnalias::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        if (args.size() < 2)
            return {};
        std::vector<std::string> names = FilterByPrefix(m_Environment.AliasNames(), args.back());
        if (args.size() == 2)
            names.push_back("-a");
        return names;
    }

    void CommandFalse::Execute(IBML *, const std::vector<std::string> &) {
        SetStatus(Status::Failure);
    }

    std::vector<std::string> CommandXargs::SplitItems(const std::string &input, const std::string *delimiter) {
        std::vector<std::string> items;
        if (delimiter && !delimiter->empty()) {
            std::size_t start = 0;
            while (start <= input.size()) {
                const std::size_t next = input.find(*delimiter, start);
                const std::size_t end = next == std::string::npos ? input.size() : next;
                items.push_back(input.substr(start, end - start));
                if (next == std::string::npos)
                    break;
                start = next + delimiter->size();
            }
            // A trailing newline or delimiter does not add an empty item.
            while (!items.empty() && items.back().empty())
                items.pop_back();
            for (std::string &item : items) {
                while (!item.empty() && (item.back() == '\n' || item.back() == '\r'))
                    item.pop_back();
            }
            return items;
        }

        std::string current;
        for (char c : input) {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (!current.empty()) {
                    items.push_back(current);
                    current.clear();
                }
                continue;
            }
            current.push_back(c);
        }
        if (!current.empty())
            items.push_back(current);
        return items;
    }

    // xargs [-n N] [-d DELIM] COMMAND [ARG...]
    void CommandXargs::Execute(IBML *bml, const std::vector<std::string> &args) {
        std::size_t index = 1;
        std::size_t perCall = 0;
        std::string delimiter;
        bool haveDelimiter = false;
        while (index < args.size() && args[index].size() >= 2 && args[index][0] == '-') {
            const std::string &option = args[index];
            if (option == "--") {
                ++index;
                break;
            }
            if (option == "-n" || option == "-d") {
                if (index + 1 >= args.size()) {
                    Fail(bml, "xargs: option '" + option + "' needs a value");
                    return;
                }
                if (option == "-n") {
                    char *end = nullptr;
                    const long value = std::strtol(args[index + 1].c_str(), &end, 10);
                    if (!end || *end != '\0' || value <= 0) {
                        Fail(bml, "xargs: -n needs a positive number");
                        return;
                    }
                    perCall = static_cast<std::size_t>(value);
                } else {
                    delimiter = args[index + 1];
                    haveDelimiter = true;
                }
                index += 2;
                continue;
            }
            Fail(bml, "xargs: unknown option '" + option + "'");
            return;
        }
        if (index >= args.size()) {
            Fail(bml, "usage: xargs [-n N] [-d DELIM] COMMAND [ARG...]");
            return;
        }
        if (!m_Invoke) {
            Fail(bml, "xargs: no command runner is available");
            return;
        }

        const std::vector<std::string> base(args.begin() + static_cast<std::ptrdiff_t>(index), args.end());
        const std::string *input = GetInput();
        const std::vector<std::string> items = SplitItems(input ? *input : std::string(), haveDelimiter ? &delimiter : nullptr);

        bool anyFailed = false;
        auto run = [&](std::vector<std::string> call) {
            if (m_Invoke(call, nullptr) != Status::Ok)
                anyFailed = true;
        };

        if (items.empty()) {
            run(base);
        } else if (perCall == 0) {
            std::vector<std::string> call = base;
            call.insert(call.end(), items.begin(), items.end());
            run(std::move(call));
        } else {
            for (std::size_t start = 0; start < items.size(); start += perCall) {
                std::vector<std::string> call = base;
                const std::size_t end = std::min(items.size(), start + perCall);
                call.insert(call.end(), items.begin() + static_cast<std::ptrdiff_t>(start),
                            items.begin() + static_cast<std::ptrdiff_t>(end));
                run(std::move(call));
            }
        }
        if (anyFailed)
            SetStatus(123);
    }

    const std::vector<std::string> CommandXargs::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        if (args.size() == 2)
            return {"-n", "-d"};
        return {};
    }
}
