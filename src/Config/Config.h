#ifndef BML_CONFIG_INTERNAL_H
#define BML_CONFIG_INTERNAL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "BML/IConfig.h"
#include "BML/IMod.h"

#include "Config/ConfigValue.h"

class Config;

class Property : public IProperty {
    friend class Config;
    friend class Category;
    friend BML_ConfigPropertyEditor BML_GetConfigPropertyEditor(const IProperty *property);
    friend int BML_SetConfigPropertyEditor(IProperty *property,
                                           BML_ConfigPropertyEditor editor);
    friend std::size_t BML_GetConfigPropertyChoiceCount(const IProperty *property);
    friend const char *BML_GetConfigPropertyChoice(const IProperty *property,
                                                   std::size_t index);
    friend int BML_SetConfigPropertyChoices(IProperty *property,
                                            const char *const choices[],
                                            std::size_t count);

public:
    using Value = ConfigValue;

    Property() = default;
    Property(Config *config, std::string category, std::string key);
    ~Property() = default;

    const char *GetName() const { return m_Key.c_str(); }

    const char *GetString() override;
    std::size_t GetStringSize();
    bool GetBoolean() override;
    int GetInteger() override;
    float GetFloat() override;
    CKKEYBOARD GetKey() override;

    void SetString(const char *value) override;
    void SetBoolean(bool value) override;
    void SetInteger(int value) override;
    void SetFloat(float value) override;
    void SetKey(CKKEYBOARD value) override;

    const char *GetComment() const { return m_Comment.c_str(); }
    void SetComment(const char *comment) override;

    void SetDefaultString(const char *value) override;
    void SetDefaultBoolean(bool value) override;
    void SetDefaultInteger(int value) override;
    void SetDefaultFloat(float value) override;
    void SetDefaultKey(CKKEYBOARD value) override;

    PropertyType GetType() override { return m_Type; }
    const Value &GetValue() const { return m_Value; }
    void SetValue(const Value &value);
    std::size_t GetHash() const;

    void CopyValue(Property *o);

    void SetModified(PropertyType previousType);

private:
    BML_ConfigPropertyEditor GetEditorMetadata() const;
    bool SetEditorMetadata(BML_ConfigPropertyEditor editor);
    bool SetChoiceMetadata(const char *const choices[], std::size_t count);

    Value m_Value = 0;
    PropertyType m_Type = INTEGER;
    BML_ConfigPropertyEditor m_Editor = BML_CONFIG_EDITOR_DEFAULT;
    std::vector<std::string> m_Choices;
    std::size_t m_Hash = 0;
    std::string m_Comment;
    std::string m_Category;
    std::string m_Key;
    Config *m_Config = nullptr;
};

class Category {
    friend class Config;

public:
    Category() = default;
    Category(Config *config, std::string name);
    ~Category();

    const char *GetName() const { return m_Name.c_str(); }

    const char *GetComment() const { return m_Comment.c_str(); }
    void SetComment(const char *comment);

    std::size_t GetPropertyCount() const { return m_Properties.size(); }
    Property *GetProperty(std::size_t i);
    Property *GetProperty(const char *key);

    bool HasKey(const char *key) const;

private:
    std::string m_Name;
    std::string m_Comment;
    Config *m_Config = nullptr;

    std::vector<Property *> m_Properties;
    std::unordered_map<std::string, Property *> m_PropertyMap;
};

class Config : public IConfig {
    friend class Category;
    friend class Property;

public:
    struct Edit {
        std::string Category;
        std::string Key;
        IProperty::PropertyType ExpectedType = IProperty::NONE;
        Property::Value BaseValue = 0;
        Property::Value NewValue = 0;
    };

    enum class ApplyError {
        None,
        OwnerChanged,
        SchemaChanged,
        PropertyMissing,
        TypeChanged,
        BaseChanged,
        DuplicateTarget,
        InvalidValue,
    };

    struct ApplyResult {
        static constexpr std::size_t NoEdit = static_cast<std::size_t>(-1);

        ApplyError Error = ApplyError::None;
        std::size_t EditIndex = NoEdit;
        std::size_t ChangedCount = 0;

        bool Succeeded() const { return Error == ApplyError::None; }
        explicit operator bool() const { return Succeeded(); }
    };

    struct PendingNotification {
        std::string Category;
        std::string Key;
        Property *ChangedProperty = nullptr;
    };

    explicit Config(IMod *mod);
    ~Config() override;

    IMod *GetMod() const { return m_Mod; }
    const std::string &GetModID() const { return m_ModID; }
    void SnapshotModMetadata();

    std::size_t GetCategoryCount() const { return m_Categories.size(); }
    Category *GetCategory(std::size_t i);
    Category *GetCategory(const char *name);

    bool HasCategory(const char *category) override;
    bool HasKey(const char *category, const char *key) override;

    IProperty *GetProperty(const char *category, const char *key) override;
    const char *GetCategoryComment(const char *category);
    void SetCategoryComment(const char *category, const char *comment) override;

    bool Load(const wchar_t *path);
    bool Save(const wchar_t *path);

    bool IsDirty() const { return m_Dirty; }
    // Schema changes invalidate documents built from category, property, or type
    // metadata. Value changes can be reconciled independently by stable key.
    std::uint64_t GetSchemaRevision() const { return m_SchemaRevision; }
    std::uint64_t GetValueRevision() const { return m_ValueRevision; }

    // This is an implementation-only transaction boundary for loader-owned UI.
    // Every edit is validated before any property is changed.
    ApplyResult ApplyEdits(const IMod *expectedOwner, std::uint64_t expectedSchemaRevision,
                           const std::vector<Edit> &edits);
    std::vector<PendingNotification> TakePendingNotifications();

private:
    void MarkDirty() { m_Dirty = true; }
    void TouchSchema() { ++m_SchemaRevision; }
    void TouchValue() { ++m_ValueRevision; }
    void QueueNotification(Property *property, const std::string &category, const std::string &key);

    IMod *m_Mod;
    std::string m_ModID;
    std::string m_ModName = "Unknown";
    std::string m_ModVersion = "Unknown";
    bool m_Dirty = false;
    std::uint64_t m_SchemaRevision = 0;
    std::uint64_t m_ValueRevision = 0;
    std::vector<PendingNotification> m_PendingNotifications;

    std::vector<Category *> m_Categories;
    std::unordered_map<std::string, Category *> m_CategoryMap;
};

#endif // BML_CONFIG_INTERNAL_H
