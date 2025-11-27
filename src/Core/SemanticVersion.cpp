#include "SemanticVersion.h"

#include <cctype>
#include <cstdlib>

namespace BML::Core {

namespace {

static std::string Trim(const std::string &str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

static bool ParseInt(const std::string &text, int &out) {
    if (text.empty())
        return false;
    // Check for negative numbers - reject them
    if (text[0] == '-')
        return false;
    char *end = nullptr;
    long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0')
        return false;
    if (value < 0)  // Additional safety check
        return false;
    out = static_cast<int>(value);
    return true;
}

} // namespace

bool ParseSemanticVersion(const std::string &text, SemanticVersion &out_version) {
    auto trimmed = Trim(text);
    if (trimmed.empty())
        return false;

    // Check for prerelease suffix (e.g., "1.0.0-alpha" or "1.0.0-rc.1")
    std::string version_part = trimmed;
    std::string prerelease_part;
    size_t hyphen_pos = trimmed.find('-');
    if (hyphen_pos != std::string::npos) {
        version_part = trimmed.substr(0, hyphen_pos);
        prerelease_part = trimmed.substr(hyphen_pos + 1);
        // Validate prerelease is not empty
        if (prerelease_part.empty())
            return false;
    }

    int parts[3] = {0, 0, 0};
    size_t begin = 0;
    size_t part_index = 0;
    while (begin <= version_part.size() && part_index < 3) {
        size_t end = version_part.find('.', begin);
        if (end == std::string::npos)
            end = version_part.size();
        auto token = version_part.substr(begin, end - begin);
        int value = 0;
        if (!ParseInt(token, value))
            return false;
        parts[part_index++] = value;
        begin = end + 1;
    }

    out_version.major = parts[0];
    out_version.minor = parts[1];
    out_version.patch = parts[2];
    out_version.prerelease = prerelease_part;
    return true;
}

bool ParseSemanticVersionRange(const std::string &text,
                               SemanticVersionRange &out_range,
                               std::string &out_error) {
    out_range.raw_expression = text;
    out_range.op = VersionOperator::Exact;
    out_range.version = {};
    out_range.parsed = false;

    auto trimmed = Trim(text);
    if (trimmed.empty()) {
        out_error = "Version expression cannot be empty";
        return false;
    }

    size_t offset = 0;
    VersionOperator op = VersionOperator::Exact;
    if (trimmed.rfind(">=", 0) == 0) {
        op = VersionOperator::GreaterEqual;
        offset = 2;
    } else if (trimmed.rfind("<=", 0) == 0) {
        op = VersionOperator::LessEqual;
        offset = 2;
    } else if (trimmed[0] == '>') {
        op = VersionOperator::Greater;
        offset = 1;
    } else if (trimmed[0] == '<') {
        op = VersionOperator::Less;
        offset = 1;
    } else if (trimmed[0] == '^') {
        op = VersionOperator::Compatible;
        offset = 1;
    } else if (trimmed[0] == '~') {
        op = VersionOperator::ApproximatelyEquivalent;
        offset = 1;
    } else if (trimmed[0] == '=') {
        op = VersionOperator::Exact;
        offset = 1;
    }

    auto version_text = Trim(trimmed.substr(offset));
    SemanticVersion version;
    if (!ParseSemanticVersion(version_text, version)) {
        out_error = "Invalid semantic version: " + version_text;
        return false;
    }

    out_range.op = op;
    out_range.version = version;
    out_range.parsed = true;
    return true;
}

static bool CompatibleMatch(const SemanticVersion &requested, const SemanticVersion &actual) {
    if (requested.major > 0) {
        return actual.major == requested.major &&
               (actual.minor > requested.minor ||
                (actual.minor == requested.minor && actual.patch >= requested.patch));
    }
    // Major zero: lock minor version as well
    return actual.major == requested.major &&
           actual.minor == requested.minor &&
           actual.patch >= requested.patch;
}

static bool ApproximateMatch(const SemanticVersion &requested, const SemanticVersion &actual) {
    return actual.major == requested.major &&
           actual.minor == requested.minor &&
           actual.patch >= requested.patch;
}

// Compare prerelease strings according to semver spec
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
// Empty prerelease has higher precedence than non-empty (1.0.0 > 1.0.0-alpha)
static int ComparePrereleases(const std::string &a, const std::string &b) {
    if (a.empty() && b.empty())
        return 0;
    if (a.empty())
        return 1;  // no prerelease > prerelease
    if (b.empty())
        return -1; // prerelease < no prerelease
    
    // Simple lexicographic comparison for prerelease identifiers
    // A full implementation would split by '.' and compare each identifier
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

// Compare two semantic versions
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
static int CompareVersions(const SemanticVersion &a, const SemanticVersion &b) {
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    return ComparePrereleases(a.prerelease, b.prerelease);
}

bool IsVersionSatisfied(const SemanticVersionRange &range,
                        const SemanticVersion &version) {
    if (!range.parsed)
        return true; // treat unknown expressions as satisfied

    int cmp = CompareVersions(version, range.version);

    switch (range.op) {
        case VersionOperator::Exact:
            return cmp == 0;
        case VersionOperator::GreaterEqual:
            return cmp >= 0;
        case VersionOperator::Greater:
            return cmp > 0;
        case VersionOperator::LessEqual:
            return cmp <= 0;
        case VersionOperator::Less:
            return cmp < 0;
        case VersionOperator::Compatible:
            return CompatibleMatch(range.version, version);
        case VersionOperator::ApproximatelyEquivalent:
            return ApproximateMatch(range.version, version);
    }
    return false;
}

bool IsVersionOutdated(const SemanticVersionRange &range,
                       const SemanticVersion &version,
                       std::string &out_suggestion) {
    if (!range.parsed || !IsVersionSatisfied(range, version))
        return false;

    // For GreaterEqual: warn if major version matches but minor is at minimum
    if (range.op == VersionOperator::GreaterEqual) {
        if (version.major == range.version.major &&
            version.minor == range.version.minor &&
            version.patch == range.version.patch) {
            out_suggestion = "Consider upgrading to a newer minor/patch version";
            return true;
        }
    }

    // For Compatible (^): warn if at exact minimum version
    if (range.op == VersionOperator::Compatible) {
        if (version.major == range.version.major &&
            version.minor == range.version.minor &&
            version.patch == range.version.patch) {
            out_suggestion = "Consider upgrading to latest compatible version";
            return true;
        }
    }

    // For ApproximatelyEquivalent (~): warn if at exact minimum version
    if (range.op == VersionOperator::ApproximatelyEquivalent) {
        if (version.major == range.version.major &&
            version.minor == range.version.minor &&
            version.patch == range.version.patch) {
            out_suggestion = "Consider upgrading to latest patch version";
            return true;
        }
    }

    return false;
}

} // namespace BML::Core
