#include "Console/Shell/FilterCommands.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <oniguruma.h>

#include "BML/IBML.h"
#include "Console/Shell/ShellIo.h"
#include "Console/Shell/ShellTypes.h"
#include "StringUtils.h"

namespace BML::Shell::Filters {
    std::vector<std::string> SplitLines(std::string_view text) {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            lines.emplace_back(line);
            start = end + 1;
        }
        return lines;
    }

    namespace {
        class Regex {
        public:
            Regex(std::string_view pattern, bool ignoreCase, bool fixed, std::string &error) {
                OnigOptionType options = ONIG_OPTION_NONE;
                if (ignoreCase)
                    options |= ONIG_OPTION_IGNORECASE;
                OnigErrorInfo info;
                const auto *begin = reinterpret_cast<const OnigUChar *>(pattern.data());
                const int result = onig_new(&m_Regex, begin, begin + pattern.size(), options, ONIG_ENCODING_UTF8,
                                            fixed ? ONIG_SYNTAX_ASIS : ONIG_SYNTAX_PERL_NG, &info);
                if (result != ONIG_NORMAL) {
                    OnigUChar buffer[ONIG_MAX_ERROR_MESSAGE_LEN];
                    onig_error_code_to_str(buffer, result, &info);
                    error = reinterpret_cast<const char *>(buffer);
                    m_Regex = nullptr;
                }
            }

            ~Regex() {
                if (m_Regex)
                    onig_free(m_Regex);
            }

            Regex(const Regex &) = delete;
            Regex &operator=(const Regex &) = delete;

            bool Valid() const { return m_Regex != nullptr; }

            bool Matches(std::string_view text) const {
                const auto *begin = reinterpret_cast<const OnigUChar *>(text.data());
                const auto *end = begin + text.size();
                return onig_search(m_Regex, begin, end, begin, end, nullptr, ONIG_OPTION_NONE) >= 0;
            }

        private:
            OnigRegex m_Regex = nullptr;
        };

        double LeadingNumber(const std::string &text) {
            const char *cursor = text.c_str();
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            char *end = nullptr;
            const double value = std::strtod(cursor, &end);
            return end == cursor ? 0.0 : value;
        }
    }

    bool Grep(const std::vector<std::string> &lines, std::string_view pattern, const GrepOptions &options,
              std::vector<std::string> &out, std::string &error) {
        out.clear();
        const Regex regex(pattern, options.ignoreCase, options.fixed, error);
        if (!regex.Valid())
            return false;

        std::size_t matched = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string plain = utils::StripAnsiCodes(lines[i].c_str());
            const bool hit = regex.Matches(plain) != options.invert;
            if (!hit)
                continue;
            ++matched;
            if (options.countOnly)
                continue;
            if (options.lineNumbers)
                out.push_back(std::to_string(i + 1) + ":" + lines[i]);
            else
                out.push_back(lines[i]);
        }
        if (options.countOnly)
            out.push_back(std::to_string(matched));
        return matched > 0;
    }

    std::vector<std::string> Head(const std::vector<std::string> &lines, std::size_t count) {
        const std::size_t taken = std::min(count, lines.size());
        return std::vector<std::string>(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(taken));
    }

    std::vector<std::string> Tail(const std::vector<std::string> &lines, std::size_t count) {
        const std::size_t taken = std::min(count, lines.size());
        return std::vector<std::string>(lines.end() - static_cast<std::ptrdiff_t>(taken), lines.end());
    }

    std::string Wc(std::string_view text, const WcOptions &options) {
        std::size_t lineCount = 0;
        std::size_t wordCount = 0;
        bool inWord = false;
        for (char c : text) {
            if (c == '\n')
                ++lineCount;
            const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
            if (space) {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                ++wordCount;
            }
        }

        const bool all = !options.lines && !options.words && !options.bytes;
        std::string out;
        auto append = [&](std::size_t value) {
            if (!out.empty())
                out.push_back(' ');
            out += std::to_string(value);
        };
        if (all || options.lines)
            append(lineCount);
        if (all || options.words)
            append(wordCount);
        if (all || options.bytes)
            append(text.size());
        return out;
    }

    std::vector<std::string> Sort(std::vector<std::string> lines, const SortOptions &options) {
        auto less = [&](const std::string &a, const std::string &b) {
            if (options.numeric) {
                const double x = LeadingNumber(a);
                const double y = LeadingNumber(b);
                if (x != y)
                    return x < y;
            }
            return utils::CompareString(a, b) < 0;
        };
        std::stable_sort(lines.begin(), lines.end(), less);
        if (options.unique) {
            lines.erase(std::unique(lines.begin(), lines.end(),
                                    [&](const std::string &a, const std::string &b) {
                                        return !less(a, b) && !less(b, a);
                                    }),
                        lines.end());
        }
        if (options.reverse)
            std::reverse(lines.begin(), lines.end());
        return lines;
    }

    std::vector<std::string> Uniq(const std::vector<std::string> &lines, bool count) {
        std::vector<std::string> out;
        std::size_t run = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            ++run;
            const bool last = i + 1 == lines.size() || lines[i + 1] != lines[i];
            if (!last)
                continue;
            if (count)
                out.push_back(std::to_string(run) + " " + lines[i]);
            else
                out.push_back(lines[i]);
            run = 0;
        }
        return out;
    }
}

