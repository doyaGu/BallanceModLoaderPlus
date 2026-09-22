// Shared vocabulary of the console shell: quoting contexts, exit statuses, and
// the hard limits that keep alias recursion, substitution nesting, and capture
// buffers bounded. Everything in BML::Shell runs on the game thread.
#ifndef BML_SHELL_TYPES_H
#define BML_SHELL_TYPES_H

#include <cstddef>

#include "BML/Command.h"

namespace BML::Shell {
    // Which quoting the caret or a word part sits in. Bare is unquoted text.
    enum class QuoteContext {
        Bare,
        Single,
        Double,
        AnsiC,
    };

    namespace Status {
        constexpr int Ok = BML_COMMAND_STATUS_SUCCESS;
        constexpr int Failure = BML_COMMAND_STATUS_FAILURE;
        constexpr int Syntax = BML_COMMAND_STATUS_SYNTAX;
        constexpr int Disabled = BML_COMMAND_STATUS_DISABLED;
        constexpr int CheatRefused = BML_COMMAND_STATUS_CHEAT_REFUSED;
        constexpr int Unknown = BML_COMMAND_STATUS_UNKNOWN;
    }

    namespace Limits {
        constexpr std::size_t MaxLineBytes = BML_COMMAND_MAX_LINE_BYTES;
        constexpr std::size_t MaxPipelineStages = 16;
        constexpr std::size_t MaxCommandsPerLine = 64;
        constexpr std::size_t MaxAliasDepth = 16;
        constexpr std::size_t MaxSubstitutionDepth = 8;
        constexpr std::size_t MaxCommandDispatchDepth = 16;
        constexpr std::size_t MaxWordBytes = 65536;
        constexpr std::size_t MaxCaptureBytes = 1u << 20;
    }
}

#endif // BML_SHELL_TYPES_H
