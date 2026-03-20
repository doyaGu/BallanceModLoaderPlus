#ifndef BML_MODMENU_CONFIG_H
#define BML_MODMENU_CONFIG_H

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "CKEnums.h"

#include "bml_builtin_interfaces.h"
#include "bml_types.h"

class Property {
public:
    enum PropertyType {
        STRING,
        BOOLEAN,
        INTEGER,
        KEY,
        FLOAT,
        NONE
    };

    Property() = default;
    Property(BML_Mod mod,
             const BML_CoreConfigInterface *config,
             std::string category,
             std::string key);

    const char *GetName() const { return m_Key.c_str(); }
    const char *GetString();
    bool GetBoolean();
    int GetInteger();
    float GetFloat();
    CKKEYBOARD GetKey();

    void SetString(const char *value);
    void SetBoolean(bool value);
    void SetInteger(int value);
    void SetFloat(float value);
    void SetKey(CKKEYBOARD value);

    const char *GetComment() const { return m_Comment.c_str(); }
    void SetComment(const char *comment) { m_Comment = comment ? comment : ""; }

    void SetDefaultString(const char *value);
    void SetDefaultBoolean(bool value);
    void SetDefaultInteger(int value);
    void SetDefaultFloat(float value);
    void SetDefaultKey(CKKEYBOARD value);

    PropertyType GetType() const { return m_Type; }

private:
    bool CommitValue();

    BML_Mod m_Mod = nullptr;
    const BML_CoreConfigInterface *m_Config = nullptr;
    std::variant<bool, int, float, std::string> m_Value = 0;
    PropertyType m_Type = NONE;
    std::string m_Comment;
    std::string m_Category;
    std::string m_Key;
};

class Category {
public:
    Category() = default;
    Category(BML_Mod mod, const BML_CoreConfigInterface *config, std::string name);
    ~Category();

    const char *GetName() const { return m_Name.c_str(); }
    const char *GetComment() const { return m_Comment.c_str(); }
    void SetComment(const char *comment) { m_Comment = comment ? comment : ""; }

    size_t GetPropertyCount() const { return m_Properties.size(); }
    Property *GetProperty(size_t index);
    Property *GetProperty(const char *key);
    bool HasKey(const char *key) const;

private:
    BML_Mod m_Mod = nullptr;
    const BML_CoreConfigInterface *m_Config = nullptr;
    std::string m_Name;
    std::string m_Comment;
    std::vector<Property *> m_Properties;
    std::unordered_map<std::string, Property *> m_PropertyMap;
};

class Config {
public:
    Config(BML_Mod mod, const BML_CoreConfigInterface *config);
    ~Config();

    size_t GetCategoryCount() const { return m_Categories.size(); }
    Category *GetCategory(size_t index);
    Category *GetCategory(const char *name);

    void Reload();

private:
    void Clear();

    BML_Mod m_Mod = nullptr;
    const BML_CoreConfigInterface *m_Config = nullptr;
    std::vector<Category *> m_Categories;
    std::unordered_map<std::string, Category *> m_CategoryMap;
};

#endif // BML_MODMENU_CONFIG_H
