#include "JsonUtils.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <utility>

#include "StringUtils.h"

namespace utils {
    namespace {
        yyjson_mut_val *CreateCopiedKey(yyjson_mut_doc *doc, std::string_view key) {
            if (!doc) {
                return nullptr;
            }
            return yyjson_mut_strncpy(doc, key.data(), key.size());
        }
    } // namespace

    JsonDocument::JsonDocument(yyjson_doc *doc) noexcept : m_Doc(doc) {}

    JsonDocument::~JsonDocument() {
        if (m_Doc) {
            yyjson_doc_free(m_Doc);
        }
    }

    JsonDocument::JsonDocument(JsonDocument &&other) noexcept : m_Doc(other.m_Doc) {
        other.m_Doc = nullptr;
    }

    JsonDocument &JsonDocument::operator=(JsonDocument &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (m_Doc) {
            yyjson_doc_free(m_Doc);
        }
        m_Doc = other.m_Doc;
        other.m_Doc = nullptr;
        return *this;
    }

    bool JsonDocument::IsValid() const noexcept {
        return m_Doc != nullptr;
    }

    yyjson_doc *JsonDocument::Get() const noexcept {
        return m_Doc;
    }

    yyjson_val *JsonDocument::Root() const noexcept {
        return m_Doc ? yyjson_doc_get_root(m_Doc) : nullptr;
    }

    JsonDocument JsonDocument::Parse(std::string_view content, std::string &error) {
        error.clear();
        std::string mutableContent(content);
        yyjson_read_err readError{};
        yyjson_doc *doc = yyjson_read_opts(
            mutableContent.data(),
            mutableContent.size(),
            YYJSON_READ_NOFLAG,
            nullptr,
            &readError);
        if (!doc) {
            error = JsonReadErrorMessage(readError);
            return {};
        }
        return JsonDocument(doc);
    }

    JsonDocument JsonDocument::ParseFile(const std::filesystem::path &path, std::string &error) {
        error.clear();
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            error = "Unable to open JSON file";
            return {};
        }

