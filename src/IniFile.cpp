#include "IniFile.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <memory>
#include <utility>

#include <utf8.h>

#include "PathUtils.h"
#include "StringUtils.h"

// Section implementation
void IniFile::Section::RebuildKeyIndex(const std::function<std::string(const std::string &)> &normalizer) const {
    if (!m_KeyIndexDirty) {
        return;
    }

    m_KeyIndex.clear();
    m_KeyIndex.reserve(entries.size());

    if (!normalizer) {
        m_KeyIndexDirty = false;
        return;
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        if (!entry.isComment && !entry.isEmpty && !entry.key.empty()) {
            std::string normalizedKey = normalizer(entry.key);
            if (!normalizedKey.empty()) {
                m_KeyIndex[std::move(normalizedKey)] = i;
            }
        }
    }

    m_KeyIndexDirty = false;
}

IniFile::KeyValue *IniFile::Section::FindKey(const std::string &normalizedKey) const {
    auto it = m_KeyIndex.find(normalizedKey);
    return (it != m_KeyIndex.end() && it->second < entries.size()) ?
           const_cast<KeyValue*>(&entries[it->second]) : nullptr;
}

IniFile::IniFile() {
    // Default section insertion logic - theme sections go first, overrides go last
    m_SectionInsertLogic = [](const std::vector<Section> &sections, const std::string &sectionName) -> size_t {
        std::string lower = utils::ToLower(sectionName);

        // Theme section goes first
        if (lower == "theme") {
            return 0;
        }

        // Overrides section goes last
        if (lower == "overrides") {
            return sections.size();
        }

        // Other sections go after theme but before overrides
        for (size_t i = 0; i < sections.size(); ++i) {
            std::string existingLower = utils::ToLower(sections[i].name);
            if (existingLower == "overrides") {
                return i;
            }
        }

        return sections.size();
    };
}

IniFile::~IniFile() {
    Clear();
}

bool IniFile::IsValidUtf8(const std::string &str) const {
    if (str.empty()) return true;
    return utf8valid((const utf8_int8_t *) str.c_str()) == nullptr;  // utf8valid returns NULL for valid strings
}

size_t IniFile::GetUtf8Length(const std::string &str) const {
    if (str.empty()) return 0;

    // Validate UTF-8 first
    if (!IsValidUtf8(str)) return 0;

    return utf8len((const utf8_int8_t *) str.c_str());
}

