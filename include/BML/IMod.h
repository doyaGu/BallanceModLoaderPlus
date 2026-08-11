// What a Mod derives from. Implement the five identity functions, write
// DECLARE_BML_VERSION, override the callbacks the Mod needs, and return one
// instance from the DLL's BMLEntry. The loader takes that pointer and hands it
// back to BMLExit to be destroyed, so both live in the Mod's own DLL and the
// object is allocated and freed by the same C++ runtime.
//
// The loader works out which callbacks a Mod overrides by comparing its vtable
// against a blank IMod subclass, and only calls the slots that differ. A callback
// whose signature does not match the declaration below therefore becomes a new
// slot rather than an override, and the loader never calls it. Write override on
// every one, which turns that mistake into a compile error.
//
// Every callback runs on the game thread. The loader catches whatever a callback
// throws, logs it against the Mod's id, and goes on to the next Mod, so an
// escaping exception abandons the rest of that callback quietly instead of
// crashing the game.
//
// The gameplay callbacks are in IMessageReceiver, which this class inherits.
#ifndef BML_IMOD_H
#define BML_IMOD_H

#include <string>
#include <vector>

#include "CKAll.h"

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IConfig.h"

// A three-part version, ordered by major, then minor, then patch. The default
// constructor takes the version of the SDK headers it is compiled against, which
// is what makes DECLARE_BML_VERSION work.
struct BMLVersion {
    int major, minor, patch;

    BMLVersion() : major(BML_MAJOR_VERSION), minor(BML_MINOR_VERSION), patch(BML_PATCH_VERSION) {}
    BMLVersion(int mj, int mn, int bd) : major(mj), minor(mn), patch(bd) {}

    bool operator<(const BMLVersion &o) const {
        if (major == o.major) {
            if (minor == o.minor)
                return patch < o.patch;
            return minor < o.minor;
        }
        return major < o.major;
    }

    bool operator>=(const BMLVersion &o) const {
        return !(*this < o);
    }

    bool operator==(const BMLVersion &o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }

    std::string ToString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
        return buf;
    }
};

// Implements GetBMLVersion from the SDK headers the Mod is being compiled
// against. Write it once in the class body.
#define DECLARE_BML_VERSION \
    BMLVersion GetBMLVersion() override { return { BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION }; }

// One entry of a Mod's dependency list, which IBML::GetDependencyInfo reports a
// copy of. A Mod does not build these itself; it calls IMod::AddDependency. id is
// allocated with BML_Malloc and released with BML_Free so that it can be handed
// across the DLL boundary, and it is left null when that allocation fails, which
// turns the entry into an empty one instead of reporting the failure. The
// comparison operators look at id alone and ignore minVersion and optional.
struct ModDependency {
    char *id;
    BMLVersion minVersion;
    int optional; // (0 = required, 1 = optional)

    ModDependency() : id(nullptr), optional(0) {}

    ModDependency(const char *modId, const BMLVersion &version, int isOptional = 0)
        : minVersion(version), optional(isOptional) {
        if (modId && strlen(modId) > 0) {
            size_t len = strlen(modId);
            id = static_cast<char *>(BML_Malloc(len + 1));
            if (id) {
                memcpy(id, modId, len);
                id[len] = '\0';
            }
        } else {
            id = nullptr;
        }
    }

    ModDependency(const ModDependency &other) : minVersion(other.minVersion), optional(other.optional) {
        if (other.id && strlen(other.id) > 0) {
            size_t len = strlen(other.id);
            id = static_cast<char *>(BML_Malloc(len + 1));
            if (id) {
                memcpy(id, other.id, len);
                id[len] = '\0';
            }
        } else {
            id = nullptr;
        }
    }

    ModDependency &operator=(const ModDependency &other) {
        if (this != &other) {
            if (id) {
                BML_Free(id);
                id = nullptr;
            }

            if (other.id && strlen(other.id) > 0) {
                size_t len = strlen(other.id);
                id = static_cast<char *>(BML_Malloc(len + 1));
                if (id) {
                    memcpy(id, other.id, len);
                    id[len] = '\0';
                }
            }

            minVersion = other.minVersion;
            optional = other.optional;
        }
        return *this;
    }

