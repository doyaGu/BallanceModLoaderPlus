#ifndef BML_CONFIG_H
#define BML_CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

#include "BML/IConfig.h"
#include "BML/IMod.h"

class Config;

class Property : public IProperty {
    friend class Config;
    friend class Category;

public:
    Property() = default;
    Property(Config *config, std::string category, std::string key);
    ~Property() = default;

    const char *GetName() const { return m_Key.c_str(); }

    const char *GetString() override;
    size_t GetStringSize();
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
    void SetComment(const char *comment) override { m_Comment = comment ? comment : ""; }

    void SetDefaultString(const char *value) override;
    void SetDefaultBoolean(bool value) override;
    void SetDefaultInteger(int value) override;
    void SetDefaultFloat(float value) override;
    void SetDefaultKey(CKKEYBOARD value) override;

    PropertyType GetType() override { return m_Type; }
    size_t GetHash() const;

    void CopyValue(Property *o);

    bool *GetBooleanPtr();
    int *GetIntegerPtr();
    float *GetFloatPtr();
    CKKEYBOARD *GetKeyPtr();

    void SetModified();

private:
    std::variant<bool, int, float, std::string> m_Value = 0;
    PropertyType m_Type = INTEGER;
    size_t m_Hash = 0;
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
    void SetComment(const char *comment) { m_Comment = comment ? comment : ""; }

    size_t GetPropertyCount() const { return m_Properties.size(); }
    Property *GetProperty(size_t i);
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
    friend class Property;

public:
    explicit Config(IMod *mod);
    ~Config() override;

    IMod *GetMod() const { return m_Mod; }

    size_t GetCategoryCount() const { return m_Categories.size(); }
    Category *GetCategory(size_t i);
    Category *GetCategory(const char *name);

    bool HasCategory(const char *category) override;
    bool HasKey(const char *category, const char *key) override;

    IProperty *GetProperty(const char *category, const char *key) override;
    const char *GetCategoryComment(const char *category);
    void SetCategoryComment(const char *category, const char *comment) override;

    bool Load(const wchar_t *path);
    bool Save(const wchar_t *path);

private:
    IMod *m_Mod;
    std::string m_ModID;

    std::vector<Category *> m_Categories;
    std::unordered_map<std::string, Category *> m_CategoryMap;
};

#endif // BML_CONFIG_H
