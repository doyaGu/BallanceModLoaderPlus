#include "SemanticVersion.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <vector>

namespace BML::Core {
    namespace {
        std::string Trim(const std::string &str) {
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

        bool ParseInt(const std::string &text, int &out) {
            if (text.empty())
                return false;
            if (text[0] == '-')
                return false;
            if (text.size() > 1 && text[0] == '0')
                return false; // no leading zeros per semver
            errno = 0;
            char *end = nullptr;
            long value = std::strtol(text.c_str(), &end, 10);
            if (errno == ERANGE)
                return false;
            if (end == text.c_str() || *end != '\0')
                return false;
            if (value < 0 || value > std::numeric_limits<int>::max())
                return false;
            out = static_cast<int>(value);
            return true;
        }

        bool ValidateIdentifiers(std::string_view text) {
            if (text.empty())
                return false;

            size_t begin = 0;
            while (begin < text.size()) {
                size_t end = text.find('.', begin);
                if (end == std::string_view::npos)
                    end = text.size();

                auto identifier = text.substr(begin, end - begin);
                if (identifier.empty())
                    return false;

                bool numeric = true;
                for (char ch : identifier) {
                    unsigned char c = static_cast<unsigned char>(ch);
                    if (!std::isalnum(c) && ch != '-')
                        return false;
                    if (!std::isdigit(c))
                        numeric = false;
                }

                if (numeric && identifier.size() > 1 && identifier.front() == '0')
                    return false;

                begin = end + 1;
            }

            return true;
        }

        std::vector<std::string_view> SplitIdentifiers(const std::string &text) {
            std::vector<std::string_view> parts;
            if (text.empty())
                return parts;

            std::string_view view{text};
            size_t begin = 0;
            while (begin < view.size()) {
                size_t end = view.find('.', begin);
                if (end == std::string_view::npos)
                    end = view.size();
                parts.emplace_back(view.substr(begin, end - begin));
                begin = end + 1;
            }
            return parts;
        }

        bool IsNumericIdentifier(std::string_view id) {
            if (id.empty())
                return false;
            for (char ch : id) {
                if (!std::isdigit(static_cast<unsigned char>(ch)))
                    return false;
            }
            return true;
        }

        std::string_view StripLeadingZeros(std::string_view id) {
            size_t offset = 0;
            while (offset + 1 < id.size() && id[offset] == '0') {
                ++offset;
            }
            return id.substr(offset);
        }
    } // namespace

    bool ParseSemanticVersion(const std::string &text,
                              SemanticVersion &out_version,
                              int *out_component_count) {
        auto trimmed = Trim(text);
        if (trimmed.empty())
            return false;

        if (!trimmed.empty() && (trimmed[0] == 'v' || trimmed[0] == 'V')) {
            trimmed = Trim(trimmed.substr(1));
            if (trimmed.empty())
                return false;
        }

        std::string base_part = trimmed;
        std::string prerelease_part;
        std::string build_part;

        size_t plus_pos = base_part.find('+');
        if (plus_pos != std::string::npos) {
            build_part = base_part.substr(plus_pos + 1);
            base_part = base_part.substr(0, plus_pos);
            if (build_part.empty() || !ValidateIdentifiers(build_part))
                return false;
        }

        size_t hyphen_pos = base_part.find('-');
        if (hyphen_pos != std::string::npos) {
            prerelease_part = base_part.substr(hyphen_pos + 1);
            base_part = base_part.substr(0, hyphen_pos);
            if (prerelease_part.empty() || !ValidateIdentifiers(prerelease_part))
                return false;
        }

        if (base_part.empty() || base_part.back() == '.')
            return false;

        int parts[3] = {0, 0, 0};
        size_t begin = 0;
        size_t part_index = 0;
        while (begin < base_part.size() && part_index < 3) {
            size_t end = base_part.find('.', begin);
            if (end == std::string::npos)
                end = base_part.size();
            auto token = base_part.substr(begin, end - begin);
            int value = 0;
            if (!ParseInt(token, value))
                return false;
            parts[part_index++] = value;
            begin = (end == base_part.size()) ? base_part.size() : end + 1;
        }

        if (begin < base_part.size())
            return false; // extra components present

        if (part_index == 0)
            return false;

        SemanticVersion parsed{};
        parsed.major = parts[0];
        parsed.minor = parts[1];
        parsed.patch = parts[2];
        parsed.prerelease = prerelease_part;
        parsed.build_metadata = build_part;

        out_version = std::move(parsed);
        if (out_component_count)
            *out_component_count = static_cast<int>(part_index);
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
        int parsed_components = 0;
        if (!ParseSemanticVersion(version_text, version, &parsed_components)) {
            out_error = "Invalid semantic version: " + (version_text.empty() ? std::string{"<empty>"} : version_text);
            return false;
        }

        out_range.op = op;
        out_range.version = version;
        out_range.parsed_components = parsed_components;
        out_range.parsed = true;
        return true;
    }