std::string IniFile::TrimUtf8String(const std::string &str) const {
    if (str.empty()) return str;


    // Validate UTF-8
    if (m_StrictUtf8 && !IsValidUtf8(str)) {
        return str; // Return as-is if invalid and strict mode is on
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

    // Trim trailing whitespace - simpler approach
    const char *trimEnd = end;
    while (trimEnd > start) {
        // Move back one byte to find the start of the last UTF-8 character
        const char *prev = trimEnd - 1;

        // Find the start of the UTF-8 character
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
    
    std::string result(start, trimEnd - start);
    return result;
}

std::string IniFile::ToLowerUtf8(const std::string &str) const {
    if (str.empty()) return str;

    if (m_StrictUtf8 && !IsValidUtf8(str)) {
        SetError("Invalid UTF-8 sequence in string");
        return str;
    }

    // Use utf8lwr - it modifies in place, so we need a copy
    utf8_int8_t *strCopy = utf8dup((const utf8_int8_t *) str.c_str());
    if (!strCopy) return str;

    utf8lwr((utf8_int8_t *) strCopy);
    std::string result((char *) strCopy);
    free(strCopy);
    return result;
}

std::string IniFile::NormalizeUtf8Key(const std::string &key) const {
    std::string trimmed = TrimUtf8String(key);
    if (m_CaseSensitive) {
        return trimmed;
    } else {
        return ToLowerUtf8(trimmed);
    }
}

std::string IniFile::NormalizeUtf8SectionName(const std::string &name) const {
    std::string trimmed = TrimUtf8String(name);
    if (m_CaseSensitive) {
        return trimmed;
    } else {
        return ToLowerUtf8(trimmed);
    }
}

int IniFile::CompareUtf8(const std::string &a, const std::string &b, bool caseSensitive) const {
    if (caseSensitive) {
        return utf8cmp((const utf8_int8_t *) a.c_str(), (const utf8_int8_t *) b.c_str());
    } else {
        return utf8casecmp((const utf8_int8_t *) a.c_str(), (const utf8_int8_t *) b.c_str());
    }
}

bool IniFile::IsUtf8Whitespace(const char *str, size_t *advance) const {
    if (!str || !*str) return false;

    // Get the UTF-8 codepoint
    utf8_int32_t codepoint;
    const char *next = (const char *) utf8codepoint((const utf8_int8_t *) str, &codepoint);
    if (!next) return false;

    if (advance) {
        *advance = next - str;
    }

    // Check common whitespace characters
    return (codepoint == 0x20) ||                       // Space
        (codepoint == 0x09) ||                          // Tab
        (codepoint == 0x0A) ||                          // Line Feed
        (codepoint == 0x0D) ||                          // Carriage Return
        (codepoint == 0x0B) ||                          // Vertical Tab
        (codepoint == 0x0C) ||                          // Form Feed
        (codepoint == 0xA0) ||                          // Non-breaking space
        (codepoint >= 0x2000 && codepoint <= 0x200A) || // Various Unicode spaces
        (codepoint == 0x2028) ||                        // Line separator
        (codepoint == 0x2029) ||                        // Paragraph separator
        (codepoint == 0x202F) ||                        // Narrow no-break space
        (codepoint == 0x205F) ||                        // Medium mathematical space
        (codepoint == 0x3000);                          // Ideographic space
}

bool IniFile::ParseFromString(const std::string &content) {
    ClearError();
    Clear();

    if (content.empty()) {
        return true; // Empty content is valid
    }

    // Validate UTF-8 first if strict mode is enabled
    if (m_StrictUtf8 && !IsValidUtf8(content)) {
        SetError("Invalid UTF-8 content");
        return false;
    }

    // Normalize line endings: convert CR to LF and CRLF to LF
    std::string normalizedContent;
    normalizedContent.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        char ch = content[i];
        if (ch == '\r') {
            // Check if it's CRLF (CR followed by LF)
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                // Skip the CR, keep the LF
                continue;
            } else {
                // Convert standalone CR to LF
                normalizedContent += '\n';
            }
        } else {
            normalizedContent += ch;
        }
    }

    if (normalizedContent.size() >= 3 &&
        static_cast<unsigned char>(normalizedContent[0]) == 0xEF &&
        static_cast<unsigned char>(normalizedContent[1]) == 0xBB &&
        static_cast<unsigned char>(normalizedContent[2]) == 0xBF) {
        normalizedContent.erase(0, 3);
    }
    std::istringstream stream(normalizedContent);
    std::string line;
    Section *currentSection = nullptr;
    bool inLeadingComments = true;
    size_t lineNumber = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;

        // Check UTF-8 validity for each line
        if (m_StrictUtf8 && !IsValidUtf8(line)) {
            SetError("Invalid UTF-8 on line " + std::to_string(lineNumber));
            return false;
        }

        // Check UTF-8 codepoint length limit
        size_t codepoints = GetUtf8Length(line);
        if (codepoints > MAX_LINE_CODEPOINTS) {
            SetError("Line " + std::to_string(lineNumber) + " exceeds maximum codepoint length (" +
                std::to_string(MAX_LINE_CODEPOINTS) + ")");
            return false;
        }

        std::string trimmed = TrimUtf8String(line);

        // Check for section header
        std::string sectionName;
        if (IsSectionHeader(trimmed, sectionName)) {
            if (!IsValidUtf8SectionName(sectionName)) {
                SetError("Invalid section name at line " + std::to_string(lineNumber) + ": " + sectionName);
                return false;
            }

            inLeadingComments = false;

            // Check section count limit
            if (m_Sections.size() >= MAX_SECTIONS) {
                SetError("Maximum number of sections exceeded");
                return false;
            }

            // Create new section
            m_Sections.emplace_back(sectionName);
            currentSection = &m_Sections.back();
            currentSection->headerLine = line;
            currentSection->lineNumber = lineNumber;
            continue;
        }

        // Handle leading comments (before first section)
        if (inLeadingComments && (IsCommentLine(trimmed) || IsEmptyLine(trimmed))) {
            m_LeadingComments.push_back(line);
            continue;
        }

        // Non-section content found
        inLeadingComments = false;

        // If no current section and we have non-comment content, create global section
        if (!currentSection && !IsCommentLine(trimmed) && !IsEmptyLine(trimmed)) {
            m_Sections.emplace_back(""); // Global section with empty name
            currentSection = &m_Sections.back();
            currentSection->headerLine = "";
            currentSection->lineNumber = lineNumber;
        }

        // If still no current section (only comments/empty lines), skip
        if (!currentSection) {
            continue;
        }

        // Check keys per section limit
        if (currentSection->entries.size() >= MAX_KEYS_PER_SECTION) {
            SetError("Maximum number of keys per section exceeded");
            return false;
        }

        // Create entry for current line
        KeyValue entry;
        entry.originalLine = line;
        entry.lineNumber = lineNumber;
        entry.isComment = IsCommentLine(trimmed);
        entry.isEmpty = IsEmptyLine(trimmed);

        // Parse key-value pairs with comment support
        if (!entry.isComment && !entry.isEmpty) {
            if (ParseKeyValueWithComment(trimmed, entry.key, entry.value, entry.inlineComment)) {
                if (!IsValidUtf8Key(entry.key)) {
                    SetError("Invalid key at line " + std::to_string(lineNumber) + ": " + entry.key);
                    return false;
                }
            } else {
                // Malformed key-value line, treat as comment but warn
                entry.isComment = true;
                entry.key = "";
                entry.value = "";
            }
        }

        currentSection->entries.push_back(std::move(entry));
    }

    // Build indices for fast lookup
    RebuildSectionIndex();
    return true;
}