        const std::string content{
            std::istreambuf_iterator<char>(ifs),
            std::istreambuf_iterator<char>()};
        return Parse(content, error);
    }

    MutableJsonDocument::MutableJsonDocument() : m_Doc(yyjson_mut_doc_new(nullptr)) {}

    MutableJsonDocument::~MutableJsonDocument() {
        if (m_Doc) {
            yyjson_mut_doc_free(m_Doc);
        }
    }

    MutableJsonDocument::MutableJsonDocument(MutableJsonDocument &&other) noexcept : m_Doc(other.m_Doc) {
        other.m_Doc = nullptr;
    }

    MutableJsonDocument &MutableJsonDocument::operator=(MutableJsonDocument &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (m_Doc) {
            yyjson_mut_doc_free(m_Doc);
        }
        m_Doc = other.m_Doc;
        other.m_Doc = nullptr;
        return *this;
    }

    bool MutableJsonDocument::IsValid() const noexcept {
        return m_Doc != nullptr;
    }

    yyjson_mut_doc *MutableJsonDocument::Get() const noexcept {
        return m_Doc;
    }

    yyjson_mut_val *MutableJsonDocument::Root() const noexcept {
        return m_Doc ? yyjson_mut_doc_get_root(m_Doc) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateObject() const noexcept {
        return m_Doc ? yyjson_mut_obj(m_Doc) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateArray() const noexcept {
        return m_Doc ? yyjson_mut_arr(m_Doc) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateNull() const noexcept {
        return m_Doc ? yyjson_mut_null(m_Doc) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateBool(bool value) const noexcept {
        return m_Doc ? yyjson_mut_bool(m_Doc, value) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateInt(int64_t value) const noexcept {
        return m_Doc ? yyjson_mut_int(m_Doc, value) : nullptr;
    }

    yyjson_mut_val *MutableJsonDocument::CreateString(std::string_view value) const noexcept {
        return m_Doc ? yyjson_mut_strncpy(m_Doc, value.data(), value.size()) : nullptr;
    }

    void MutableJsonDocument::SetRoot(yyjson_mut_val *root) const noexcept {
        if (m_Doc) {
            yyjson_mut_doc_set_root(m_Doc, root);
        }
    }

    bool MutableJsonDocument::AddValue(yyjson_mut_val *object,
                                       std::string_view key,
                                       yyjson_mut_val *value) const noexcept {
        if (!m_Doc || !yyjson_mut_is_obj(object) || !value) {
            return false;
        }
        yyjson_mut_val *copiedKey = CreateCopiedKey(m_Doc, key);
        return copiedKey && yyjson_mut_obj_add(object, copiedKey, value);
    }

    bool MutableJsonDocument::AddNull(yyjson_mut_val *object, std::string_view key) const noexcept {
        return AddValue(object, key, CreateNull());
    }

    bool MutableJsonDocument::AddBool(yyjson_mut_val *object, std::string_view key, bool value) const noexcept {
        return AddValue(object, key, CreateBool(value));
    }

    bool MutableJsonDocument::AddInt(yyjson_mut_val *object, std::string_view key, int64_t value) const noexcept {
        return AddValue(object, key, CreateInt(value));
    }

    bool MutableJsonDocument::AddString(yyjson_mut_val *object,
                                        std::string_view key,
                                        std::string_view value) const noexcept {
        return AddValue(object, key, CreateString(value));
    }

    bool MutableJsonDocument::AddValue(yyjson_mut_val *array, yyjson_mut_val *value) const noexcept {
        return m_Doc && yyjson_mut_is_arr(array) && value && yyjson_mut_arr_add_val(array, value);
    }

    bool MutableJsonDocument::AddNull(yyjson_mut_val *array) const noexcept {
        return AddValue(array, CreateNull());
    }

    bool MutableJsonDocument::AddBool(yyjson_mut_val *array, bool value) const noexcept {
        return AddValue(array, CreateBool(value));
    }

    bool MutableJsonDocument::AddInt(yyjson_mut_val *array, int64_t value) const noexcept {
        return AddValue(array, CreateInt(value));
    }

    bool MutableJsonDocument::AddString(yyjson_mut_val *array, std::string_view value) const noexcept {
        return AddValue(array, CreateString(value));
    }

    std::string MutableJsonDocument::Write(bool pretty, std::string &error) const {
        error.clear();
        if (!m_Doc || !yyjson_mut_doc_get_root(m_Doc)) {
            error = "JSON document has no root";
            return {};
        }

        const yyjson_write_flag flags = pretty
            ? static_cast<yyjson_write_flag>(YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END)
            : YYJSON_WRITE_NEWLINE_AT_END;
        yyjson_write_err writeError{};
        size_t len = 0;
        char *output = yyjson_mut_write_opts(m_Doc, flags, nullptr, &len, &writeError);
        if (!output) {
            error = JsonWriteErrorMessage(writeError);
            return {};
        }

        std::string result(output, len);
        std::free(output);
        return result;
    }

    std::string JsonReadErrorMessage(const yyjson_read_err &error) {
        if (error.msg) {
            return std::string(error.msg) + " at byte " + std::to_string(error.pos);
        }
        return "Failed to parse JSON";
    }

    std::string JsonWriteErrorMessage(const yyjson_write_err &error) {
        if (error.msg) {
            return error.msg;
        }
        return "Failed to write JSON";
    }

    std::string JsonGetString(const yyjson_val *object, const char *key, std::string_view defaultValue) {
        if (!object || !key) {
            return std::string(defaultValue);
        }
        yyjson_val *value = yyjson_obj_get(object, key);
        if (!value || !yyjson_is_str(value)) {
            return std::string(defaultValue);
        }
        const char *text = yyjson_get_str(value);
        return text ? std::string(text) : std::string(defaultValue);
    }

    std::wstring JsonGetWString(const yyjson_val *object, const char *key) {
        return Utf8ToUtf16(JsonGetString(object, key));
    }

    bool JsonGetBool(const yyjson_val *object, const char *key, bool defaultValue) {
        if (!object || !key) {
            return defaultValue;
        }
        yyjson_val *value = yyjson_obj_get(object, key);
        return value && yyjson_is_bool(value) ? yyjson_get_bool(value) : defaultValue;
    }

    int64_t JsonGetInt(const yyjson_val *object, const char *key, int64_t defaultValue) {
        if (!object || !key) {
            return defaultValue;
        }
        yyjson_val *value = yyjson_obj_get(object, key);
        return value && yyjson_is_num(value) ? yyjson_get_int(value) : defaultValue;
    }
} // namespace utils
