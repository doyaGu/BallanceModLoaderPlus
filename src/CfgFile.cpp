#include "CfgFile.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <utility>

#include <utf8.h>

#include "StringUtils.h"
#include "PathUtils.h"

// Property implementation
std::string CfgFile::Property::GetString() const {
    try {
        return type == CfgPropertyType::STRING ? std::get<std::string>(value) : "";
    } catch (const std::bad_variant_access &) {
        return "";
    }
}

bool CfgFile::Property::GetBoolean() const {
    try {
        return type == CfgPropertyType::BOOLEAN ? std::get<bool>(value) : false;
    } catch (const std::bad_variant_access &) {
        return false;
    }
}

int CfgFile::Property::GetInteger() const {
    try {
        return (type == CfgPropertyType::INTEGER || type == CfgPropertyType::KEY) ? std::get<int>(value) : 0;
    } catch (const std::bad_variant_access &) {
        return 0;
    }
}

float CfgFile::Property::GetFloat() const {
    try {
        return type == CfgPropertyType::FLOAT ? std::get<float>(value) : 0.0f;
    } catch (const std::bad_variant_access &) {
        return 0.0f;
    }
}

void CfgFile::Property::SetString(const std::string &val) {
    type = CfgPropertyType::STRING;
    value = val;
}

void CfgFile::Property::SetBoolean(bool val) {
    type = CfgPropertyType::BOOLEAN;
    value = val;
}

void CfgFile::Property::SetInteger(int val) {
    type = CfgPropertyType::INTEGER;
    value = val;
}

void CfgFile::Property::SetFloat(float val) {
    type = CfgPropertyType::FLOAT;
    value = val;
}

// Category implementation
CfgFile::Property *CfgFile::Category::FindProperty(const std::string &propName) {
    RebuildPropertyIndex(nullptr);

    std::string normalizedName = caseSensitive ? propName : utils::ToLower(propName);
    auto it = m_PropertyIndex.find(normalizedName);
    return (it != m_PropertyIndex.end() && it->second < properties.size()) ? &properties[it->second] : nullptr;
}

const CfgFile::Property *CfgFile::Category::FindProperty(const std::string &propName) const {
    RebuildPropertyIndex(nullptr);

    std::string normalizedName = caseSensitive ? propName : utils::ToLower(propName);
    auto it = m_PropertyIndex.find(normalizedName);
    return (it != m_PropertyIndex.end() && it->second < properties.size()) ? &properties[it->second] : nullptr;
}

CfgFile::Property *CfgFile::Category::AddProperty(const std::string &propName, CfgPropertyType type) {
    if (propName.empty() || properties.size() >= 1000) {
        return nullptr;
    }

    // Check if property already exists
    Property *existing = FindProperty(propName);
    if (existing) {
        existing->type = type;
        return existing;
    }

    properties.emplace_back(propName, type);
    MarkPropertyIndexDirty();
    return &properties.back();
}

bool CfgFile::Category::RemoveProperty(const std::string &propName) {
    std::string normalizedName = caseSensitive ? propName : utils::ToLower(propName);

    auto it = std::remove_if(properties.begin(), properties.end(),
                             [this, &normalizedName](const Property &prop) {
                                 const std::string currentName = caseSensitive ? prop.name : utils::ToLower(prop.name);
                                 return currentName == normalizedName;
                             });

    bool removed = (it != properties.end());
    properties.erase(it, properties.end());

    if (removed) {
        MarkPropertyIndexDirty();
    }

    return removed;
}

bool CfgFile::Category::HasProperty(const std::string &propName) const {
    return FindProperty(propName) != nullptr;
}