bool IniFile::ParseFromFile(const std::wstring &filePath) {
    ClearError();

    if (!utils::FileExistsW(filePath)) {
        SetError("File does not exist: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    std::wstring contentW = utils::ReadTextFileW(filePath);
    if (contentW.empty()) {
        return ParseFromString(""); // Empty file is valid
    }

    std::string content = utils::Utf16ToUtf8(contentW);
    return ParseFromString(content);
}

std::string IniFile::WriteToString() const {
    std::string result;
    result.reserve(8192); // Pre-allocate reasonable size

    // Add leading comments
    for (const auto &comment : m_LeadingComments) {
        result += comment + "\n";
    }

    // Add sections
    bool needNewlineBeforeSection = !m_LeadingComments.empty();

    for (size_t i = 0; i < m_Sections.size(); ++i) {
        const Section &section = m_Sections[i];

        // Add spacing before section header (except for global section)
        if (!section.name.empty()) {
            if (needNewlineBeforeSection) {
                // Check if we already have proper spacing (avoid duplicate empty lines)
                if (!result.empty() && result.back() != '\n') {
                    result += "\n";
                } else if (result.length() >= 2 && result.substr(result.length() - 2) != "\n\n") {
                    result += "\n";
                }
            }
            result += section.headerLine + "\n";
            needNewlineBeforeSection = false;
        }

        // Add section entries
        bool lastWasEmpty = false;
        for (const auto &entry : section.entries) {
            // Add preceding comment if present
            if (!entry.precedingComment.empty()) {
                // Ensure we have proper spacing before the comment
                if (!lastWasEmpty && !result.empty() && result.back() != '\n') {
                    result += "\n";
                }
                result += entry.precedingComment + "\n";
                lastWasEmpty = false;
            }

            // Avoid consecutive empty lines
            if (entry.isEmpty) {
                if (!lastWasEmpty) {
                    result += "\n";
                    lastWasEmpty = true;
                }
            } else {
                result += entry.originalLine + "\n";
                lastWasEmpty = false;
            }
        }

        // Only add spacing before next section if current section had content that ends without an empty line
        if (i < m_Sections.size() - 1 && !section.entries.empty()) {
             // Check if the last entry was not an empty line
             const auto &lastEntry = section.entries.back();
             needNewlineBeforeSection = !lastEntry.isEmpty;
         }
    }

    return result;
}

bool IniFile::WriteToFile(const std::wstring &filePath) const {
    const_cast<IniFile*>(this)->ClearError();

    std::string content = WriteToString();

    // Validate UTF-8 before writing
    if (m_StrictUtf8 && !IsValidUtf8(content)) {
        SetError("Generated content contains invalid UTF-8");
        return false;
    }

    std::wstring contentW = utils::Utf8ToUtf16(content);

    if (!utils::WriteTextFileW(filePath, contentW)) {
        SetError("Failed to write file: " + utils::Utf16ToUtf8(filePath));
        return false;
    }

    return true;
}

// Section operations
bool IniFile::HasSection(const std::string &sectionName) const {
    return FindSectionIndex(sectionName) != SIZE_MAX;
}

IniFile::Section *IniFile::GetSection(const std::string &sectionName) {
    std::string normalized = NormalizeUtf8SectionName(sectionName);
    auto it = m_SectionIndex.find(normalized);
    return (it != m_SectionIndex.end() && it->second < m_Sections.size()) ? &m_Sections[it->second] : nullptr;
}

const IniFile::Section *IniFile::GetSection(const std::string &sectionName) const {
    std::string normalized = NormalizeUtf8SectionName(sectionName);
    auto it = m_SectionIndex.find(normalized);
    return (it != m_SectionIndex.end() && it->second < m_Sections.size()) ? &m_Sections[it->second] : nullptr;
}

IniFile::Section *IniFile::AddSection(const std::string &sectionName) {
    ClearError();

    if (!IsValidUtf8SectionName(sectionName)) {
        SetError("Invalid section name: " + sectionName);
        return nullptr;
    }

    if (HasSection(sectionName)) {
        return GetSection(sectionName);
    }

    if (m_Sections.size() >= MAX_SECTIONS) {
        SetError("Maximum number of sections exceeded");
        return nullptr;
    }

    // Determine insertion position
    size_t insertPos = GetDefaultSectionInsertPosition(sectionName);

    // Create section
    Section newSection(sectionName);

    // Insert at determined position
    auto insertIter = m_Sections.begin() + insertPos;
    auto result = m_Sections.insert(insertIter, std::move(newSection));

    // Rebuild section index since we inserted
    RebuildSectionIndex();

    return &(*result);
}

bool IniFile::RemoveSection(const std::string &sectionName) {
    size_t index = FindSectionIndex(sectionName);
    if (index == SIZE_MAX) {
        return false;
    }

    // Before removing the section, check if we need to clean up trailing empty lines
    // from the previous section to avoid leaving extra blank lines
    if (index > 0) {
        Section &prevSection = m_Sections[index - 1];
        // Remove trailing empty entries from the previous section
        while (!prevSection.entries.empty() && prevSection.entries.back().isEmpty) {
            prevSection.entries.pop_back();
        }
        // Mark key index dirty after trimming entries
        prevSection.MarkKeyIndexDirty();
    }

    m_Sections.erase(m_Sections.begin() + index);
    RebuildSectionIndex();
    return true;
}

std::vector<std::string> IniFile::GetSectionNames() const {
    std::vector<std::string> names;
    names.reserve(m_Sections.size());
    for (const auto &section : m_Sections) {
        names.push_back(section.name);
    }
    return names;
}

// Key-value operations
bool IniFile::HasKey(const std::string &sectionName, const std::string &key) const {
    const Section *section = GetSection(sectionName);
    if (!section) return false;

    return FindKeyInSection(section, key) != nullptr;
}

std::string IniFile::GetValue(const std::string &sectionName, const std::string &key,
                              const std::string &defaultValue) const {
    const Section *section = GetSection(sectionName);
    if (!section) return defaultValue;

    const KeyValue *entry = FindKeyInSection(section, key);
    return entry ? entry->value : defaultValue;
}

bool IniFile::SetValue(const std::string &sectionName, const std::string &key, const std::string &value) {
    ClearError();

    if (!IsValidUtf8Key(key)) {
        SetError("Invalid key: " + key);
        return false;
    }

    // Validate UTF-8 in value if strict mode is enabled
    if (m_StrictUtf8 && !IsValidUtf8(value)) {
        SetError("Invalid UTF-8 in value for key: " + key);
        return false;
    }

    Section *section = GetSection(sectionName);
    if (!section) {
        section = AddSection(sectionName);
        if (!section) return false;
    }

    // Try to find existing key
    KeyValue *entry = FindKeyInSection(section, key);
    if (entry) {
        entry->value = value;
        // Preserve the inline comment when updating the value
        entry->originalLine = FormatKeyValueWithComment(entry->key, value, entry->inlineComment);
        return true;
    }

    // Check keys per section limit
    if (section->entries.size() >= MAX_KEYS_PER_SECTION) {
        SetError("Maximum number of keys per section exceeded");
        return false;
    }

    // Find the best insertion position - before any trailing empty lines or comments
    size_t insertPos = section->entries.size();

    // Scan backwards from the end to find the last non-empty, non-comment entry
    for (size_t i = section->entries.size(); i > 0; --i) {
        const KeyValue &existing = section->entries[i - 1];
        if (existing.isEmpty || existing.isComment) {
            insertPos = i - 1;
        } else {
            // Found a real key-value entry, insert after it
            break;
        }
    }

    // Insert the new entry at the calculated position
    section->entries.emplace(section->entries.begin() + insertPos, key, value);
    section->MarkKeyIndexDirty();

    return true;
}

bool IniFile::SetValueWithComment(const std::string &sectionName, const std::string &key,
                                 const std::string &value, const std::string &inlineComment) {
    if (!SetValue(sectionName, key, value)) {
        return false;
    }

    // Now set the comment
    return SetInlineComment(sectionName, key, inlineComment);
}

std::string IniFile::GetInlineComment(const std::string &sectionName, const std::string &key) const {
    const Section *section = GetSection(sectionName);
    if (!section) return "";

    const KeyValue *entry = FindKeyInSection(section, key);
    return entry ? entry->inlineComment : "";
}

bool IniFile::SetInlineComment(const std::string &sectionName, const std::string &key, const std::string &comment) {
    Section *section = GetSection(sectionName);
    if (!section) return false;

    KeyValue *entry = FindKeyInSection(section, key);
    if (!entry) return false;

    // Normalize the comment format - ensure it starts with # if not empty
    std::string normalizedComment = comment;
    if (!normalizedComment.empty() && normalizedComment[0] != '#' && normalizedComment[0] != ';') {
        normalizedComment = "# " + normalizedComment;
    }

    entry->inlineComment = normalizedComment;
    // Update the original line to reflect the comment
    entry->originalLine = FormatKeyValueWithComment(entry->key, entry->value, normalizedComment);
    return true;
}

std::string IniFile::GetPrecedingComment(const std::string &sectionName, const std::string &key) const {
    const Section *section = GetSection(sectionName);
    if (!section) return "";

    const KeyValue *entry = FindKeyInSection(section, key);
    return entry ? entry->precedingComment : "";
}

bool IniFile::SetPrecedingComment(const std::string &sectionName, const std::string &key, const std::string &comment) {
    Section *section = GetSection(sectionName);
    if (!section) return false;

    KeyValue *entry = FindKeyInSection(section, key);
    if (!entry) return false;

    entry->precedingComment = comment;
    return true;
}

bool IniFile::RemoveKey(const std::string &sectionName, const std::string &key) {
    Section *section = GetSection(sectionName);
    if (!section) return false;

    std::string normalizedKey = NormalizeUtf8Key(key);

    auto it = std::remove_if(section->entries.begin(), section->entries.end(),
                             [this, &normalizedKey](const KeyValue &entry) {
                                 return !entry.isComment && !entry.isEmpty &&
                                     NormalizeUtf8Key(entry.key) == normalizedKey;
                             });

    bool removed = (it != section->entries.end());
    section->entries.erase(it, section->entries.end());

    if (removed) {
        section->MarkKeyIndexDirty();
    }

    return removed;
}

bool IniFile::ApplyMutations(const std::string &sectionName,
                             const std::vector<Mutation> &mutations,
                             KeyMatcher matcher,
                             KeyCanonicalizer canonicalizer) {
    ClearError();

    Section *section = GetSection(sectionName);
    if (!section) {
        section = AddSection(sectionName);
        if (!section) return false;
    }

    // Separate mutations into operations
    std::vector<std::pair<size_t, const Mutation *>> removeOps;
    std::vector<std::pair<size_t, const Mutation *>> setOps;
    std::vector<const Mutation *> addOps;

    // Validate all mutations first
    for (const Mutation &mut : mutations) {
        if (!IsValidUtf8Key(mut.key)) {
            SetError("Invalid UTF-8 key in mutation: " + mut.key);
            return false;
        }
        if (m_StrictUtf8 && !mut.remove && !IsValidUtf8(mut.value)) {
            SetError("Invalid UTF-8 value in mutation for key: " + mut.key);
            return false;
        }
    }

    // Process existing entries
    for (size_t i = 0; i < mutations.size(); ++i) {
        const Mutation &mut = mutations[i];
        std::string targetKey = canonicalizer ? canonicalizer(mut.key) : mut.key;
        bool found = false;

        // Find matching entry in section
        for (size_t j = 0; j < section->entries.size(); ++j) {
            KeyValue &entry = section->entries[j];
            if (entry.isComment || entry.isEmpty) continue;

            bool keyMatches = false;
            if (matcher) {
                keyMatches = matcher(entry.key, targetKey);
            } else {
                keyMatches = (CompareUtf8(NormalizeUtf8Key(entry.key), NormalizeUtf8Key(targetKey), m_CaseSensitive) ==
                    0);
            }

            if (keyMatches) {
                if (mut.remove) {
                    removeOps.emplace_back(j, &mut);
                } else {
                    setOps.emplace_back(j, &mut);
                }
                found = true;
                break;
            }
        }

        // If not found and not a remove operation, add to new entries
        if (!found && !mut.remove) {
            addOps.push_back(&mut);
        }
    }

    // Apply operations in safe order

    // 1. Apply set operations
    for (const auto &op : setOps) {
        size_t index = op.first;
        const Mutation *mut = op.second;

        KeyValue &entry = section->entries[index];
        entry.key = mut->key;
        entry.value = mut->value;
        entry.originalLine = FormatKeyValueWithComment(entry.key, entry.value, entry.inlineComment);
    }

    // 2. Apply remove operations (reverse order)
    std::sort(removeOps.begin(), removeOps.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    for (const auto &op : removeOps) {
        section->entries.erase(section->entries.begin() + op.first);
    }

    // 3. Add new entries
    for (const Mutation *mut : addOps) {
        if (section->entries.size() >= MAX_KEYS_PER_SECTION) {
            SetError("Maximum number of keys per section exceeded");
            return false;
        }

        section->entries.emplace_back(mut->key, mut->value);
    }

    // Mark the key index dirty since we modified entries
    section->MarkKeyIndexDirty();

    return true;
}

void IniFile::SetSectionInsertionLogic(SectionInsertLogic logic) {
    m_SectionInsertLogic = std::move(logic);
}

void IniFile::SetCaseSensitive(bool caseSensitive) {
    if (m_CaseSensitive != caseSensitive) {
        m_CaseSensitive = caseSensitive;
        for (auto &section : m_Sections) {
            section.MarkKeyIndexDirty();
        }
        // Rebuild all indices since case sensitivity changed
        RebuildSectionIndex();
    }
}

void IniFile::Clear() {
    m_Sections.clear();
    m_SectionIndex.clear();
    m_LeadingComments.clear();
    ClearError();
}

bool IniFile::IsEmpty() const {
    return m_Sections.empty() && m_LeadingComments.empty();
}

// Private helper methods
bool IniFile::IsCommentLine(const std::string &line) const {
    return !line.empty() && (line[0] == '#' || line[0] == ';');
}

bool IniFile::IsEmptyLine(const std::string &line) const {
    return line.empty();
}

bool IniFile::IsSectionHeader(const std::string &line, std::string &sectionName) const {
    if (line.size() < 3 || line.front() != '[' || line.back() != ']') {
        return false;
    }

    sectionName = TrimUtf8String(line.substr(1, line.size() - 2));
    return true; // Allow empty section names for global section
}

bool IniFile::ParseKeyValue(const std::string &line, std::string &key, std::string &value) const {
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
        return false;
    }

    key = TrimUtf8String(line.substr(0, eq));
    value = TrimUtf8String(line.substr(eq + 1));

    return !key.empty();
}

bool IniFile::ParseKeyValueWithComment(const std::string &line, std::string &key, std::string &value, std::string &comment) const {
    size_t eq = line.find('=');
    if (eq == std::string::npos) {
        return false;
    }

    key = TrimUtf8String(line.substr(0, eq));
    std::string valueAndComment = line.substr(eq + 1);

    bool inQuotes = false;
    size_t commentPos = std::string::npos;
    for (size_t i = 0; i < valueAndComment.size(); ++i) {
        char ch = valueAndComment[i];

        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (inQuotes || (ch != '#' && ch != ';')) {
            continue;
        }

        size_t prevIdx = i;
        while (prevIdx > 0 && (valueAndComment[prevIdx - 1] == ' ' || valueAndComment[prevIdx - 1] == '\t')) {
            --prevIdx;
        }
        char prevNonWhitespace = (prevIdx > 0) ? valueAndComment[prevIdx - 1] : '\0';

        size_t nextIdx = i + 1;
        while (nextIdx < valueAndComment.size() && (valueAndComment[nextIdx] == ' ' || valueAndComment[nextIdx] == '\t')) {
            ++nextIdx;
        }
        char nextNonWhitespace = (nextIdx < valueAndComment.size()) ? valueAndComment[nextIdx] : '\0';

        bool immediateWhitespace = (i == 0) || valueAndComment[i - 1] == ' ' || valueAndComment[i - 1] == '\t';

        if (ch == '#') {
            if (!immediateWhitespace) {
                continue;
            }

            size_t hexDigits = 0;
            for (size_t j = i + 1; j < valueAndComment.size() && hexDigits < 8; ++j) {
                char c = valueAndComment[j];
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                    ++hexDigits;
                } else {
                    break;
                }
            }

            if (hexDigits == 3 || hexDigits == 4 || hexDigits == 6 || hexDigits == 8) {
                size_t afterHex = i + 1 + hexDigits;
                while (afterHex < valueAndComment.size() && (valueAndComment[afterHex] == ' ' || valueAndComment[afterHex] == '\t')) {
                    ++afterHex;
                }
                if (afterHex >= valueAndComment.size()) {
                    continue;
                }
                commentPos = afterHex;
                break;
            }

            commentPos = i;
            break;
        } else {
            bool prevIsDigit = std::isdigit(static_cast<unsigned char>(prevNonWhitespace));
            bool nextIsDigit = std::isdigit(static_cast<unsigned char>(nextNonWhitespace));
            bool nextIsEnd = (nextIdx >= valueAndComment.size());
            bool prevIsSemicolon = (prevNonWhitespace == ';');

            bool digitAhead = false;
            for (size_t j = i + 1; j < valueAndComment.size(); ++j) {
                char c = valueAndComment[j];
                if (c == ' ' || c == '\t') {
                    continue;
                }
                if (c == ';') {
                    continue;
                }
                digitAhead = std::isdigit(static_cast<unsigned char>(c));
                break;
            }

            if (prevIsDigit && nextIsDigit) {
                continue;
            }

            if (prevIsDigit && nextIsEnd) {
                continue;
            }

            if (prevIsDigit && nextNonWhitespace == ';' && digitAhead) {
                continue;
            }

            if (prevIsSemicolon) {
                commentPos = i;
                break;
            }

            bool prevIsAlpha = std::isalpha(static_cast<unsigned char>(prevNonWhitespace));

            if (!immediateWhitespace) {
                if (prevIsAlpha && nextIsDigit) {
                    commentPos = i;
                    break;
                }
                continue;
            }

            commentPos = i;
            break;
        }
    }

    if (commentPos != std::string::npos) {
        value = TrimUtf8String(valueAndComment.substr(0, commentPos));
        comment = TrimUtf8String(valueAndComment.substr(commentPos));
    } else {
        value = TrimUtf8String(valueAndComment);
        comment.clear();
    }

    return !key.empty();
}

std::string IniFile::ExtractInlineComment(const std::string &line) const {
    std::string key, value, comment;
    if (ParseKeyValueWithComment(line, key, value, comment)) {
        return comment;
    }
    return "";
}

std::string IniFile::StripInlineComment(const std::string &line) const {
    std::string key, value, comment;
    if (ParseKeyValueWithComment(line, key, value, comment)) {
        return FormatKeyValueWithComment(key, value, "");
    }
    return line;
}

std::string IniFile::FormatKeyValueWithComment(const std::string &key, const std::string &value, const std::string &comment) const {
    std::string result = key + " = " + value;
    if (!comment.empty()) {
        // Ensure comment starts with a comment character
        std::string trimmedComment = TrimUtf8String(comment);
        if (!trimmedComment.empty() && trimmedComment[0] != '#' && trimmedComment[0] != ';') {
            trimmedComment = "# " + trimmedComment;
        }
        result += "  " + trimmedComment;
    }
    return result;
}

bool IniFile::IsValidUtf8SectionName(const std::string &name) const {
    if (name.empty()) {
        return true; // Global section
    }

    // Validate UTF-8
    if (m_StrictUtf8 && !IsValidUtf8(name)) {
        return false;
    }

    // Check UTF-8 codepoint length
    size_t codepoints = GetUtf8Length(name);
    if (codepoints > MAX_SECTION_CODEPOINTS) {
        return false;
    }

    // Check for invalid characters
    const char *ptr = name.c_str();
    while (*ptr) {
        utf8_int32_t codepoint;
        const char *next = (const char *) utf8codepoint((const utf8_int8_t *) ptr, &codepoint);
        if (!next) break;

        // Check for forbidden characters
        if (codepoint == '[' || codepoint == ']' || codepoint == '\n' || codepoint == '\r') {
            return false;
        }

        ptr = next;
    }

    return true;
}

bool IniFile::IsValidUtf8Key(const std::string &key) const {
    if (key.empty()) {
        return false;
    }

    // Validate UTF-8
    if (m_StrictUtf8 && !IsValidUtf8(key)) {
        return false;
    }

    // Check UTF-8 codepoint length
    size_t codepoints = GetUtf8Length(key);
    if (codepoints > MAX_KEY_CODEPOINTS) {
        return false;
    }

    // Check for invalid characters
    const char *ptr = key.c_str();
    while (*ptr) {
        utf8_int32_t codepoint;
        const char *next = (const char *) utf8codepoint((const utf8_int8_t *) ptr, &codepoint);
        if (!next) break;

        // Check for forbidden characters
        if (codepoint == '=' || codepoint == '\n' || codepoint == '\r') {
            return false;
        }

        ptr = next;
    }

    return true;
}

size_t IniFile::FindSectionIndex(const std::string &sectionName) const {
    std::string normalized = NormalizeUtf8SectionName(sectionName);
    auto it = m_SectionIndex.find(normalized);
    return (it != m_SectionIndex.end()) ? it->second : SIZE_MAX;
}

size_t IniFile::GetDefaultSectionInsertPosition(const std::string &sectionName) const {
    if (m_SectionInsertLogic) {
        size_t pos = m_SectionInsertLogic(m_Sections, sectionName);
        return std::min(pos, m_Sections.size());
    }

    return m_Sections.size(); // Default: append at end
}

void IniFile::RebuildSectionIndex() const {
    m_SectionIndex.clear();
    m_SectionIndex.reserve(m_Sections.size());
    const auto normalizeSection = [this](const std::string &name) { return NormalizeUtf8SectionName(name); };
    const auto normalizeKey = [this](const std::string &key) { return NormalizeUtf8Key(key); };

    for (size_t i = 0; i < m_Sections.size(); ++i) {
        std::string normalized = normalizeSection(m_Sections[i].name);
        // Later sections override earlier ones for lookups while preserving duplicates in m_Sections
        m_SectionIndex[normalized] = i;
        m_Sections[i].RebuildKeyIndex(normalizeKey);
    }
}

IniFile::KeyValue *IniFile::FindKeyInSection(Section *section, const std::string &key) {
    if (!section) return nullptr;

    section->RebuildKeyIndex([this](const std::string &candidate) { return NormalizeUtf8Key(candidate); });
    std::string normalizedKey = NormalizeUtf8Key(key);
    return section->FindKey(normalizedKey);
}

const IniFile::KeyValue *IniFile::FindKeyInSection(const Section *section, const std::string &key) const {
    if (!section) return nullptr;

    section->RebuildKeyIndex([this](const std::string &candidate) { return NormalizeUtf8Key(candidate); });
    std::string normalizedKey = NormalizeUtf8Key(key);
    return section->FindKey(normalizedKey);
}
