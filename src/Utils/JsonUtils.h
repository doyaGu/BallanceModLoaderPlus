#ifndef BML_JSONUTILS_H
#define BML_JSONUTILS_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include <yyjson.h>

namespace utils {
    class JsonDocument {
    public:
        JsonDocument() = default;
        explicit JsonDocument(yyjson_doc *doc) noexcept;
        ~JsonDocument();

        JsonDocument(JsonDocument &&other) noexcept;
        JsonDocument &operator=(JsonDocument &&other) noexcept;

        JsonDocument(const JsonDocument &) = delete;
        JsonDocument &operator=(const JsonDocument &) = delete;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] yyjson_doc *Get() const noexcept;
        [[nodiscard]] yyjson_val *Root() const noexcept;

        static JsonDocument Parse(std::string_view content, std::string &error);
        static JsonDocument ParseFile(const std::filesystem::path &path, std::string &error);

    private:
        yyjson_doc *m_Doc{nullptr};
    };

    class MutableJsonDocument {
    public:
        MutableJsonDocument();
        ~MutableJsonDocument();

        MutableJsonDocument(MutableJsonDocument &&other) noexcept;
        MutableJsonDocument &operator=(MutableJsonDocument &&other) noexcept;

        MutableJsonDocument(const MutableJsonDocument &) = delete;
        MutableJsonDocument &operator=(const MutableJsonDocument &) = delete;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] yyjson_mut_doc *Get() const noexcept;
        [[nodiscard]] yyjson_mut_val *Root() const noexcept;

        [[nodiscard]] yyjson_mut_val *CreateObject() const noexcept;
        [[nodiscard]] yyjson_mut_val *CreateArray() const noexcept;
        [[nodiscard]] yyjson_mut_val *CreateNull() const noexcept;
        [[nodiscard]] yyjson_mut_val *CreateBool(bool value) const noexcept;
        [[nodiscard]] yyjson_mut_val *CreateInt(int64_t value) const noexcept;
        [[nodiscard]] yyjson_mut_val *CreateString(std::string_view value) const noexcept;

        void SetRoot(yyjson_mut_val *root) const noexcept;

        [[nodiscard]] bool AddValue(yyjson_mut_val *object,
                                    std::string_view key,
                                    yyjson_mut_val *value) const noexcept;
        [[nodiscard]] bool AddNull(yyjson_mut_val *object, std::string_view key) const noexcept;
        [[nodiscard]] bool AddBool(yyjson_mut_val *object, std::string_view key, bool value) const noexcept;
        [[nodiscard]] bool AddInt(yyjson_mut_val *object, std::string_view key, int64_t value) const noexcept;
        [[nodiscard]] bool AddString(yyjson_mut_val *object,
                                     std::string_view key,
                                     std::string_view value) const noexcept;

        [[nodiscard]] bool AddValue(yyjson_mut_val *array, yyjson_mut_val *value) const noexcept;
        [[nodiscard]] bool AddNull(yyjson_mut_val *array) const noexcept;
        [[nodiscard]] bool AddBool(yyjson_mut_val *array, bool value) const noexcept;
        [[nodiscard]] bool AddInt(yyjson_mut_val *array, int64_t value) const noexcept;
        [[nodiscard]] bool AddString(yyjson_mut_val *array, std::string_view value) const noexcept;

        [[nodiscard]] std::string Write(bool pretty, std::string &error) const;

    private:
        yyjson_mut_doc *m_Doc{nullptr};
    };

    [[nodiscard]] std::string JsonReadErrorMessage(const yyjson_read_err &error);
    [[nodiscard]] std::string JsonWriteErrorMessage(const yyjson_write_err &error);
    [[nodiscard]] std::string JsonGetString(const yyjson_val *object,
                                            const char *key,
                                            std::string_view defaultValue = {});
    [[nodiscard]] std::wstring JsonGetWString(const yyjson_val *object, const char *key);
    [[nodiscard]] bool JsonGetBool(const yyjson_val *object, const char *key, bool defaultValue = false);
    [[nodiscard]] int64_t JsonGetInt(const yyjson_val *object, const char *key, int64_t defaultValue = 0);
} // namespace utils

#endif // BML_JSONUTILS_H