void CfgFile::Category::RebuildPropertyIndex(const std::function<std::string(const std::string &)> &normalizer) const {
    if (!m_PropertyIndexDirty) {
        return;
    }

    m_PropertyIndex.clear();
    m_PropertyIndex.reserve(properties.size());

    std::function<std::string(const std::string &)> effective = normalizer;
    if (!effective) {
        effective = [this](const std::string &name) {
            return caseSensitive ? name : utils::ToLower(name);
        };
    }

    for (size_t i = 0; i < properties.size(); ++i) {
        const auto &prop = properties[i];
        if (!prop.name.empty()) {
            std::string normalizedName = effective(prop.name);
            if (!normalizedName.empty()) {
                m_PropertyIndex[normalizedName] = i;
            }
        }
    }

    m_PropertyIndexDirty = false;
}

// CfgFile implementation
CfgFile::CfgFile() = default;

CfgFile::~CfgFile() {
    Clear();
}

bool CfgFile::IsValidUtf8(const std::string &str) const {
    if (str.empty()) return true;
    return utf8valid((const utf8_int8_t *) str.c_str()) == nullptr;
}

size_t CfgFile::GetUtf8Length(const std::string &str) const {
    if (str.empty()) return 0;
    if (!IsValidUtf8(str)) return 0;
    return utf8len((const utf8_int8_t *) str.c_str());
}

std::string CfgFile::TrimUtf8String(const std::string &str) const {
    if (str.empty()) return str;

    if (m_StrictUtf8 && !IsValidUtf8(str)) {
        return str;
    }

    const char *start = str.c_str();
    const char *end = start + str.length();

    // Trim leading whitespace
    while (start < end) {
        size_t advance = 0;
        if (IsUtf8Whitespace(start, &advance)) {
            start += advance;
        } else {
            break;
        }
    }

    // Trim trailing whitespace
    const char *trimEnd = end;
    while (trimEnd > start) {
        const char *prev = trimEnd - 1;

        while (prev > start && (static_cast<unsigned char>(*prev) & 0xC0) == 0x80) {
            prev--;
        }

        if (prev < start) break;

        size_t advance = 0;
        if (IsUtf8Whitespace(prev, &advance)) {
            trimEnd = prev;
        } else {
            break;
        }
    }

    if (start >= trimEnd) {
        return "";
    }

    return std::string(start, trimEnd - start);
}

std::string CfgFile::ToLowerUtf8(const std::string &str) const {
    if (str.empty()) return str;

    if (m_StrictUtf8 && !IsValidUtf8(str)) {
        SetError("Invalid UTF-8 sequence in string");
        return str;
    }

    utf8_int8_t *strCopy = utf8dup((const utf8_int8_t *) str.c_str());
    if (!strCopy) return str;

    utf8lwr((utf8_int8_t *) strCopy);
    std::string result((char *) strCopy);
    free(strCopy);
    return result;
}

std::string CfgFile::NormalizeUtf8Name(const std::string &name) const {
    std::string trimmed = TrimUtf8String(name);
    if (m_CaseSensitive) {
        return trimmed;
    } else {
        return ToLowerUtf8(trimmed);
    }
}

int CfgFile::CompareUtf8(const std::string &a, const std::string &b, bool caseSensitive) const {
    if (caseSensitive) {
        return utf8cmp((const utf8_int8_t *) a.c_str(), (const utf8_int8_t *) b.c_str());
    } else {
        return utf8casecmp((const utf8_int8_t *) a.c_str(), (const utf8_int8_t *) b.c_str());
    }
}

bool CfgFile::IsUtf8Whitespace(const char *str, size_t *advance) const {
    if (!str || !*str) return false;

    utf8_int32_t codepoint;
    const char *next = (const char *) utf8codepoint((const utf8_int8_t *) str, &codepoint);
    if (!next) return false;

    if (advance) {
        *advance = next - str;
    }

    return (codepoint == 0x20) || // Space
        (codepoint == 0x09) ||    // Tab
        (codepoint == 0x0A) ||    // Line Feed
        (codepoint == 0x0D) ||    // Carriage Return
        (codepoint == 0x0B) ||    // Vertical Tab
        (codepoint == 0x0C) ||    // Form Feed
        (codepoint == 0xA0) ||    // Non-breaking space
        (codepoint >= 0x2000 && codepoint <= 0x200A) ||
        (codepoint == 0x2028) ||
        (codepoint == 0x2029) ||
        (codepoint == 0x202F) ||
        (codepoint == 0x205F) ||
        (codepoint == 0x3000);
}

