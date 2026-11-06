// Private configuration storage interface used by loader features and script bindings.
// Native Mods use BML/Config.h and BML/Config.hpp.
#ifndef BML_ICONFIG_H
#define BML_ICONFIG_H

#include "CKEnums.h"

#include "BML/Defines.h"

#include "BML/Config.h"

// One setting. The loader owns it, it lives as long as the config does, and a Mod
// may keep the pointer.
class IProperty {
public:
    // Each of these returns the stored value when GetType matches, and an empty
    // value otherwise: an empty string, false, 0, 0.0f, or key 0. So a property
    // that has never been given a type reads as empty through every one of them.
    // The pointer GetString returns belongs to the property and is replaced by the
    // next SetString, so copy it rather than storing it.
    virtual const char *GetString() = 0;
    virtual bool GetBoolean() = 0;
    virtual int GetInteger() = 0;
    virtual float GetFloat() = 0;
    virtual CKKEYBOARD GetKey() = 0;

    // Writes the value and makes that the property's type, replacing whatever type
    // it had. Each one does nothing at all when the value and the type are already
    // what is being written; otherwise it queues the owning Mod's OnModifyConfig
    // callback and a config save for the end of the current frame. The in-memory
    // value is visible immediately, but the callback is deliberately not re-entrant.
    // SetString treats a null value as an empty string.
    virtual void SetString(const char *value) = 0;
    virtual void SetBoolean(bool value) = 0;
    virtual void SetInteger(int value) = 0;
    virtual void SetFloat(float value) = 0;
    virtual void SetKey(CKKEYBOARD value) = 0;

    // The line the loader writes above the entry in the file and shows next to it
    // in the mod menu. It is stored, not written through, so it reaches the file
    // with the next save. A null comment clears it.
    virtual void SetComment(const char *comment) = 0;

    // Sets the type and the starting value, but only while the property does not
    // already have that type, which is what makes these safe to call on every run:
    // a value loaded from the file keeps its type and survives untouched. Nothing is
    // reported when the call is skipped, and nothing notifies the Mod or touches
    // the file. Note that a property whose saved type differs from the one asked
    // for here loses its saved value, which is what happens when a Mod changes the
    // type of a setting between releases.
    virtual void SetDefaultString(const char *value) = 0;
    virtual void SetDefaultBoolean(bool value) = 0;
    virtual void SetDefaultInteger(int value) = 0;
    virtual void SetDefaultFloat(float value) = 0;
    virtual void SetDefaultKey(CKKEYBOARD value) = 0;

    // KEY is an integer holding a CKKEYBOARD value, which the mod menu offers a key
    // picker for instead of a number box, and which it strips the modifiers from, so
    // a KEY property holds one plain key. NONE means the property exists but has
    // never been given a type, either because GetProperty had just created it or
    // because no SetDefault call has run on it. The mod menu shows such a property
    // as a blank row and the file records it as the integer 0, so do not leave a
    // property that a Mod created at NONE.
    enum PropertyType { STRING, BOOLEAN, INTEGER, KEY, FLOAT, NONE };

    virtual PropertyType GetType() = 0;
};

namespace BML::ConfigMetadata {

// Metadata for loader-owned properties. COLOR and CHOICE apply to strings.
// Get returns DEFAULT for null/unsupported properties; Set returns 1 or 0.
BML_ConfigPropertyEditor GetEditor(const IProperty *property);
int SetEditor(IProperty *property, BML_ConfigPropertyEditor editor);

// Replaces the copied choice metadata in display order. An empty string is a
// valid choice; its presentation is chosen by the editor. Passing a null
// choices pointer is valid only when count is zero. Returned strings belong to
// the property and remain valid until its choices are replaced or the property
// is destroyed. Count/Get
// return 0/null for a null or unsupported property or an out-of-range index;
// Set returns 0 for those properties, invalid input, duplicates, or allocation
// failure.
size_t ChoiceCount(const IProperty *property);
const char *Choice(const IProperty *property, size_t index);
int SetChoices(IProperty *property, const char *const choices[], size_t count);

} // namespace BML::ConfigMetadata

class IConfig {
public:
    // Whether the initially loaded values or the current Mod declarations hold
    // this category or key. Both answer false for a null argument. These queries
    // do not create menu entries. GetProperty declares the setting, so ask first
    // if the difference between a saved value and a new setting matters.
    virtual bool HasCategory(const char *category) = 0;
    virtual bool HasKey(const char *category, const char *key) = 0;

    // Never null except for a null category or key: an unknown category and key are
    // declared on the spot, importing the saved value if present, otherwise using
    // type NONE ready for a SetDefault call. Category
    // and key names are matched exactly, case included, and they are written into
    // the file as single whitespace-separated words, so keep them free of spaces.
    virtual IProperty *GetProperty(const char *category, const char *key) = 0;

    // The line written above the category in the file and shown as its heading in
    // the mod menu. Creates the category if it does not exist yet, and reaches the
    // file with the next save.
    virtual void SetCategoryComment(const char *category, const char *comment) = 0;

    virtual ~IConfig() = default;
};

#endif // BML_ICONFIG_H
