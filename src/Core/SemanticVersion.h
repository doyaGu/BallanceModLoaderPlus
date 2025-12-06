#ifndef BML_CORE_SEMANTIC_VERSION_H
#define BML_CORE_SEMANTIC_VERSION_H

#include <string>

namespace BML::Core {
    struct SemanticVersion {
        int major{0};
        int minor{0};
        int patch{0};
        std::string prerelease; // e.g., "alpha", "beta.1", "rc.2"
        std::string build_metadata; // e.g., "build.45"
    };

    enum class VersionOperator {
        Exact,
        GreaterEqual,           // ">="
        Greater,                // ">"
        LessEqual,              // "<="
        Less,                   // "<"
        Compatible,             // "^"
        ApproximatelyEquivalent // "~"
    };

    struct SemanticVersionRange {
        std::string raw_expression;
        VersionOperator op{VersionOperator::Exact};
        SemanticVersion version{};
        bool parsed{false};
        int parsed_components{0};
    };

    bool ParseSemanticVersion(const std::string &text,
                              SemanticVersion &out_version,
                              int *out_component_count = nullptr);
    bool ParseSemanticVersionRange(const std::string &text,
                                   SemanticVersionRange &out_range,
                                   std::string &out_error);
    bool IsVersionSatisfied(const SemanticVersionRange &range,
                            const SemanticVersion &version);
    bool IsVersionOutdated(const SemanticVersionRange &range,
                           const SemanticVersion &version,
                           std::string &out_suggestion);
} // namespace BML::Core

#endif // BML_CORE_SEMANTIC_VERSION_H