bool CfgFile::ParseFromString(const std::string &content) {
    ClearError();
    Clear();

    if (content.empty()) {
        return true;
    }

    if (m_StrictUtf8 && !IsValidUtf8(content)) {
        SetError("Invalid UTF-8 content");
        return false;
    }

    auto joinComments = [](const std::vector<std::string> &comments) {
        std::string result;
        for (size_t i = 0; i < comments.size(); ++i) {
            if (i > 0) {
                result += '\n';
            }
            result += comments[i];
        }
        return result;
    };

    auto trimAscii = [](const std::string &value) -> std::string {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    };

    auto parseBool = [&](const std::string &text, bool &result) -> bool {
        std::string normalized = trimAscii(text);
        if (normalized.empty()) {
            return false;
        }

        std::string lowered = utils::ToLower(normalized);
        if (lowered == "true" || lowered == "yes" || lowered == "on") {
            result = true;
            return true;
        }
        if (lowered == "false" || lowered == "no" || lowered == "off") {
            result = false;
            return true;
        }

        char *end = nullptr;
        long numeric = std::strtol(normalized.c_str(), &end, 10);
        if (end && *end == '\0') {
            result = numeric != 0;
            return true;
        }

        return false;
    };

    auto unescapeString = [](const std::string &value) {
        std::string result;
        result.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i) {
            char ch = value[i];
            if (ch == '\\' && i + 1 < value.size()) {
                char next = value[++i];
                switch (next) {
                case '\\':
                case '"':
                case '\'':
                    result.push_back(next);
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    result.push_back(next);
                    break;
                }
            } else {
                result.push_back(ch);
            }
        }
        return result;
    };

    auto parseStringValue = [&](const std::string &raw, std::string &out) -> bool {
        std::string trimmed = TrimUtf8String(raw);
        if (trimmed.size() < 2) {
            return false;
        }

        char first = trimmed.front();
        char last = trimmed.back();
        if (!((first == '"' && last == '"') || (first == '\'' && last == '\''))) {
            return false;
        }

        out = unescapeString(trimmed.substr(1, trimmed.size() - 2));
        return true;
    };

    std::istringstream stream(content);
    std::string line;
    size_t lineNumber = 0;
    Category *currentCategory = nullptr;
    std::string pendingCategoryName;
    bool awaitingCategoryOpen = false;
    bool hasAnyContent = false;
    std::vector<std::string> commentBuffer;
    std::vector<bool> commentBufferIsLeading;
    std::vector<std::string> leadingCommentsBuffer;

    auto assignComment = [&](std::string &target) {
        if (commentBuffer.empty()) {
            target.clear();
            return;
        }

        target = joinComments(commentBuffer);

        size_t consumedLeading = 0;
        for (bool isLeading : commentBufferIsLeading) {
            if (isLeading) {
                ++consumedLeading;
            }
        }
        if (consumedLeading > 0 && consumedLeading <= leadingCommentsBuffer.size()) {
            leadingCommentsBuffer.erase(leadingCommentsBuffer.end() - consumedLeading, leadingCommentsBuffer.end());
        }

        commentBuffer.clear();
        commentBufferIsLeading.clear();
    };

    while (std::getline(stream, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = TrimUtf8String(line);
        if (trimmed.empty()) {
            // Empty lines separate header comments from section comments
            if (!commentBuffer.empty() && !hasAnyContent) {
                // If we have accumulated comments but no content yet, commit them as header comments
                // Only commit as header comments if ALL comments are leading candidates
                bool allAreLeading = true;
                for (bool isLeading : commentBufferIsLeading) {
                    if (!isLeading) {
                        allAreLeading = false;
                        break;
                    }
                }

                if (allAreLeading) {
                    for (size_t i = 0; i < commentBuffer.size(); ++i) {
                        std::string leading = "#";
                        if (!commentBuffer[i].empty()) {
                            leading += " " + commentBuffer[i];
                        }
                        leadingCommentsBuffer.push_back(std::move(leading));
                    }
                    commentBuffer.clear();
                    commentBufferIsLeading.clear();
                }
            }
            continue;
        }

        if (IsCommentLine(trimmed)) {
            std::string commentText = TrimUtf8String(trimmed.substr(1));
            bool isLeadingCandidate = !hasAnyContent && !currentCategory && pendingCategoryName.empty() && !awaitingCategoryOpen;

            commentBuffer.push_back(commentText);
            commentBufferIsLeading.push_back(isLeadingCandidate);

            // Note: We don't add to leadingCommentsBuffer here anymore,
            // it will be handled when we encounter an empty line or content

            continue;
        }

        if (trimmed == "{") {
            if (pendingCategoryName.empty()) {
                SetError("Unexpected '{' at line " + std::to_string(lineNumber));
                return false;
            }

            // Don't commit as header comments here - let assignComment handle it

            hasAnyContent = true;
            Category *category = AddCategory(pendingCategoryName);
            if (!category) {
                return false;
            }
            category->lineNumber = lineNumber;
            assignComment(category->comment);

            currentCategory = category;
            pendingCategoryName.clear();
            awaitingCategoryOpen = false;
            continue;
        }

        if (trimmed == "}") {
            if (!currentCategory) {
                SetError("Unexpected '}' at line " + std::to_string(lineNumber));
                return false;
            }

            currentCategory = nullptr;
            awaitingCategoryOpen = false;
            commentBuffer.clear();
            commentBufferIsLeading.clear();
            continue;
        }

        if (!trimmed.empty() && trimmed.back() == '{') {
            std::string namePart = TrimUtf8String(trimmed.substr(0, trimmed.size() - 1));
            if (namePart.empty()) {
                SetError("Empty category name at line " + std::to_string(lineNumber));
                return false;
            }

            // Don't commit as header comments here - let assignComment handle it

            hasAnyContent = true;
            Category *category = AddCategory(namePart);
            if (!category) {
                return false;
            }
            category->lineNumber = lineNumber;
            assignComment(category->comment);

            currentCategory = category;
            pendingCategoryName.clear();
            awaitingCategoryOpen = false;
            continue;
        }

        auto firstSpace = trimmed.find_first_of(" \t");
        std::string token = firstSpace == std::string::npos ? trimmed : trimmed.substr(0, firstSpace);
        auto isPropertyToken = [](char c) {
            return c == 'S' || c == 'B' || c == 'I' || c == 'F' || c == 'K';
        };

        if (token.size() != 1 || !isPropertyToken(token[0])) {
            // Don't commit as header comments here - let assignComment handle it

            hasAnyContent = true;
            pendingCategoryName = trimmed;
            awaitingCategoryOpen = true;
            currentCategory = nullptr;
            continue;
        }

        if (!currentCategory) {
            SetError("Property outside of a category at line " + std::to_string(lineNumber));
            return false;
        }

        std::istringstream lineStream(trimmed);
        hasAnyContent = true;
        std::string typeToken;
        std::string propertyName;
        lineStream >> typeToken >> propertyName;

        if (propertyName.empty()) {
            SetError("Missing property name at line " + std::to_string(lineNumber));
            return false;
        }

        CfgPropertyType type = CharToPropertyType(static_cast<wchar_t>(typeToken[0]));
        if (type == CfgPropertyType::NONE) {
            SetError("Unknown property type at line " + std::to_string(lineNumber));
            return false;
        }

        std::string rawValue;
        std::getline(lineStream, rawValue);

        Property *prop = currentCategory->AddProperty(propertyName, type);
        if (!prop) {
            SetError("Failed to add property '" + propertyName + "' at line " + std::to_string(lineNumber));
            return false;
        }

        prop->lineNumber = lineNumber;
        assignComment(prop->comment);

        bool parseSuccess = false;

        try {
            switch (type) {
            case CfgPropertyType::STRING: {
                std::string parsedValue;
                if (parseStringValue(rawValue, parsedValue)) {
                    prop->SetString(parsedValue);
                    parseSuccess = true;
                }
                break;
            }
            case CfgPropertyType::BOOLEAN: {
                bool value = false;
                if (parseBool(rawValue, value)) {
                    prop->SetBoolean(value);
                    parseSuccess = true;
                }
                break;
            }
            case CfgPropertyType::INTEGER: {
                int value = std::stoi(TrimUtf8String(rawValue));
                prop->SetInteger(value);
                parseSuccess = true;
                break;
            }
            case CfgPropertyType::FLOAT: {
                float value = std::stof(TrimUtf8String(rawValue));
                prop->SetFloat(value);
                parseSuccess = true;
                break;
            }
            case CfgPropertyType::KEY: {
                int value = std::stoi(TrimUtf8String(rawValue));
                prop->type = CfgPropertyType::KEY;
                prop->value = value;
                parseSuccess = true;
                break;
            }
            default:
                break;
            }
        } catch (const std::exception &) {
            parseSuccess = false;
        }

        if (!parseSuccess) {
            currentCategory->RemoveProperty(propertyName);
            SetError("Invalid value for property '" + propertyName + "' at line " + std::to_string(lineNumber));
            return false;
        }
    }

    if (currentCategory) {
        SetError("Category '" + currentCategory->name + "' missing closing brace");
        return false;
    }

    if (!pendingCategoryName.empty() || awaitingCategoryOpen) {
        SetError("Category '" + pendingCategoryName + "' missing opening brace");
        return false;
    }

    m_LeadingComments = std::move(leadingCommentsBuffer);
    RebuildCategoryIndex();
    return true;
}