namespace BML::Shell {
    namespace {
        std::vector<std::string> InputLines() {
            const std::string *input = GetInput();
            return Filters::SplitLines(input ? *input : std::string());
        }

        void PrintLines(IBML *bml, const std::vector<std::string> &lines) {
            if (!bml)
                return;
            for (const std::string &line : lines)
                bml->SendIngameMessage(line.c_str());
        }

        // Reads "-n N" or "-N" style counts. Returns false after printing an error.
        bool ReadCount(IBML *bml, const char *command, const std::vector<std::string> &args, std::size_t &count) {
            count = 10;
            for (std::size_t i = 1; i < args.size(); ++i) {
                const std::string &arg = args[i];
                std::string number;
                if (arg == "-n") {
                    if (i + 1 >= args.size()) {
                        Fail(bml, std::string(command) + ": -n needs a number");
                        return false;
                    }
                    number = args[++i];
                } else if (arg.size() > 1 && arg[0] == '-') {
                    number = arg.substr(1);
                } else {
                    Fail(bml, std::string(command) + ": unexpected argument '" + arg + "'");
                    return false;
                }
                char *end = nullptr;
                const long value = std::strtol(number.c_str(), &end, 10);
                if (!end || *end != '\0' || value < 0) {
                    Fail(bml, std::string(command) + ": '" + number + "' is not a count");
                    return false;
                }
                count = static_cast<std::size_t>(value);
            }
            return true;
        }
    }

    void CommandGrep::Execute(IBML *bml, const std::vector<std::string> &args) {
        Filters::GrepOptions options;
        std::string pattern;
        bool havePattern = false;
        for (std::size_t i = 1; i < args.size(); ++i) {
            const std::string &arg = args[i];
            if (!havePattern && arg.size() > 1 && arg[0] == '-' && arg != "--") {
                if (arg == "-e") {
                    if (i + 1 >= args.size()) {
                        Fail(bml, "grep: -e needs a pattern");
                        return;
                    }
                    pattern = args[++i];
                    havePattern = true;
                    continue;
                }
                for (std::size_t k = 1; k < arg.size(); ++k) {
                    switch (arg[k]) {
                        case 'i': options.ignoreCase = true; break;
                        case 'v': options.invert = true; break;
                        case 'n': options.lineNumbers = true; break;
                        case 'c': options.countOnly = true; break;
                        case 'F': options.fixed = true; break;
                        default:
                            Fail(bml, "grep: unknown option '-" + std::string(1, arg[k]) + "'");
                            return;
                    }
                }
                continue;
            }
            if (arg == "--" && !havePattern) {
                if (i + 1 < args.size()) {
                    pattern = args[++i];
                    havePattern = true;
                }
                continue;
            }
            if (!havePattern) {
                pattern = arg;
                havePattern = true;
                continue;
            }
            Fail(bml, "grep: unexpected argument '" + arg + "'; grep reads piped text only");
            return;
        }
        if (!havePattern) {
            Fail(bml, "usage: grep [-i] [-v] [-n] [-c] [-F] PATTERN");
            return;
        }

        std::vector<std::string> out;
        std::string error;
        const bool matched = Filters::Grep(InputLines(), pattern, options, out, error);
        if (!error.empty()) {
            Fail(bml, "grep: " + error);
            SetStatus(Status::Syntax);
            return;
        }
        PrintLines(bml, out);
        if (!matched)
            SetStatus(Status::Failure);
    }

