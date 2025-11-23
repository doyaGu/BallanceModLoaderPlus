#ifndef BML_ICONFIG_H
#define BML_ICONFIG_H

#include "CKEnums.h"

#include "BML/Defines.h"

class BML_EXPORT IProperty {
public:
    virtual const char *GetString() = 0;
    virtual bool GetBoolean() = 0;
    virtual int GetInteger() = 0;
    virtual float GetFloat() = 0;
    virtual CKKEYBOARD GetKey() = 0;

    virtual void SetString(const char *value) = 0;
    virtual void SetBoolean(bool value) = 0;
    virtual void SetInteger(int value) = 0;
    virtual void SetFloat(float value) = 0;
    virtual void SetKey(CKKEYBOARD value) = 0;

    virtual void SetComment(const char *comment) = 0;
    virtual void SetDefaultString(const char *value) = 0;
    virtual void SetDefaultBoolean(bool value) = 0;
    virtual void SetDefaultInteger(int value) = 0;
    virtual void SetDefaultFloat(float value) = 0;
    virtual void SetDefaultKey(CKKEYBOARD value) = 0;

    enum PropertyType {
        STRING,
        BOOLEAN,
        INTEGER,
        KEY,
        FLOAT,
        NONE
    };

    virtual PropertyType GetType() = 0;
};

class BML_EXPORT IConfig {
public:
    virtual bool HasCategory(const char *category) = 0;
    virtual bool HasKey(const char *category, const char *key) = 0;
    virtual IProperty *GetProperty(const char *category, const char *key) = 0;
    virtual void SetCategoryComment(const char *category, const char *comment) = 0;
    virtual ~IConfig() = default;
};

#endif // BML_ICONFIG_H