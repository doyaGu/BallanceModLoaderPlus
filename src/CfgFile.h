#ifndef BML_CFGFILE_H
#define BML_CFGFILE_H

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <variant>
#include <functional>

/**
 * Property types supported by CFG files.
 * Each type corresponds to a specific character prefix in the CFG format.
 */
enum class CfgPropertyType {
    NONE,
    STRING,  // S - String value
    BOOLEAN, // B - Boolean value (true/false)
    INTEGER, // I - Integer value
    FLOAT,   // F - Floating point value
    KEY      // K - Keyboard key value (stored as integer)
};

/**
 * A UTF-8 aware utility class for parsing and writing CFG files.
 *
 * CFG files have a specific format used for configuration:
 * - Comments start with '#'
 * - Categories are defined by name followed by '{'
 * - Properties have format: [Type] [Name] [Value]
 * - Categories end with '}'
 *
 * Example CFG file:
 * # This is a comment
 * Graphics {
 *     # Screen resolution
 *     I Width 1920
 *     I Height 1080
 *     B Fullscreen true
 *     S Title "My Game"
 * }
 *
 * The class provides type-safe access to properties and preserves
 * comments and formatting when possible.
 */
class CfgFile {
public:
    struct Property {
        std::string name;
        CfgPropertyType type = CfgPropertyType::NONE;
        std::variant<std::string, bool, int, float> value;
        std::string comment;
        size_t lineNumber = 0;

        Property() = default;

        Property(std::string n, CfgPropertyType t) : name(std::move(n)), type(t) {}

        // Type-safe value accessors
        std::string GetString() const;
        bool GetBoolean() const;
        int GetInteger() const;
        float GetFloat() const;

        // Type-safe value setters
        void SetString(const std::string &val);
        void SetBoolean(bool val);
        void SetInteger(int val);
        void SetFloat(float val);
    };

    struct Category {
        std::string name;
        std::string comment;
        std::vector<Property> properties;
        mutable std::unordered_map<std::string, size_t> m_PropertyIndex;
        mutable bool m_PropertyIndexDirty = true;
        bool caseSensitive = false;
        size_t lineNumber = 0;

        Category() = default;

        explicit Category(std::string categoryName) : name(std::move(categoryName)) {}

        // Property management
        Property *FindProperty(const std::string &propName);
        const Property *FindProperty(const std::string &propName) const;
        Property *AddProperty(const std::string &propName, CfgPropertyType type);
        bool RemoveProperty(const std::string &propName);
        bool HasProperty(const std::string &propName) const;

        // Index management
        void RebuildPropertyIndex(const std::function<std::string(const std::string &)> &normalizer) const;
        void MarkPropertyIndexDirty() const { m_PropertyIndexDirty = true; }
    };

    // Construction and destruction
    CfgFile();
    ~CfgFile();

    // File operations
    bool ParseFromString(const std::string &content);
    bool ParseFromFile(const std::wstring &filePath);
    bool ParseFromFile(const std::string &filePath);
    std::string WriteToString() const;
    bool WriteToFile(const std::wstring &filePath) const;
    bool WriteToFile(const std::string &filePath) const;

    // Category operations
    bool HasCategory(const std::string &categoryName) const;
    Category *GetCategory(const std::string &categoryName);
    const Category *GetCategory(const std::string &categoryName) const;
    Category *AddCategory(const std::string &categoryName);
    bool RemoveCategory(const std::string &categoryName);
    std::vector<std::string> GetCategoryNames() const;

    // Property operations
    bool HasProperty(const std::string &categoryName, const std::string &propName) const;
    Property *GetProperty(const std::string &categoryName, const std::string &propName);
    const Property *GetProperty(const std::string &categoryName, const std::string &propName) const;