    // Compare prerelease strings according to semver spec
    // Returns: -1 if a < b, 0 if a == b, 1 if a > b
    // Empty prerelease has higher precedence than non-empty (1.0.0 > 1.0.0-alpha)
    static int ComparePrereleases(const std::string &a, const std::string &b) {
        if (a.empty() && b.empty())
            return 0;
        if (a.empty())
            return 1;
        if (b.empty())
            return -1;

        auto identifiers_a = SplitIdentifiers(a);
        auto identifiers_b = SplitIdentifiers(b);
        size_t compare_count = std::min(identifiers_a.size(), identifiers_b.size());

        for (size_t i = 0; i < compare_count; ++i) {
            auto lhs = identifiers_a[i];
            auto rhs = identifiers_b[i];
            bool lhs_numeric = IsNumericIdentifier(lhs);
            bool rhs_numeric = IsNumericIdentifier(rhs);

            if (lhs_numeric && rhs_numeric) {
                auto lhs_norm = StripLeadingZeros(lhs);
                auto rhs_norm = StripLeadingZeros(rhs);
                if (lhs_norm.size() != rhs_norm.size())
                    return lhs_norm.size() < rhs_norm.size() ? -1 : 1;
                int cmp = lhs_norm.compare(rhs_norm);
                if (cmp != 0)
                    return cmp < 0 ? -1 : 1;
            } else if (lhs_numeric != rhs_numeric) {
                return lhs_numeric ? -1 : 1;
            } else {
                int cmp = lhs.compare(rhs);
                if (cmp != 0)
                    return cmp < 0 ? -1 : 1;
            }
        }

        if (identifiers_a.size() == identifiers_b.size())
            return 0;
        return identifiers_a.size() < identifiers_b.size() ? -1 : 1;
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

    static SemanticVersion StripQualifiers(const SemanticVersion &version) {
        SemanticVersion base = version;
        base.prerelease.clear();
        base.build_metadata.clear();
        return base;
    }

    static SemanticVersion ComputeCaretUpperBound(const SemanticVersion &version) {
        SemanticVersion upper = StripQualifiers(version);
        if (version.major != 0) {
            ++upper.major;
            upper.minor = 0;
            upper.patch = 0;
        } else if (version.minor != 0) {
            ++upper.minor;
            upper.patch = 0;
        } else {
            ++upper.patch;
        }
        return upper;
    }

    static SemanticVersion ComputeTildeUpperBound(const SemanticVersion &version, int parsed_components) {
        SemanticVersion upper = StripQualifiers(version);
        int components = std::max(parsed_components, 1);
        if (components <= 1) {
            ++upper.major;
            upper.minor = 0;
            upper.patch = 0;
        } else {
            ++upper.minor;
            upper.patch = 0;
        }
        return upper;
    }

    static bool VersionInHalfOpenRange(const SemanticVersion &lower,
                                       const SemanticVersion &upper,
                                       const SemanticVersion &candidate) {
        return CompareVersions(candidate, lower) >= 0 &&
               CompareVersions(candidate, upper) < 0;
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
        case VersionOperator::Compatible: {
            auto upper = ComputeCaretUpperBound(range.version);
            return VersionInHalfOpenRange(range.version, upper, version);
        }
        case VersionOperator::ApproximatelyEquivalent: {
            auto upper = ComputeTildeUpperBound(range.version, range.parsed_components);
            return VersionInHalfOpenRange(range.version, upper, version);
        }
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