bool CfgFile::ParseFromFile(const std::wstring &filePath) {
    ClearError();

    if (!utils::FileExistsW(filePath)) {
        SetError("File does not exist: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    FILE *fp = _wfopen(filePath.c_str(), L"rb");
    if (!fp) {
        SetError("Cannot open file: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size == 0) {
        fclose(fp);
        return ParseFromString("");
    }

    auto *buffer = new char[size + 1];
    size_t read = fread(buffer, sizeof(char), size, fp);
    fclose(fp);

    if (read != size) {
        delete[] buffer;
        SetError("Failed to read file: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    buffer[size] = 0;
    std::string content(buffer);
    delete[] buffer;

    return ParseFromString(content);
}

std::string CfgFile::WriteToString() const {
    std::ostringstream out;
    out << std::boolalpha;

    auto writeCommentLines = [&](const std::string &comment, const std::string &prefix) {
        if (comment.empty()) {
            return;
        }

        std::istringstream stream(comment);
        std::string line;
        while (std::getline(stream, line)) {
            out << prefix << line << std::endl;
        }
    };

    auto escapeString = [](const std::string &value) {
        std::string result;
        result.reserve(value.size() + 2);
        result.push_back('"');
        for (char ch : value) {
            switch (ch) {
            case '\\':
            case '"':
                result.push_back('\\');
                result.push_back(ch);
                break;
            case '\n':
                result.push_back('\\');
                result.push_back('n');
                break;
            case '\t':
                result.push_back('\\');
                result.push_back('t');
                break;
            default:
                result.push_back(ch);
                break;
            }
        }
        result.push_back('"');
        return result;
    };

    for (const auto &comment : m_LeadingComments) {
        out << comment << std::endl;
    }

    // Add empty line after header comments if there are categories
    if (!m_LeadingComments.empty() && !m_Categories.empty()) {
        out << std::endl;
    }

    for (const auto &category : m_Categories) {
        if (category.properties.empty()) {
            continue;
        }

        writeCommentLines(category.comment, "# ");
        out << category.name << " {" << std::endl << std::endl;

        for (const auto &property : category.properties) {
            writeCommentLines(property.comment, "\t# ");

            out << "\t" << static_cast<char>(PropertyTypeToChar(property.type)) << " " << property.name << " ";

            switch (property.type) {
            case CfgPropertyType::STRING:
                out << escapeString(property.GetString());
                break;
            case CfgPropertyType::BOOLEAN:
                out << property.GetBoolean();
                break;
            case CfgPropertyType::INTEGER:
                out << property.GetInteger();
                break;
            case CfgPropertyType::FLOAT:
                out << property.GetFloat();
                break;
            case CfgPropertyType::KEY:
                out << property.GetInteger();
                break;
            default:
                out << "0";
                break;
            }

            out << std::endl << std::endl;
        }

        out << "}" << std::endl << std::endl;
    }

    return out.str();
}

bool CfgFile::WriteToFile(const std::wstring &filePath) const {
    const_cast<CfgFile *>(this)->ClearError();

    std::string content = WriteToString();

    if (m_StrictUtf8 && !IsValidUtf8(content)) {
        SetError("Generated content contains invalid UTF-8");
        return false;
    }

    FILE *fp = _wfopen(filePath.c_str(), L"wb");
    if (!fp) {
        SetError("Cannot open file for writing: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    bool success = (fwrite(content.c_str(), sizeof(char), content.size(), fp) == content.size());
    fclose(fp);

    if (!success) {
        SetError("Failed to write to file: " + utils::Utf16ToUtf8(filePath));
    }

    return success;
}

bool CfgFile::HasCategory(const std::string &categoryName) const {
    return FindCategoryIndex(categoryName) != SIZE_MAX;
}

CfgFile::Category *CfgFile::GetCategory(const std::string &categoryName) {
    std::string normalized = NormalizeUtf8Name(categoryName);
    auto it = m_CategoryIndex.find(normalized);
    return (it != m_CategoryIndex.end() && it->second < m_Categories.size()) ? &m_Categories[it->second] : nullptr;
}

const CfgFile::Category *CfgFile::GetCategory(const std::string &categoryName) const {
    std::string normalized = NormalizeUtf8Name(categoryName);
    auto it = m_CategoryIndex.find(normalized);
    return (it != m_CategoryIndex.end() && it->second < m_Categories.size()) ? &m_Categories[it->second] : nullptr;
}

CfgFile::Category *CfgFile::AddCategory(const std::string &categoryName) {
    ClearError();

    if (!IsValidUtf8CategoryName(categoryName)) {
        SetError("Invalid category name: " + categoryName);
        return nullptr;
    }

    if (HasCategory(categoryName)) {
        return GetCategory(categoryName);
    }

    if (m_Categories.size() >= MAX_CATEGORIES) {
        SetError("Maximum number of categories exceeded");
        return nullptr;
    }

    m_Categories.emplace_back(categoryName);
    Category &category = m_Categories.back();
    category.caseSensitive = m_CaseSensitive;
    category.MarkPropertyIndexDirty();
    RebuildCategoryIndex();
    return &category;
}

bool CfgFile::RemoveCategory(const std::string &categoryName) {
    size_t index = FindCategoryIndex(categoryName);
    if (index == SIZE_MAX) {
        return false;
    }

    m_Categories.erase(m_Categories.begin() + index);
    RebuildCategoryIndex();
    return true;
}

std::vector<std::string> CfgFile::GetCategoryNames() const {
    std::vector<std::string> names;
    names.reserve(m_Categories.size());
    for (const auto &category : m_Categories) {
        names.push_back(category.name);
    }
    return names;
}

bool CfgFile::HasProperty(const std::string &categoryName, const std::string &propName) const {
    const Category *category = GetCategory(categoryName);
    return category ? category->HasProperty(propName) : false;
}

CfgFile::Property *CfgFile::GetProperty(const std::string &categoryName, const std::string &propName) {
    Category *category = GetCategory(categoryName);
    return category ? category->FindProperty(propName) : nullptr;
}

const CfgFile::Property *CfgFile::GetProperty(const std::string &categoryName, const std::string &propName) const {
    const Category *category = GetCategory(categoryName);
    return category ? category->FindProperty(propName) : nullptr;
}

bool CfgFile::SetStringProperty(const std::string &categoryName, const std::string &propName, const std::string &value) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    Property *prop = category->FindProperty(propName);
    if (!prop) {
        prop = category->AddProperty(propName, CfgPropertyType::STRING);
        if (!prop) return false;
    }

    prop->SetString(value);
    return true;
}

bool CfgFile::SetBooleanProperty(const std::string &categoryName, const std::string &propName, bool value) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    Property *prop = category->FindProperty(propName);
    if (!prop) {
        prop = category->AddProperty(propName, CfgPropertyType::BOOLEAN);
        if (!prop) return false;
    }

    prop->SetBoolean(value);
    return true;
}

bool CfgFile::SetIntegerProperty(const std::string &categoryName, const std::string &propName, int value) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    Property *prop = category->FindProperty(propName);
    if (!prop) {
        prop = category->AddProperty(propName, CfgPropertyType::INTEGER);
        if (!prop) return false;
    }

    prop->SetInteger(value);
    return true;
}

bool CfgFile::SetFloatProperty(const std::string &categoryName, const std::string &propName, float value) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    Property *prop = category->FindProperty(propName);
    if (!prop) {
        prop = category->AddProperty(propName, CfgPropertyType::FLOAT);
        if (!prop) return false;
    }

    prop->SetFloat(value);
    return true;
}

bool CfgFile::SetKeyProperty(const std::string &categoryName, const std::string &propName, int value) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    Property *prop = category->FindProperty(propName);
    if (!prop) {
        prop = category->AddProperty(propName, CfgPropertyType::KEY);
        if (!prop) return false;
    }

    prop->type = CfgPropertyType::KEY;
    prop->value = value;
    return true;
}

std::string CfgFile::GetStringProperty(const std::string &categoryName, const std::string &propName, const std::string &defaultValue) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->GetString() : defaultValue;
}

bool CfgFile::GetBooleanProperty(const std::string &categoryName, const std::string &propName, bool defaultValue) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->GetBoolean() : defaultValue;
}