    ~ModDependency() {
        if (id) {
            BML_Free(id);
            id = nullptr;
        }
    }

    int operator==(const ModDependency &other) const {
        if (id == nullptr || other.id == nullptr)
            return id == other.id;
        return strcmp(id, other.id) == 0;
    }

    int operator!=(const ModDependency &other) const {
        return !(*this == other);
    }
};

class BML_EXPORT IMod : public IMessageReceiver {
public:
    explicit IMod(IBML *bml) : m_BML(bml) {}
    virtual ~IMod();

    // The identity of the Mod, and the key everything else files it under: the
    // config lives in Configs\<id>.cfg under the loader directory, log lines carry
    // it as their prefix, other Mods name it in their dependency list, and
    // IBML::FindMod matches it exactly. Changing it in a later release orphans the
    // config file the earlier one wrote. It has to be non-empty and unlike every
    // other loaded Mod's, or the loader refuses to register this Mod. The loader
    // keeps the pointer rather than copying the string, so return a string literal
    // or other storage that outlives the Mod.
    virtual const char *GetID() = 0;

    // Read back as up to three numbers by a lenient parser when another Mod
    // declares a dependency on this one: every run of digits is one part and
    // anything else separates, so "1.2.3-beta" and "v1.2.3+meta" both read as
    // 1.2.3, and "1.2" reads as 1.2.0. A string with no digits reads as 0.0.0 and
    // fails every dependency that asks for more than that.
    virtual const char *GetVersion() = 0;

    // Shown to the player as they are, by the bml command and by the mod list, and
    // not used to identify the Mod. Free text.
    virtual const char *GetName() = 0;
    virtual const char *GetAuthor() = 0;
    virtual const char *GetDescription() = 0;

    // Which loader the Mod needs, filled in by DECLARE_BML_VERSION from the SDK
    // headers it is compiled against. A loader older than this refuses to register
    // the Mod, logging that it requires that version, so an SDK function added in
    // 0.3.13 cannot be reached by a Mod that a 0.3.12 loader has accepted.
    virtual BMLVersion GetBMLVersion() = 0;

    // The first callback, and where the setting up belongs: registering commands,
    // ball and floor types, config keys, and IMC clients. It runs once the loader
    // has checked this Mod's dependencies, so a Mod that got here has them, and
    // GetConfig here reads the values the previous run saved. Dependencies
    // themselves are declared earlier than this, in the constructor.
    virtual void OnLoad() {}

    // Runs once at shutdown, for every Mod that loaded, in the reverse of the
    // order they loaded in. The loader tears the Mod's IMC state down right after
    // it returns, saves the configs once every Mod has been through, and ignores
    // any timer scheduled from here. Commands cannot be withdrawn, so do not
    // delete anything the loader still points at.
    virtual void OnUnload() {}

    // Runs from inside IProperty::SetString and its siblings, straight away and on
    // the setting thread, whenever one of them changes a value or its type. That
    // includes the Mod's own calls, so setting another value from in here calls
    // this again. It does not run while the loader reads the config file at
    // startup, because that path uses the SetDefault functions. prop is the
    // property that changed and already carries the new value.
    virtual void OnModifyConfig(const char *category, const char *key, IProperty *prop) {}

    // Every time one of the game's scripts runs the Object Load building block,
    // reported after the file has been read, with the arguments that block was
    // given. OnLoadObject runs once per load, with objArray listing what came in,
    // and OnLoadScript then runs once for each script among those objects. This is
    // where a Mod patches the game's own scripts and swaps its own assets in.
    //
    // filename is what the block was told to load, except that while a custom map
    // is being loaded the loader substitutes that map's name, so compare it against
    // the file the game ships rather than assuming a path. isMap is true only for
    // the load that Levelinit_build performs, which is the level itself.
    //
    // objArray and masterObj belong to the game: the array pointer is only good
    // until the callback returns, and the objects it names die with the level.
    virtual void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {}
    virtual void OnLoadScript(const char *filename, CKBehavior *script) {}

    // Draw all ImGui and Bui controls from OnProcess. It is the only callback
    // that runs inside the active ImGui frame.
    virtual void OnProcess() {}

