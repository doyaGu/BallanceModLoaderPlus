#ifndef BML_CORE_SEMANTIC_VERSION_H
#define BML_CORE_SEMANTIC_VERSION_H

#include <string>

namespace BML::Core {

struct SemanticVersion {
    int major{0};
    int minor{0};
    int patch{0};
};

enum class VersionOperator {
    Exact,
    GreaterEqual,
    Compatible,      // "^"
    ApproximatelyEquivalent // "~"
};

struct SemanticVersionRange {
    std::string raw_expression;
    VersionOperator op{VersionOperator::Exact};
    SemanticVersion version{};
    bool parsed{false};
};

bool ParseSemanticVersion(const std::string &text, SemanticVersion &out_version);
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