int CfgFile::GetIntegerProperty(const std::string &categoryName, const std::string &propName, int defaultValue) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->GetInteger() : defaultValue;
}

float CfgFile::GetFloatProperty(const std::string &categoryName, const std::string &propName, float defaultValue) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->GetFloat() : defaultValue;
}

int CfgFile::GetKeyProperty(const std::string &categoryName, const std::string &propName, int defaultValue) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->GetInteger() : defaultValue;
}

std::string CfgFile::GetCategoryComment(const std::string &categoryName) const {
    const Category *category = GetCategory(categoryName);
    return category ? category->comment : "";
}

bool CfgFile::SetCategoryComment(const std::string &categoryName, const std::string &comment) {
    Category *category = GetCategory(categoryName);
    if (!category) {
        category = AddCategory(categoryName);
        if (!category) return false;
    }

    category->comment = comment;
    return true;
}

std::string CfgFile::GetPropertyComment(const std::string &categoryName, const std::string &propName) const {
    const Property *prop = GetProperty(categoryName, propName);
    return prop ? prop->comment : "";
}

bool CfgFile::SetPropertyComment(const std::string &categoryName, const std::string &propName, const std::string &comment) {
    Property *prop = GetProperty(categoryName, propName);
    if (!prop) return false;

    prop->comment = comment;
    return true;
}