    // OnRender runs after the loader has already ended the ImGui frame, so
    // ImGui and Bui calls made here draw nothing and may trip an ImGui
    // assertion. Use it for renderer state work only.
    virtual void OnRender(CK_RENDER_FLAGS flags) {}

    // The cheat flag changed, which is a single flag shared by every Mod. It is
    // only broadcast on a real change, so a Mod that keeps its own copy stays in
    // step with IBML::IsCheatEnabled.
    virtual void OnCheatEnabled(bool enable) {}

    // The game's Physicalize building block is about to run, on target. Both of
    // these are reported before the block does its work, so the physics object does
    // not exist yet inside OnPhysicalize and is already scheduled for removal inside
    // OnUnphysicalize. The values are copies, so changing them here changes nothing;
    // to alter the physics of an object, wait for the block to finish and act on the
    // object afterwards.
    //
    // convexMesh, ballCenter, ballRadius, and concaveMesh are the loader's own
    // arrays, freed as soon as the callback returns, and their counts can be zero,
    // in which case the pointer is null. collGroup and collSurface come straight
    // from the block's parameters and can also be null.
    virtual void OnPhysicalize(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                               const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                               float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                               int convexCnt, CKMesh **convexMesh, int ballCnt, VxVector *ballCenter, float *ballRadius,
                               int concaveCnt, CKMesh **concaveMesh) {}
    virtual void OnUnphysicalize(CK3dEntity *target) {}

    // Every command the loader runs, whoever registered it, which is how a Mod
    // watches the command bar without owning a command. args[0] is the command name
    // as it was typed, so the arguments start at args[1], and both callbacks see the
    // same vector. Neither runs for a line that names no known command or for a
    // cheat command refused because cheats are off, and OnPostCommandExecute is
    // skipped when the command itself throws.
    virtual void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {}
    virtual void OnPostCommandExecute(ICommand *command, const std::vector<std::string> &args) {}

protected:
    // Both are created on the first call and owned by this object, so do not delete
    // what they return. GetLogger writes to the loader's log with this Mod's id as
    // the prefix. The first GetConfig call reads Configs\<id>.cfg from the loader
    // directory, so call it from OnLoad or later, and reach for the SetDefault
    // functions on the properties rather than the Set ones, which would overwrite
    // what the player saved.
    virtual ILogger *GetLogger() final;
    virtual IConfig *GetConfig() final;

    // Declare these from the Mod constructor. The loader checks dependencies once,
    // between constructing the Mods and calling OnLoad, so one declared in OnLoad
    // is recorded but no longer gates anything. See IBML::RegisterDependency for
    // what a missing, too-old, or optional dependency does.
    bool AddDependency(const char *modId, const BMLVersion &minVersion = BMLVersion(0, 0, 0)) {
        return m_BML->RegisterDependency(this, modId, minVersion.major, minVersion.minor, minVersion.patch) == BML_OK;
    }

    bool AddOptionalDependency(const char *modId, const BMLVersion &minVersion = BMLVersion(0, 0, 0)) {
        return m_BML->RegisterOptionalDependency(this, modId, minVersion.major, minVersion.minor, minVersion.patch) == BML_OK;
    }

    // True when every required dependency is installed and new enough. The loader
    // has already made this check by the time OnLoad runs, so calling it there only
    // re-answers a question that has been answered; it is useful after
    // ClearDependencies has changed the list.
    bool CheckDependencies() {
        return m_BML->CheckDependencies(this) != 0;
    }

    // How many entries the list holds, required and optional together. Read them
    // with IBML::GetDependencyInfo.
    int GetDependencyCount() {
        return m_BML->GetDependencyCount(this);
    }

    // Drops the whole list. The loader has already used it by the time any callback
    // runs, so this does not unload anything; it only stops the list being reported.
    // The destructor does it as well.
    bool ClearDependencies() {
        return m_BML->ClearDependencies(this) == BML_OK;
    }

    // The loader, handed to the constructor. It stays valid for as long as the Mod
    // does, and is never null for a Mod the loader constructed through BMLEntry.
    IBML *m_BML = nullptr;

private:
    ILogger *m_Logger = nullptr;
    IConfig *m_Config = nullptr;
};

#endif // BML_IMOD_H
