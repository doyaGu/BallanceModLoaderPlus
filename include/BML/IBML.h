// What a Mod calls into the loader with. Every IMod already holds one as m_BML,
// so a Mod never looks this up and never implements it.
//
// The vtable layout of this class is fixed. Existing functions keep their order
// and their signatures across loader releases, because a Mod compiled against an
// older SDK calls them by slot number, and no function is ever added, removed, or
// reordered here. Capabilities that arrived after the layout was fixed live
// elsewhere: the BML_* C functions in BML.h, and the versioned interfaces and
// C++ facades in Runtime.h, Scene.h, Gameplay.h, UI.h, Speedrun.h, and Events.h.
// So when something a Mod needs is missing here, look there rather than expecting
// it to appear below.
//
// Everything here is for the game thread, called from a Mod callback. Nothing
// here is safe from a thread of the Mod's own.
#ifndef BML_IBML_H
#define BML_IBML_H

#include <functional>

#include "CKAll.h"

#include "BML/Defines.h"
#include "BML/ICommand.h"
#include "BML/IMessageReceiver.h"
#include "BML/InputHook.h"

class IMod;

class BML_EXPORT IBML : public IMessageReceiver {
public:
    virtual ~IBML() = default;

    // GetCKContext is the context the loader was created with and stays the same
    // for the whole run. GetRenderContext asks the context for its player render
    // context on every call, so it returns null before the player window exists
    // and must be null-checked rather than cached.
    virtual CKContext *GetCKContext() = 0;
    virtual CKRenderContext *GetRenderContext() = 0;

    // Broadcasts OnExitGame to every Mod, then closes the game one frame later,
    // so this returns to its caller and the frame finishes normally.
    virtual void ExitGame() = 0;

    // The game's own managers, looked up once when the loader starts. They are
    // shared with the game, so a Mod that changes manager state is changing what
    // the game's scripts read.
    virtual CKAttributeManager *GetAttributeManager() = 0;
    virtual CKBehaviorManager *GetBehaviorManager() = 0;
    virtual CKCollisionManager *GetCollisionManager() = 0;

    // Not the CKInputManager. It is the loader's InputHook wrapper, which adds
    // per-device blocking and key edge detection on top of that manager. See
    // InputHook.h.
    virtual InputHook *GetInputManager() = 0;
    virtual CKMessageManager *GetMessageManager() = 0;
    virtual CKPathManager *GetPathManager() = 0;
    virtual CKParameterManager *GetParameterManager() = 0;
    virtual CKRenderManager *GetRenderManager() = 0;
    virtual CKSoundManager *GetSoundManager() = 0;
    virtual CKTimeManager *GetTimeManager() = 0;

