// Private lifecycle adapter for built-in and Script Mods, and the native C protocol bridge.
// External Mods use BML/Mod.h or the module-local BML/Mod.hpp wrapper.
#ifndef BML_IMOD_H
#define BML_IMOD_H

#include <string>
#include <vector>

#include "CKAll.h"

#include "BML/BML.h"
#include "Console/CommandRecord.h"
#include "Loader/IMessageReceiver.h"
#include "Logging/Logger.h"
#include "Config/Config.h"

class ModContext;

// A three-part version, ordered by major, then minor, then patch. The default
// constructor takes the version of the SDK headers it is compiled against, which
// is what the built-in adapters report.
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

    bool operator>=(const BMLVersion &o) const { return !(*this < o); }

    bool operator==(const BMLVersion &o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }

    std::string ToString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
        return buf;
    }
};

// One entry of a Mod's dependency list, which ModContext::GetDependencyInfo reports a
// copy of. The loader builds entries from native or script declarations. The
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

    ModDependency(const ModDependency &other)
        : minVersion(other.minVersion), optional(other.optional) {
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

    int operator!=(const ModDependency &other) const { return !(*this == other); }
};

class IMod : public IMessageReceiver {
public:
    explicit IMod(ModContext *bml) : m_BML(bml) {}

    virtual ~IMod();

    // The identity of the Mod, and the key everything else files it under: the
    // config lives in Configs\<id>.cfg under the loader directory, log lines carry
    // it as their prefix, other Mods name it in their dependency list, and
    // ModContext::FindMod matches it exactly. Changing it in a later release orphans the
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
    // 0.3.14 cannot be reached by a Mod that a 0.3.13 loader has accepted.
    virtual BMLVersion GetBMLVersion() = 0;

    // The first callback, and where the setting up belongs: registering commands,
    // ball and floor types, config keys, and IMC clients. It runs once the loader
    // has checked this Mod's dependencies, so a Mod that got here has them, and
    // GetConfig here reads the values the previous run saved. Dependencies
    // themselves are declared earlier than this, in the constructor.
    virtual void OnLoad() {}

    // Runs once at shutdown, for every Mod that loaded, in the reverse of the
    // order they loaded in. This is the final event callback into the Mod; the
    // loader may still read the stable identity metadata promised above while it
    // detaches registrations. It tears the Mod's IMC state down right after this
    // returns, saves the configs once every Mod has been through, and ignores any
    // timer scheduled from here. A config write here is saved but does not cause a
    // later OnModifyConfig callback. Commands registered through the legacy
    // RegisterCommand path remain until loader teardown. Commands registered
    // through bml.command may be unregistered, and all remaining registrations
    // are detached before the Mod DLL is released.
    virtual void OnUnload() {}

    // Runs during the loader's config flush after Property::SetString or one of its
    // siblings changes a value or its type. The in-memory value changes immediately,
    // but writes to the same property in one frame are coalesced into one callback
    // carrying the latest value. A write made from this callback is queued for a
    // later flush instead of calling recursively. The loader also flushes after
    // OnLoad and immediately before normal teardown. Writes made from OnUnload are
    // persisted without another callback because OnUnload is the final callback.
    // Loading a config file uses the SetDefault functions and does not queue this
    // callback. prop is the property that changed and already carries the new value.
    virtual void OnModifyConfig(const char *category, const char *key, Property *prop) {}

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
    virtual void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                              CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                              CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray,
                              CKObject *masterObj) {}

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
    // step with ModContext::IsCheatEnabled.
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
    virtual void OnPhysicalize(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity,
                               float mass, const char *collGroup, CKBOOL startFrozen,
                               CKBOOL enableColl, CKBOOL calcMassCenter, float linearDamp,
                               float rotDamp, const char *collSurface, VxVector massCenter,
                               int convexCnt, CKMesh **convexMesh, int ballCnt,
                               VxVector *ballCenter, float *ballRadius, int concaveCnt,
                               CKMesh **concaveMesh) {}

    virtual void OnUnphysicalize(CK3dEntity *target) {}

    // Every command the loader runs, whoever registered it, which is how a Mod
    // watches the command bar without owning a command. A line such as `a; b | c`
    // fires them once per simple command, after the shell has split and expanded
    // its words. args[0] is the word that named the command, so the arguments start
    // at args[1], and both callbacks see the same vector. Neither runs for a line that names no
    // known command or for a cheat command refused because cheats are off, and OnPostCommandExecute
    // is skipped when the command itself throws.
    virtual void OnPreCommandExecute(BML::CommandRecord *command, const std::vector<std::string> &args) {}

    virtual void OnPostCommandExecute(BML::CommandRecord *command, const std::vector<std::string> &args) {}

protected:
    // Both are created on the first call, so do not delete what they return.
    // GetLogger is owned by this object and writes to the loader's log with this
    // Mod's id as the prefix. GetConfig is owned by the loader until the Mod is
    // unregistered. Its first call reads Configs\<id>.cfg from the loader directory,
    // so call it from OnLoad or later, and reach for the SetDefault functions on the
    // properties rather than the Set ones, which would overwrite what the player saved.
    virtual Logger *GetLogger() final;
    virtual Config *GetConfig() final;

    // The loader, handed to the constructor. It stays valid for as long as the Mod
    // does, and is never null for an adapter constructed by this loader.
    ModContext *m_BML = nullptr;

private:
    friend class ModContext;

    Logger *m_Logger = nullptr;
    Config *m_Config = nullptr;
};

#endif // BML_IMOD_H
