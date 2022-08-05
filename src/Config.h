#ifndef BML_CONFIG_H
#define BML_CONFIG_H

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <functional>

#include "IConfig.h"
#include "IMod.h"

class Config;

class Property : public IProperty {
    friend class Config;
    friend class GuiModMenu;
    friend class GuiModCategory;

public:
    Property() = default;
    Property(Config *config, std::string category, std::string key);

    const char *GetString() override;
    bool GetBoolean() override;
    int GetInteger() override;
    float GetFloat() override;
    CKKEYBOARD GetKey() override;

    void SetString(const char *value) override;
    void SetBoolean(bool value) override;
    void SetInteger(int value) override;
    void SetFloat(float value) override;
    void SetKey(CKKEYBOARD value) override;

    const char *GetComment();
    void SetComment(const char *comment) override;

    void SetDefaultString(const char *value) override;
    void SetDefaultBoolean(bool value) override;
    void SetDefaultInteger(int value) override;
    void SetDefaultFloat(float value) override;
    void SetDefaultKey(CKKEYBOARD value) override;

    PropertyType GetType() override { return m_type; }

    void CopyValue(Property *o);

private:
    union {
        bool m_bool;
        int m_int = 0;
        float m_float;
        CKKEYBOARD m_key;
    } m_value;
    std::string m_string;

    PropertyType m_type = INTEGER;
    std::string m_comment;

    std::string m_category, m_key;
    Config *m_config = nullptr;
};

class Config : public IConfig {
    friend class Property;
    friend class GuiModMenu;
    friend class GuiModCategory;

public:
    explicit Config(IMod *mod);
    ~Config() override;

    IMod *GetMod() { return m_Mod; }

    bool HasCategory(const char *category) override;
    bool HasKey(const char *category, const char *key) override;

    IProperty *GetProperty(const char *category, const char *key) override;
    const char *GetCategoryComment(const char *category);
    void SetCategoryComment(const char *category, const char *comment) override;

    void Load();
    void Save();

private:
    void trim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char x){return !std::isspace(x); }));
		s.erase(std::find_if(s.rbegin(), s.rend(), [](char x){return !std::isspace(x); }).base(), s.end());
    }

    IMod *m_Mod;
    std::string m_ModName;

    struct Category {
        std::string name;
        std::string comment;
        Config *config;

        std::vector<Property *> props;

        Property *GetProperty(const char *name);
    };

    Category &GetCategory(const char *name);

    std::vector<Category> m_Data;
};

#endif // BML_CONFIG_H