    const std::vector<std::string> CommandGrep::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        if (args.size() >= 2 && !args.back().empty() && args.back()[0] == '-')
            return {"-i", "-v", "-n", "-c", "-F", "-e"};
        return {};
    }

    void CommandHead::Execute(IBML *bml, const std::vector<std::string> &args) {
        std::size_t count = 0;
        if (!ReadCount(bml, "head", args, count))
            return;
        PrintLines(bml, Filters::Head(InputLines(), count));
    }

    const std::vector<std::string> CommandHead::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"-n"} : std::vector<std::string>{};
    }

    void CommandTail::Execute(IBML *bml, const std::vector<std::string> &args) {
        std::size_t count = 0;
        if (!ReadCount(bml, "tail", args, count))
            return;
        PrintLines(bml, Filters::Tail(InputLines(), count));
    }

    const std::vector<std::string> CommandTail::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"-n"} : std::vector<std::string>{};
    }

    void CommandWc::Execute(IBML *bml, const std::vector<std::string> &args) {
        Filters::WcOptions options;
        for (std::size_t i = 1; i < args.size(); ++i) {
            const std::string &arg = args[i];
            if (arg.size() < 2 || arg[0] != '-') {
                Fail(bml, "wc: unexpected argument '" + arg + "'; wc reads piped text only");
                return;
            }
            for (std::size_t k = 1; k < arg.size(); ++k) {
                switch (arg[k]) {
                    case 'l': options.lines = true; break;
                    case 'w': options.words = true; break;
                    case 'c': options.bytes = true; break;
                    default:
                        Fail(bml, "wc: unknown option '-" + std::string(1, arg[k]) + "'");
                        return;
                }
            }
        }
        const std::string *input = GetInput();
        if (bml)
            bml->SendIngameMessage(Filters::Wc(input ? *input : std::string(), options).c_str());
    }

    const std::vector<std::string> CommandWc::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"-l", "-w", "-c"} : std::vector<std::string>{};
    }

    void CommandSort::Execute(IBML *bml, const std::vector<std::string> &args) {
        Filters::SortOptions options;
        for (std::size_t i = 1; i < args.size(); ++i) {
            const std::string &arg = args[i];
            if (arg.size() < 2 || arg[0] != '-') {
                Fail(bml, "sort: unexpected argument '" + arg + "'; sort reads piped text only");
                return;
            }
            for (std::size_t k = 1; k < arg.size(); ++k) {
                switch (arg[k]) {
                    case 'r': options.reverse = true; break;
                    case 'u': options.unique = true; break;
                    case 'n': options.numeric = true; break;
                    default:
                        Fail(bml, "sort: unknown option '-" + std::string(1, arg[k]) + "'");
                        return;
                }
            }
        }
        PrintLines(bml, Filters::Sort(InputLines(), options));
    }

    const std::vector<std::string> CommandSort::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"-r", "-u", "-n"} : std::vector<std::string>{};
    }

    void CommandUniq::Execute(IBML *bml, const std::vector<std::string> &args) {
        bool count = false;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "-c") {
                count = true;
                continue;
            }
            Fail(bml, "uniq: unexpected argument '" + args[i] + "'; uniq reads piped text only");
            return;
        }
        PrintLines(bml, Filters::Uniq(InputLines(), count));
    }

    const std::vector<std::string> CommandUniq::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"-c"} : std::vector<std::string>{};
    }
}
