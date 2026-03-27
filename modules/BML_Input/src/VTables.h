/**
 * @file VTables.h
 * @brief CKInputManager VTable structure for BML_Input module
 *
 * Self-contained VTable definitions without macro dependencies.
 */

#ifndef BML_INPUT_VTABLES_H
#define BML_INPUT_VTABLES_H

#include "CKInputManager.h"

// CKBaseManager VTable
template <class T>
struct CKBaseManagerVTable {
    void (T::*Destructor)();
    CKStateChunk *(T::*SaveData)(CKFile *SavedFile);
    CKERROR (T::*LoadData)(CKStateChunk *chunk, CKFile *LoadedFile);
    CKERROR (T::*PreClearAll)();
    CKERROR (T::*PostClearAll)();
    CKERROR (T::*PreProcess)();
    CKERROR (T::*PostProcess)();
    CKERROR (T::*SequenceAddedToScene)(CKScene *scn, CK_ID *objids, int count);
    CKERROR (T::*SequenceRemovedFromScene)(CKScene *scn, CK_ID *objids, int count);
    CKERROR (T::*PreLaunchScene)(CKScene *OldScene, CKScene *NewScene);
    CKERROR (T::*PostLaunchScene)(CKScene *OldScene, CKScene *NewScene);
    CKERROR (T::*OnCKInit)();
    CKERROR (T::*OnCKEnd)();
    CKERROR (T::*OnCKReset)();
    CKERROR (T::*OnCKPostReset)();
    CKERROR (T::*OnCKPause)();
    CKERROR (T::*OnCKPlay)();
    CKERROR (T::*SequenceToBeDeleted)(CK_ID *objids, int count);
    CKERROR (T::*SequenceDeleted)(CK_ID *objids, int count);
    CKERROR (T::*PreLoad)();
    CKERROR (T::*PostLoad)();
    CKERROR (T::*PreSave)();
    CKERROR (T::*PostSave)();
    CKERROR (T::*OnPreCopy)(CKDependenciesContext &context);
    CKERROR (T::*OnPostCopy)(CKDependenciesContext &context);
    CKERROR (T::*OnPreRender)(CKRenderContext *dev);
    CKERROR (T::*OnPostRender)(CKRenderContext *dev);
    CKERROR (T::*OnPostSpriteRender)(CKRenderContext *dev);
    int (T::*GetFunctionPriority)(CKMANAGER_FUNCTIONS Function);
    CKDWORD (T::*GetValidFunctionsMask)();
};

// CKInputManager VTable (extends CKBaseManager)
template <class T>
struct CKInputManagerVTable : public CKBaseManagerVTable<CKInputManager> {
    void (T::*EnableKeyboardRepetition)(CKBOOL iEnable);
    CKBOOL (T::*IsKeyboardRepetitionEnabled)();
    CKBOOL (T::*IsKeyDown)(CKDWORD iKey, CKDWORD *oStamp);
    CKBOOL (T::*IsKeyUp)(CKDWORD iKey);
    CKBOOL (T::*IsKeyToggled)(CKDWORD iKey, CKDWORD *oStamp);
    void (T::*GetKeyName)(CKDWORD iKey, CKSTRING oKeyName);
    CKDWORD (T::*GetKeyFromName)(CKSTRING iKeyName);
    unsigned char *(T::*GetKeyboardState)();
    CKBOOL (T::*IsKeyboardAttached)();
    int (T::*GetNumberOfKeyInBuffer)();
    int (T::*GetKeyFromBuffer)(int i, CKDWORD &oKey, CKDWORD *oTimeStamp);
    CKBOOL (T::*IsMouseButtonDown)(CK_MOUSEBUTTON iButton);
    CKBOOL (T::*IsMouseClicked)(CK_MOUSEBUTTON iButton);
    CKBOOL (T::*IsMouseToggled)(CK_MOUSEBUTTON iButton);
    void (T::*GetMouseButtonsState)(CKBYTE oStates[4]);
    void (T::*GetMousePosition)(Vx2DVector &oPosition, CKBOOL iAbsolute);
    void (T::*GetMouseRelativePosition)(VxVector &oPosition);
    CKBOOL (T::*IsMouseAttached)();
    CKBOOL (T::*IsJoystickAttached)(int iJoystick);
    void (T::*GetJoystickPosition)(int iJoystick, VxVector *oPosition);
    void (T::*GetJoystickRotation)(int iJoystick, VxVector *oRotation);
    void (T::*GetJoystickSliders)(int iJoystick, Vx2DVector *oPosition);
    void (T::*GetJoystickPointOfViewAngle)(int iJoystick, float *oAngle);
    CKDWORD (T::*GetJoystickButtonsState)(int iJoystick);
    CKBOOL (T::*IsJoystickButtonDown)(int iJoystick, int iButton);
    void (T::*Pause)(CKBOOL pause);
    void (T::*ShowCursor)(CKBOOL iShow);
    CKBOOL (T::*GetCursorVisibility)();
    VXCURSOR_POINTER (T::*GetSystemCursor)();
    void (T::*SetSystemCursor)(VXCURSOR_POINTER cursor);
};

#endif // BML_INPUT_VTABLES_H
