#include "Console/Shell/ShellExecutor.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace BML::Shell {
    namespace {
        class SinkGuard {
        public:
            SinkGuard(IDispatcher &dispatcher, OutputSink *sink) : m_Dispatcher(dispatcher) {
                m_Dispatcher.PushSink(sink);
            }

            ~SinkGuard() { m_Dispatcher.PopSink(); }

            SinkGuard(const SinkGuard &) = delete;
            SinkGuard &operator=(const SinkGuard &) = delete;

        private:
            IDispatcher &m_Dispatcher;
        };

        // A word made only of variables and substitutions vanishes when it expands
        // to nothing, the way $unset does in other shells; anything quoted stays.
        bool VanishesWhenEmpty(const Word &word) {
            for (const WordPart &part : word.parts) {
                if (part.kind != WordPart::Kind::Variable && part.kind != WordPart::Kind::Substitution)
                    return false;
            }
            return true;
        }

        std::size_t CountCodepoints(std::string_view text) {
            std::size_t count = 0;
            for (unsigned char c : text) {
                if ((c & 0xC0) != 0x80)
                    ++count;
            }
            return count;
        }
    }

    void CaptureSink::Write(std::string_view message) {
        if (m_Truncated)
            return;
        if (m_Text.size() + message.size() + 1 > Limits::MaxCaptureBytes) {
            m_Truncated = true;
            return;
        }
        m_Text.append(message.data(), message.size());
        if (message.empty() || message.back() != '\n')
            m_Text.push_back('\n');
    }

    std::string CaptureSink::TakeText() {
        std::string text = std::move(m_Text);
        m_Text.clear();
        return text;
    }

    std::string FormatError(std::string_view message) {
        std::string out = "\x1b[31m";
        out.append(message.data(), message.size());
        out += "\x1b[0m";
        return out;
    }

    Executor::Executor(IDispatcher &dispatcher, const VariableResolver *variables, const AliasResolver *aliases)
        : m_Dispatcher(dispatcher), m_Variables(variables), m_Aliases(aliases) {}

    int Executor::Execute(std::string_view line) {
        if (line.size() > Limits::MaxLineBytes) {
            m_Dispatcher.WriteError(FormatError("command line is too long"));
            m_LastStatus = Status::Syntax;
            return m_LastStatus;
        }

        const ParseResult parsed = Parse(line, m_Aliases);
        if (!parsed.Ok()) {
            ReportSyntaxError(line, parsed);
            m_LastStatus = Status::Syntax;
            return m_LastStatus;
        }
        if (parsed.list.Empty())
            return Status::Ok;

        m_Dispatcher.LogExecute(line);
        m_LastStatus = RunList(parsed.list, 0);
        return m_LastStatus;
    }

    int Executor::RunList(const List &list, std::size_t depth) {
        int status = Status::Ok;
        for (const AndOr &item : list.items)
            status = RunAndOr(item, depth);
        return status;
    }

    int Executor::RunAndOr(const AndOr &andOr, std::size_t depth) {
        int status = RunPipeline(andOr.pipelines[0], depth);
        m_LastStatus = status;
        for (std::size_t i = 0; i < andOr.ops.size(); ++i) {
            const ListOp op = andOr.ops[i];
            if ((op == ListOp::And && status != Status::Ok) || (op == ListOp::Or && status == Status::Ok))
                continue;
            status = RunPipeline(andOr.pipelines[i + 1], depth);
            m_LastStatus = status;
        }
        return status;
    }

    int Executor::RunPipeline(const Pipeline &pipeline, std::size_t depth) {
        const std::size_t count = pipeline.commands.size();
        std::string input;
        bool haveInput = false;
        int status = Status::Ok;
        for (std::size_t i = 0; i < count; ++i) {
            const SimpleCommand &command = pipeline.commands[i];
            const std::string *stageInput = haveInput ? &input : nullptr;
            if (i + 1 < count) {
                CaptureSink sink;
                {
                    SinkGuard guard(m_Dispatcher, &sink);
                    status = RunCommand(command, stageInput, depth);
                }
                if (sink.Truncated()) {
                    m_Dispatcher.WriteError(FormatError("pipeline output exceeded the capture limit"));
                    return Status::Failure;
                }
                input = sink.TakeText();
                haveInput = true;
            } else {
                status = RunCommand(command, stageInput, depth);
            }
        }
        return status;
    }

    int Executor::RunCommand(const SimpleCommand &command, const std::string *input, std::size_t depth) {
        ExpandContext context;
        context.variables = m_Variables;
        context.lastStatus = m_LastStatus;
        context.runSubstitution = [this, depth](std::string_view source, std::string &out, std::string &error) {
            return RunSubstitution(source, depth, out, error);
        };

        std::vector<std::string> args;
        std::string error;
        if (!ExpandWords(command.words, context, args, error)) {
            m_Dispatcher.WriteError(FormatError(error));
            return Status::Failure;
        }

        std::vector<std::string> kept;
        kept.reserve(args.size());
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i].empty() && VanishesWhenEmpty(command.words[i]))
                continue;
            kept.push_back(std::move(args[i]));
        }
        if (kept.empty())
            return Status::Ok;

        return m_Dispatcher.Invoke(kept, input);
    }

    bool Executor::RunSubstitution(std::string_view source, std::size_t depth, std::string &out, std::string &error) {
        if (depth >= Limits::MaxSubstitutionDepth) {
            error = "command substitution nested too deeply";
            return false;
        }
        const ParseResult parsed = Parse(source, m_Aliases);
        if (!parsed.Ok()) {
            error = "in command substitution: " + parsed.message;
            return false;
        }

        const int savedStatus = m_LastStatus;
        CaptureSink sink;
        {
            SinkGuard guard(m_Dispatcher, &sink);
            if (!parsed.list.Empty())
                RunList(parsed.list, depth + 1);
        }
        m_LastStatus = savedStatus;

        if (sink.Truncated()) {
            error = "command substitution output exceeded the capture limit";
            return false;
        }

        out = sink.TakeText();
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        return true;
    }

    void Executor::ReportSyntaxError(std::string_view source, const ParseResult &parsed) {
        std::string message = "syntax error: ";
        if (parsed.incomplete) {
            message += "unexpected end of input (";
            message += parsed.message;
            message += ")";
        } else {
            message += parsed.message;
        }
        std::string text = FormatError(message);

        if (!source.empty()) {
            const std::size_t pos = std::min(parsed.errorPos, source.size());
            std::size_t lineStart = 0;
            if (pos > 0) {
                const std::size_t newline = source.rfind('\n', pos - 1);
                if (newline != std::string_view::npos)
                    lineStart = newline + 1;
            }
            std::size_t lineEnd = source.find('\n', pos);
            if (lineEnd == std::string_view::npos)
                lineEnd = source.size();
            const std::string_view sourceLine = source.substr(lineStart, lineEnd - lineStart);
            const std::size_t column = CountCodepoints(source.substr(lineStart, pos - lineStart));

            text += "\n  ";
            text.append(sourceLine.data(), sourceLine.size());
            text += "\n  ";
            text.append(column, ' ');
            text += "^";
        }
        m_Dispatcher.WriteError(text);
    }
}