std::string CfgFile::GetHeaderComment() const {
    if (m_LeadingComments.empty()) {
        return "";
    }

    std::string result;
    for (const auto &line : m_LeadingComments) {
        if (!result.empty()) {
            result += "\n";
        }
        result += line;
    }
    return result;
}

bool CfgFile::SetHeaderComment(const std::string &comment) {
    m_LeadingComments.clear();

    if (comment.empty()) {
        return true;
    }

    // Split comment into lines and format as comments
    std::istringstream iss(comment);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove trailing whitespace
        while (!line.empty() && std::isspace(line.back())) {
            line.pop_back();
        }

        // Add comment prefix if not present
        if (line.empty()) {
            m_LeadingComments.push_back("#");
        } else if (line.front() != '#') {
            m_LeadingComments.push_back("# " + line);
        } else {
            m_LeadingComments.push_back(line);
        }
    }

    return true;
}

void CfgFile::ClearHeaderComment() {
    m_LeadingComments.clear();
}

void CfgFile::SetCaseSensitive(bool caseSensitive) {
    if (m_CaseSensitive != caseSensitive) {
        m_CaseSensitive = caseSensitive;
        for (auto &category : m_Categories) {
            category.caseSensitive = caseSensitive;
            category.MarkPropertyIndexDirty();
        }
        RebuildCategoryIndex();
    }
}

