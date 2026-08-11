// A Mod's settings, persisted as Configs\<mod id>.cfg under the loader directory
// and editable by the player from the loader's mod menu. Get the one instance from
// IMod::GetConfig; the loader owns it and reads the file on that first call.
//
// The usual shape of a Mod's setup, in OnLoad:
//
//     IProperty *prop = GetConfig()->GetProperty("Misc", "ShowHud");
//     prop->SetComment("Draw the HUD.");
//     prop->SetDefaultBoolean(true);
//     m_ShowHud = prop->GetBoolean();
//
// SetDefaultBoolean is what makes that work: the SetDefault functions write only
// when the property does not yet have their type, so they establish the type and
// the starting value on a first run and leave the saved value alone afterwards. The
// plain Set functions always write, so using one where a default was meant
// overwrites what the player chose.
//
// A property has one type, and the getters do not convert between them: GetFloat on
// a property holding an integer returns 0.0f rather than that number. Read with the
// getter that matches the type the Mod declared.
//
// Everything here is for the game thread. Nothing is locked, and a write reaches
// the Mod's OnModifyConfig and the file on disk before it returns.
#ifndef BML_ICONFIG_H
#define BML_ICONFIG_H

#include "CKEnums.h"

#include "BML/Defines.h"

// One setting. The loader owns it, it lives as long as the config does, and a Mod
// may keep the pointer.
class BML_EXPORT IProperty {
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
    // what is being written; otherwise it calls the owning Mod's OnModifyConfig and
    // saves the whole config file before returning, so this is a write to disk and
    // not just to memory. Do not call one every frame. SetString treats a null
    // value as an empty string.
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
    // Whether the file that was read holds this category, or this key in it. Both
    // answer false for a null argument. Note that GetProperty creates what it does
    // not find, so asking these after that call reports what the Mod itself just
    // created; ask first if the difference matters.
    virtual bool HasCategory(const char *category) = 0;
    virtual bool HasKey(const char *category, const char *key) = 0;

    // Never null except for a null category or key: an unknown category and key are
    // created on the spot, with the type NONE, ready for a SetDefault call. Category
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
