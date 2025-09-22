#ifndef BML_INIFILE_H
#define BML_INIFILE_H

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <functional>

/**
 * A UTF-8 aware utility class for parsing and modifying INI files.
 * Preserves comments, empty lines, and formatting while allowing modifications.
 * Handles international text correctly using proper UTF-8 string operations.
 */
class IniFile {
public:
    struct KeyValue {
        std::string key;
        std::string value;
        std::string originalLine;
        std::string inlineComment;  // Comment that appears after the value on same line
        std::string precedingComment; // Comment line(s) that appear before this key
        bool isComment = false;
        bool isEmpty = false;
        size_t lineNumber = 0; // Line number in original file for error reporting

        KeyValue() = default;

        KeyValue(const std::string &k, const std::string &v, const std::string &line = "")
            : key(k), value(v), originalLine(line.empty() ? (k + " = " + v) : line) {}
    };

    struct Section {
        std::string name;
        std::string headerLine;
        std::vector<KeyValue> entries;
        mutable std::unordered_map<std::string, size_t> m_KeyIndex; // For O(1) key lookup
        mutable bool m_KeyIndexDirty = true;
        size_t lineNumber = 0;

        Section() = default;

        explicit Section(const std::string &sectionName) : name(sectionName), headerLine("[" + sectionName + "]") {}

        // Update key index after modifications - must be called when entries change
        void RebuildKeyIndex(const std::function<std::string(const std::string &)> &normalizer) const;
        void MarkKeyIndexDirty() const { m_KeyIndexDirty = true; }

        // Find key with O(1) lookup
        KeyValue *FindKey(const std::string &normalizedKey) const;
    };

    // Function types for flexible operations
    using KeyMatcher = std::function<bool(const std::string &existingKey, const std::string &targetKey)>;
    using KeyCanonicalizer = std::function<std::string(const std::string &key)>;
    using SectionInsertLogic = std::function<size_t(const std::vector<Section> &, const std::string &)>;

    // Construction
    IniFile();
    ~IniFile();

    // Parsing operations
    bool ParseFromString(const std::string &content);
    bool ParseFromFile(const std::wstring &filePath);

    // Writing operations
    std::string WriteToString() const;
    bool WriteToFile(const std::wstring &filePath) const;

    // Section operations
    bool HasSection(const std::string &sectionName) const;
    Section *GetSection(const std::string &sectionName);
    const Section *GetSection(const std::string &sectionName) const;
    Section *AddSection(const std::string &sectionName);
    bool RemoveSection(const std::string &sectionName);
    std::vector<std::string> GetSectionNames() const;

    // Key-value operations
    bool HasKey(const std::string &sectionName, const std::string &key) const;
    std::string GetValue(const std::string &sectionName, const std::string &key,
                         const std::string &defaultValue = "") const;
    bool SetValue(const std::string &sectionName, const std::string &key, const std::string &value);
    bool SetValueWithComment(const std::string &sectionName, const std::string &key,
                            const std::string &value, const std::string &inlineComment = "");
    bool RemoveKey(const std::string &sectionName, const std::string &key);

    // Comment operations
    std::string GetInlineComment(const std::string &sectionName, const std::string &key) const;
    bool SetInlineComment(const std::string &sectionName, const std::string &key, const std::string &comment);
    std::string GetPrecedingComment(const std::string &sectionName, const std::string &key) const;
    bool SetPrecedingComment(const std::string &sectionName, const std::string &key, const std::string &comment);

    // Header comment operations
    std::string GetHeaderComment() const;
    bool SetHeaderComment(const std::string &comment);
    void ClearHeaderComment();

    // Bulk operations
    struct Mutation {
        std::string key;
        std::string value;
        bool remove = false;

        Mutation(std::string k, std::string v, bool rem = false) : key(std::move(k)), value(std::move(v)), remove(rem) {}
    };

    bool ApplyMutations(const std::string &sectionName,
                        const std::vector<Mutation> &mutations,
                        KeyMatcher matcher = nullptr,
                        KeyCanonicalizer canonicalizer = nullptr);

    // Configuration
    void SetSectionInsertionLogic(SectionInsertLogic logic);
    void SetCaseSensitive(bool caseSensitive);
    bool IsCaseSensitive() const { return m_CaseSensitive; }

    // UTF-8 specific settings
    void SetStrictUtf8Validation(bool strict) { m_StrictUtf8 = strict; }
    bool IsStrictUtf8Validation() const { return m_StrictUtf8; }

    // Utility
    void Clear();
    bool IsEmpty() const;
    size_t GetSectionCount() const { return m_Sections.size(); }

    // Access to raw sections (for advanced usage)
    const std::vector<Section> &GetSections() const { return m_Sections; }

    // Error reporting
    std::string GetLastError() const { return m_LastError; }
    void ClearError() { m_LastError.clear(); }

    // UTF-8 validation
    bool IsValidUtf8(const std::string &str) const;
    size_t GetUtf8Length(const std::string &str) const;

private:
    std::vector<Section> m_Sections;
    mutable std::unordered_map<std::string, size_t> m_SectionIndex; // For O(1) section lookup
    std::vector<std::string> m_LeadingComments;
    SectionInsertLogic m_SectionInsertLogic;
    bool m_CaseSensitive = false;
    bool m_StrictUtf8 = true;
    mutable std::string m_LastError;

    // UTF-8 aware helper methods
    std::string TrimUtf8String(const std::string &str) const;
    std::string NormalizeUtf8Key(const std::string &key) const;
    std::string NormalizeUtf8SectionName(const std::string &name) const;
    std::string ToLowerUtf8(const std::string &str) const;
    int CompareUtf8(const std::string &a, const std::string &b, bool caseSensitive) const;
    bool IsUtf8Whitespace(const char *str, size_t *advance) const;

    // Line classification
    bool IsCommentLine(const std::string &line) const;
    bool IsEmptyLine(const std::string &line) const;
    bool IsSectionHeader(const std::string &line, std::string &sectionName) const;
    bool ParseKeyValue(const std::string &line, std::string &key, std::string &value) const;
    bool ParseKeyValueWithComment(const std::string &line, std::string &key, std::string &value, std::string &comment) const;

    // Comment helpers
    std::string ExtractInlineComment(const std::string &line) const;
    std::string StripInlineComment(const std::string &line) const;
    std::string FormatKeyValueWithComment(const std::string &key, const std::string &value, const std::string &comment) const;

    // Validation
    bool IsValidUtf8SectionName(const std::string &name) const;
    bool IsValidUtf8Key(const std::string &key) const;

    // Internal operations
    size_t FindSectionIndex(const std::string &sectionName) const;
    size_t GetDefaultSectionInsertPosition(const std::string &sectionName) const;
    void RebuildSectionIndex() const;
    KeyValue *FindKeyInSection(Section *section, const std::string &key);
    const KeyValue *FindKeyInSection(const Section *section, const std::string &key) const;

    // Error handling
    void SetError(const std::string &error) const { m_LastError = error; }

    // Constants (in UTF-8 codepoints)
    static constexpr size_t MAX_LINE_CODEPOINTS = 8192;
    static constexpr size_t MAX_SECTIONS = 1000;
    static constexpr size_t MAX_KEYS_PER_SECTION = 1000;
    static constexpr size_t MAX_KEY_CODEPOINTS = 255;
    static constexpr size_t MAX_SECTION_CODEPOINTS = 255;
};

#endif // BML_INIFILE_H