void CfgFile::Clear() {
    m_Categories.clear();
    m_CategoryIndex.clear();
    m_LeadingComments.clear();
    ClearError();
}

bool CfgFile::IsEmpty() const {
    for (const auto &category : m_Categories) {
        if (!category.properties.empty() || !category.comment.empty()) {
            return false;
        }
    }
    return true;
}

bool CfgFile::IsCommentLine(const std::string &line) const {
    return !line.empty() && line[0] == '#';
}

bool CfgFile::IsEmptyLine(const std::string &line) const {
    return line.empty();
}

bool CfgFile::IsCategoryHeader(const std::string &line, std::string &categoryName) const {
    std::string trimmed = TrimUtf8String(line);
    if (trimmed.empty()) return false;

    // Simple category header detection - just the category name
    categoryName = trimmed;
    return !categoryName.empty();
}

CfgPropertyType CfgFile::CharToPropertyType(wchar_t c) const {
    switch (c) {
    case L'S': return CfgPropertyType::STRING;
    case L'B': return CfgPropertyType::BOOLEAN;
    case L'I': return CfgPropertyType::INTEGER;
    case L'F': return CfgPropertyType::FLOAT;
    case L'K': return CfgPropertyType::KEY;
    default: return CfgPropertyType::NONE;
    }
}

