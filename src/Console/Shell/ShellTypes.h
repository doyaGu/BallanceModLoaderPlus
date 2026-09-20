// Shared vocabulary of the console shell: quoting contexts, exit statuses, and
// the hard limits that keep alias recursion, substitution nesting, and capture
// buffers bounded. Everything in BML::Shell runs on the game thread.
#ifndef BML_SHELL_TYPES_H
#define BML_SHELL_TYPES_H

#include <cstddef>

namespace BML::Shell {
    // Which quoting the caret or a word part sits in. Bare is unquoted text.
    enum class QuoteContext {
        Bare,
        Single,
        Double,
        AnsiC,
    };

    namespace Status {
        constexpr int Ok = 0;
        constexpr int Failure = 1;
        constexpr int Syntax = 2;
        constexpr int CheatRefused = 126;
        constexpr int Unknown = 127;
    }

    namespace Limits {
        constexpr std::size_t MaxLineBytes = 65535;
        constexpr std::size_t MaxPipelineStages = 16;
        constexpr std::size_t MaxCommandsPerLine = 64;
        constexpr std::size_t MaxAliasDepth = 16;
        constexpr std::size_t MaxSubstitutionDepth = 8;
        constexpr std::size_t MaxWordBytes = 65536;
        constexpr std::size_t MaxCaptureBytes = 1u << 20;
    }
}

#endif // BML_SHELL_TYPES_H