    // The CKDWORD overloads count frames; the float overloads count
    // milliseconds. The two units share a name, so an unsuffixed integer
    // literal is ambiguous. Write AddTimer(1ul, ...) for frames and
    // AddTimer(1.0f, ...) for milliseconds.
    // The loop callbacks keep running while they return true.
    //
    // The callback runs on the game thread during the loader's per-frame pump,
    // just before IMod::OnProcess, so a timer never fires inside the call that
    // scheduled it. Nothing is returned, so there is no handle to cancel with: a
    // loop stops only by returning false, and a one-shot cannot be called off at
    // all. A callback that throws is dropped rather than retried. Scheduling is
    // silently ignored once the loader has begun shutting down, which includes
    // calls made from OnUnload. The loader cancels whatever is still pending after
    // the last OnUnload has run, so a queued callback cannot fire after that.
    virtual void AddTimer(CKDWORD delay, std::function<void()> callback) = 0;
    virtual void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) = 0;
    virtual void AddTimer(float delay, std::function<void()> callback) = 0;
    virtual void AddTimerLoop(float delay, std::function<bool()> callback) = 0;

    // One flag shared by every Mod and by the loader's own cheat commands, not a
    // per-Mod setting. EnableCheat broadcasts OnCheatEnabled only when the value
    // actually changes, so setting it to what it already is notifies nobody.
    virtual bool IsCheatEnabled() = 0;
    virtual void EnableCheat(bool enable) = 0;

    // Appends one line to the loader's ingame message list, the same list the
    // command bar prints to and the same one UI::AddMessage writes to. Older lines
    // scroll off on their own. A null message is treated as an empty line.
    virtual void SendIngameMessage(const char *msg) = 0;

    // The loader stores the raw pointer and never deletes it. Keep the command
    // alive while it is registered. This interface has no matching virtual slot,
    // but BML_UnregisterCommand in BML.h removes a command without changing this
    // frozen vtable. A Mod may unregister during OnUnload and delete the command
    // only after BML_UnregisterCommand returns BML_OK. A command that is not
    // unregistered must remain alive for the whole process lifetime.
    // Registration is silent on success and only logs on failure. It fails for
    // a null command, an invalid name or alias, and an already registered name.
    virtual void RegisterCommand(ICommand *cmd) = 0;

    // SetIC records the object's current state as the initial condition of the
    // current scene, and RestoreIC puts the object back to whatever was recorded
    // there, which is what the game does when a level resets. RestoreIC on an
    // object with nothing recorded does nothing at all rather than failing.
    // hierarchy walks the 2D and 3D children as well. A null object is ignored.
    virtual void SetIC(CKBeObject *obj, bool hierarchy = false) = 0;
    virtual void RestoreIC(CKBeObject *obj, bool hierarchy = false) = 0;

    // CKBeObject::Show plus the same optional walk over the children.
    virtual void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy = false) = 0;

    // IsIngame is true from the start of a level until the game is left, through
    // resets and pauses. IsPaused is the pause state on its own. IsPlaying is
    // exactly IsIngame and not IsPaused, so it does not narrow to the part of a
    // level where the ball actually moves.
    //
    // The loader updates these after it has broadcast the matching callback, so
    // inside OnStartLevel IsIngame is still false, and inside OnPostExitLevel it is
    // still true. In a callback, act on the callback rather than reading the state
    // back.
    virtual bool IsIngame() = 0;
    virtual bool IsPaused() = 0;
    virtual bool IsPlaying() = 0;

    // One CKContext::GetObjectByNameAndClass lookup per call, filtered to the
    // class in the function name, returning null when nothing matches. Nothing is
    // cached, and the returned object is the game's, so it dies with the level:
    // look it up again after a level load rather than holding the pointer. The
    // IMC counterpart is Scene::FindObject, which hands out references that survive
    // deletion safely.
    virtual CKDataArray *GetArrayByName(const char *name) = 0;
    virtual CKGroup *GetGroupByName(const char *name) = 0;
    virtual CKMaterial *GetMaterialByName(const char *name) = 0;
    virtual CKMesh *GetMeshByName(const char *name) = 0;
    virtual CK2dEntity *Get2dEntityByName(const char *name) = 0;
    virtual CK3dEntity *Get3dEntityByName(const char *name) = 0;
    virtual CK3dObject *Get3dObjectByName(const char *name) = 0;
    virtual CKCamera *GetCameraByName(const char *name) = 0;
    virtual CKTargetCamera *GetTargetCameraByName(const char *name) = 0;
    virtual CKLight *GetLightByName(const char *name) = 0;
    virtual CKTargetLight *GetTargetLightByName(const char *name) = 0;
    virtual CKSound *GetSoundByName(const char *name) = 0;
    virtual CKTexture *GetTextureByName(const char *name) = 0;
    virtual CKBehavior *GetScriptByName(const char *name) = 0;

    // Declares a new ball type, floor type, or modul. Nothing happens in the scene
    // when these are called: the loader only records the entry, and writes it into
    // the game's own physics and sound tables later, while the game is loading
    // Balls.nmo, Levelinit.nmo, and Sound.nmo. Call them from OnLoad, because an
    // entry recorded after those files have loaded is never written anywhere. Every
    // string is copied, so the caller does not have to keep it alive.
    virtual void RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                  float friction, float elasticity, float mass, const char *collGroup, float linearDamp,
                                  float rotDamp, float force, float radius) = 0;
    virtual void RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                   const char *collGroup, bool enableColl) = 0;
    virtual void RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                   const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                   float linearDamp, float rotDamp, float radius) = 0;
    virtual void RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                     const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                     float linearDamp, float rotDamp) = 0;
    virtual void RegisterTrafo(const char *modulName) = 0;
    virtual void RegisterModul(const char *modulName) = 0;

    // Every registered Mod, including the loader's own built-in ones and any script
    // Mods. GetMod returns null for an index outside 0 to GetModCount() - 1. Prefer
    // FindMod when a specific Mod is wanted; walk the list only to enumerate.
    virtual int GetModCount() = 0;
    virtual IMod *GetMod(int index) = 0;

    // GetSRScore is the speedrun timer in milliseconds, despite the name, not a
    // score and not in seconds. GetHSScore is the highscore the game would award,
    // computed from the Energy array as points plus 200 per remaining life, so it
    // reads 0 outside a level. Both return 0 when the values are unavailable rather
    // than reporting an error.
    virtual float GetSRScore() = 0;
    virtual int GetHSScore() = 0;

    // Drops the next frame's rendering and turns it back on one frame later, which
    // is what to call before work that would otherwise be seen half-finished. It is
    // not a counter: two Mods calling it in the same frame still get exactly one
    // frame skipped.
    virtual void SkipRenderForNextTick() = 0;

    // The loader's command table, which holds the commands of every Mod, not just
    // the caller's. FindCommand matches the name case-insensitively and also matches
    // a registered alias, returning null when there is no such command. The index
    // GetCommand takes is a position in that shared table, which the loader sorts by
    // name once every Mod has loaded and re-sorts when a script Mod reloads, so look
    // an index up and use it, do not store it.
    virtual int GetCommandCount() const = 0;
    virtual ICommand *GetCommand(int index) const = 0;
    virtual ICommand *FindCommand(const char *name) const = 0;

    // By the id a Mod reports from GetID, which is an exact match. Null when no Mod
    // has that id, which is also how to test whether an optional dependency is
    // present.
    virtual IMod *FindMod(const char *id) const = 0;

    // Runs a whole command line, the same way the command bar does, so cmd includes
    // the command name and its arguments. It reports an unknown command, an empty
    // line, and a cheat command refused because cheats are off by writing an ingame
    // message, and returns nothing either way, so a caller cannot tell success from
    // failure. Call FindCommand first when that matters.
    virtual void ExecuteCommand(const char *cmd) = 0;

    // Declares that mod needs another Mod, at dependencyId's version or newer. The
    // check that actually gates loading runs before OnLoad, so registering from
    // OnLoad is too late to protect the Mod that registers: do it in the Mod
    // constructor. IMod::AddDependency and AddOptionalDependency wrap these and
    // pass the right mod pointer.
    //
    // A required dependency has to be installed at all, or the loader gives up on
    // the whole init pass and no Mod loads, the caller included. A dependency cycle
    // does the same. A required dependency that is installed but too old, or that
    // failed to load, is milder: only the Mod that declared it is skipped. An
    // optional dependency never blocks anything and only pulls the other Mod ahead
    // of this one in load order, which it also does for a required one. Both return
    // BML_OK, or BML_ERROR_FAIL for a null argument or an allocation failure.
    virtual int RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) = 0;
    virtual int RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) = 0;

    // Note the return values do not agree across this group. CheckDependencies
    // answers 1 when every required dependency is installed and new enough and 0
    // otherwise, and never returns a BML_ERROR_ code. GetDependencyCount returns the
    // count, or -1 for a null Mod. GetDependencyInfo and ClearDependencies do use
    // BML_OK and the BML_ERROR_ codes, with GetDependencyInfo returning
    // BML_ERROR_NOT_FOUND for an index past the end.
    //
    // GetDependencyInfo copies into dependencyId at most idSize - 1 bytes and always
    // terminates, so a short buffer silently truncates. Every out parameter is
    // optional and may be null.
    virtual int CheckDependencies(IMod *mod) const = 0;
    virtual int GetDependencyCount(IMod *mod) const = 0;
    virtual int GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                                  int *major, int *minor, int *patch, int *optional) const = 0;
    virtual int ClearDependencies(IMod *mod) = 0;
};

#endif // BML_IBML_H