wchar_t CfgFile::PropertyTypeToChar(CfgPropertyType type) const {
    switch (type) {
    case CfgPropertyType::STRING: return L'S';
    case CfgPropertyType::BOOLEAN: return L'B';
    case CfgPropertyType::INTEGER: return L'I';
    case CfgPropertyType::FLOAT: return L'F';
    case CfgPropertyType::KEY: return L'K';
    default: return L'I';
    }
}

bool CfgFile::IsValidUtf8CategoryName(const std::string &name) const {
    if (name.empty()) return false;

    if (m_StrictUtf8 && !IsValidUtf8(name)) return false;

    size_t codepoints = GetUtf8Length(name);
    if (codepoints > MAX_NAME_CODEPOINTS) return false;

    return true;
}

bool CfgFile::IsValidUtf8PropertyName(const std::string &name) const {
    if (name.empty()) return false;

    if (m_StrictUtf8 && !IsValidUtf8(name)) return false;

    size_t codepoints = GetUtf8Length(name);
    if (codepoints > MAX_NAME_CODEPOINTS) return false;

    return true;
}

size_t CfgFile::FindCategoryIndex(const std::string &categoryName) const {
    std::string normalized = NormalizeUtf8Name(categoryName);
    auto it = m_CategoryIndex.find(normalized);
    return (it != m_CategoryIndex.end()) ? it->second : SIZE_MAX;
}

void CfgFile::RebuildCategoryIndex() const {
    m_CategoryIndex.clear();
    m_CategoryIndex.reserve(m_Categories.size());

    const auto normalizeName = [this](const std::string &name) { return NormalizeUtf8Name(name); };

    for (size_t i = 0; i < m_Categories.size(); ++i) {
        std::string normalized = normalizeName(m_Categories[i].name);
        m_CategoryIndex[normalized] = i;
        m_Categories[i].RebuildPropertyIndex(normalizeName);
    }
}