    // Type-safe property setters
    bool SetStringProperty(const std::string &categoryName, const std::string &propName, const std::string &value);
    bool SetBooleanProperty(const std::string &categoryName, const std::string &propName, bool value);
    bool SetIntegerProperty(const std::string &categoryName, const std::string &propName, int value);
    bool SetFloatProperty(const std::string &categoryName, const std::string &propName, float value);
    bool SetKeyProperty(const std::string &categoryName, const std::string &propName, int value);

    // Type-safe property getters
    std::string GetStringProperty(const std::string &categoryName, const std::string &propName, const std::string &defaultValue = "") const;
    bool GetBooleanProperty(const std::string &categoryName, const std::string &propName, bool defaultValue = false) const;
    int GetIntegerProperty(const std::string &categoryName, const std::string &propName, int defaultValue = 0) const;
    float GetFloatProperty(const std::string &categoryName, const std::string &propName, float defaultValue = 0.0f) const;
    int GetKeyProperty(const std::string &categoryName, const std::string &propName, int defaultValue = 0) const;

    // Comment operations
    std::string GetCategoryComment(const std::string &categoryName) const;
    bool SetCategoryComment(const std::string &categoryName, const std::string &comment);
    std::string GetPropertyComment(const std::string &categoryName, const std::string &propName) const;
    bool SetPropertyComment(const std::string &categoryName, const std::string &propName, const std::string &comment);

    // Header comment operations
    std::string GetHeaderComment() const;
    bool SetHeaderComment(const std::string &comment);
    void ClearHeaderComment();

    // Configuration
    void SetCaseSensitive(bool caseSensitive);
    bool IsCaseSensitive() const { return m_CaseSensitive; }
    void SetStrictUtf8Validation(bool strict) { m_StrictUtf8 = strict; }
    bool IsStrictUtf8Validation() const { return m_StrictUtf8; }

    // Utility
    void Clear();
    bool IsEmpty() const;
    size_t GetCategoryCount() const { return m_Categories.size(); }

    // Error reporting
    std::string GetLastError() const { return m_LastError; }
    void ClearError() { m_LastError.clear(); }

    // UTF-8 validation
    bool IsValidUtf8(const std::string &str) const;
    size_t GetUtf8Length(const std::string &str) const;

private:
    std::vector<Category> m_Categories;
    mutable std::unordered_map<std::string, size_t> m_CategoryIndex;
    std::vector<std::string> m_LeadingComments;
    bool m_CaseSensitive = false;
    bool m_StrictUtf8 = true;
    mutable std::string m_LastError;

    // UTF-8 aware helper methods
    std::string TrimUtf8String(const std::string &str) const;
    std::string NormalizeUtf8Name(const std::string &name) const;
    std::string ToLowerUtf8(const std::string &str) const;
    int CompareUtf8(const std::string &a, const std::string &b, bool caseSensitive) const;
    bool IsUtf8Whitespace(const char *str, size_t *advance) const;

    // Parsing helpers
    bool IsCommentLine(const std::string &line) const;
    bool IsEmptyLine(const std::string &line) const;
    bool IsCategoryHeader(const std::string &line, std::string &categoryName) const;
    CfgPropertyType CharToPropertyType(wchar_t c) const;
    wchar_t PropertyTypeToChar(CfgPropertyType type) const;

    // Validation
    bool IsValidUtf8CategoryName(const std::string &name) const;
    bool IsValidUtf8PropertyName(const std::string &name) const;

    // Internal operations
    size_t FindCategoryIndex(const std::string &categoryName) const;
    void RebuildCategoryIndex() const;

    // Error handling
    void SetError(const std::string &error) const { m_LastError = error; }

    // Constants
    static constexpr size_t MAX_LINE_CODEPOINTS = 8192;
    static constexpr size_t MAX_CATEGORIES = 1000;
    static constexpr size_t MAX_PROPERTIES_PER_CATEGORY = 1000;
    static constexpr size_t MAX_NAME_CODEPOINTS = 4096;
};

#endif // BML_CFGFILE_H
