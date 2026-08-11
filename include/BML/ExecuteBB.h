// Building blocks called from C++. Some of what Ballance does exists only as a Virtools
// building block, physicalizing an object and loading an NMO among them, with no function
// in the SDK behind it, and the way to get at those from a Mod is to build the block and
// run it. That is what this namespace does, and it comes in two halves.
//
// The functions with a plain name do the thing at once. Each keeps one block of its own,
// created by the loader inside the game's Level_Init script while the game starts, and a
// call writes that block's parameters and executes it there and then, before returning.
// So the work happens outside any script's flow, on the thread that called, which has to
// be the game thread. There is one block per action for the whole loader, so these are
// not reentrant: do not call one from inside a callback of the same one, and do not call
// them from a thread of the Mod's own. Before the loader has built them, which is
// anything earlier than the game's own scripts loading, they do nothing at all and
// report nothing.
//
// The Create functions build a block instead of running one, inside the script passed as
// the first argument, with every parameter given a source of its own, and hand it back
// for the Mod to use: wire it into the script, or set what is wanted and call
// ActivateInput then Execute as the direct functions do. The block belongs to the script
// from then on, meaning it is destroyed with the level, and ScriptHelper.h is what reads
// and writes its parameters afterwards.
//
// CreateHookBlock is the odd one and the most useful: it makes a block that calls a
// plain C function of the Mod's when the script reaches it, with as many input and
// output pins as asked for. Inserting one into a game script is how the loader gets its
// own callbacks, IMessageReceiver among them, so a Mod can hook a place the loader does
// not cover. The callback runs inside the script's execution, so it is as brief as a
// building block has to be.
//
// ObjectLoad returns the array of loaded objects together with the master object, and
// that array belongs to the block: it is filled again by the next call, so copy out of it
// before doing anything else, and never free it. With rename set, which is the default,
// every loaded object gets _BMLLoad_<n> put after its name, so a map's objects cannot
// collide with the level's.
//
// FontType names the game's own menu fonts rather than a font file, and the loader fills
// in which is which while the game starts, so a font asked for before that draws as
// NOFONT. The TEXT_ and ALIGN_ macros above are the 2D Text block's flags and alignment
// as Virtools numbers them, for Create2DText and for BGui::Label.
#ifndef BML_EXECUTEBB_H
#define BML_EXECUTEBB_H

#include <utility>

#include "CKAll.h"

#include "BML/Defines.h"

#define TEXT_SCREEN        1
#define TEXT_BACKGROUND    2
#define TEXT_CLIP          4
#define TEXT_RESIZE_VERT   8
#define TEXT_RESIZE_HORI   16
#define TEXT_WORDWRAP      32
#define TEXT_JUSTIFIED     64
#define TEXT_COMPILED      128
#define TEXT_MULTIPLE      256
#define TEXT_SHOWCARET     512
#define TEXT_3D            1024
#define TEXT_SCREENCLIP    2048

#define ALIGN_CENTER       0
#define ALIGN_LEFT         1
#define ALIGN_RIGHT        2
#define ALIGN_TOP          4
#define ALIGN_TOPLEFT      5
#define ALIGN_TOPRIGHT     6
#define ALIGN_BOTTOM       8
#define ALIGN_BOTTOMLEFT   9
#define ALIGN_BOTTOMRIGHT  10

namespace ExecuteBB {
    enum FontType {
        NOFONT,
        GAMEFONT_01,            // Normal
        GAMEFONT_02,            // Larger
        GAMEFONT_03,            // Small
        GAMEFONT_03A,           // Small, Gray
        GAMEFONT_04,            // Large
        GAMEFONT_CREDITS_SMALL,
        GAMEFONT_CREDITS_BIG
    };

    BML_EXPORT void PhysicalizeConvex(CK3dEntity *target = nullptr, CKBOOL fixed = false, float friction = 0.7f,
                                      float elasticity = 0.4f, float mass = 1.0f, const char *collGroup = "",
                                      CKBOOL startFrozen = false, CKBOOL enableColl = true,
                                      CKBOOL calcMassCenter = false, float linearDamp = 0.1f, float rotDamp = 0.1f,
                                      const char *collSurface = "", VxVector massCenter = VxVector(0, 0, 0),
                                      CKMesh *mesh = nullptr);

    BML_EXPORT void PhysicalizeBall(CK3dEntity *target = nullptr, CKBOOL fixed = false, float friction = 0.7f,
                                    float elasticity = 0.4f, float mass = 1.0f, const char *collGroup = "",
                                    CKBOOL startFrozen = false, CKBOOL enableColl = true, CKBOOL calcMassCenter = false,
                                    float linearDamp = 0.1f, float rotDamp = 0.1f, const char *collSurface = "",
                                    VxVector massCenter = VxVector(0, 0, 0), VxVector ballCenter = VxVector(0, 0, 0),
                                    float ballRadius = 2.0f);

    BML_EXPORT void PhysicalizeConcave(CK3dEntity *target = nullptr, CKBOOL fixed = false, float friction = 0.7f,
                                       float elasticity = 0.4f, float mass = 1.0f, const char *collGroup = "",
                                       CKBOOL startFrozen = false, CKBOOL enableColl = true,
                                       CKBOOL calcMassCenter = false, float linearDamp = 0.1f, float rotDamp = 0.1f,
                                       const char *collSurface = "", VxVector massCenter = VxVector(0, 0, 0),
                                       CKMesh *mesh = nullptr);

    BML_EXPORT void Unphysicalize(CK3dEntity *target);

    BML_EXPORT void SetPhysicsForce(CK3dEntity *target = nullptr,
                                    VxVector position = VxVector(0, 0, 0), CK3dEntity *posRef = nullptr,
                                    VxVector direction = VxVector(0, 0, 0), CK3dEntity *directionRef = nullptr,
                                    float force = 0.0f);

    BML_EXPORT void UnsetPhysicsForce(CK3dEntity *target = nullptr);

    BML_EXPORT void PhysicsImpulse(CK3dEntity *target = nullptr,
                                   VxVector position = VxVector(0, 0, 0), CK3dEntity *posRef = nullptr,
                                   VxVector direction = VxVector(0, 0, 0), CK3dEntity *dirRef = nullptr,
                                   float impulse = 0.0f);

    BML_EXPORT void PhysicsWakeUp(CK3dEntity *target = nullptr);

    BML_EXPORT ::std::pair<XObjectArray *, CKObject *> ObjectLoad(const char *file = "", bool rename = true,
                                                                  const char *mastername = "",
                                                                  CK_CLASSID filter = CKCID_3DOBJECT,
                                                                  CKBOOL addToScene = true, CKBOOL reuseMesh = true,
                                                                  CKBOOL reuseMtl = true, CKBOOL dynamic = true);

    BML_EXPORT CKBehavior *Create2DText(CKBehavior *script, CK2dEntity *target = nullptr, FontType font = NOFONT, const char *text = "",
                                        int align = ALIGN_CENTER, VxRect margin = {2, 2, 2, 2},
                                        Vx2DVector offset = {0, 0}, Vx2DVector pindent = {0, 0},
                                        CKMaterial *bgmat = nullptr,float caretsize = 0.1f,
                                        CKMaterial *caretmat = nullptr, int flags = TEXT_SCREEN);

    BML_EXPORT CKBehavior *CreatePhysicalizeConvex(CKBehavior *script, CK3dEntity *target = nullptr, CKBOOL fixed = false,
                                                   float friction = 0.7f, float elasticity = 0.4f, float mass = 1.0f,
                                                   const char *collGroup = "", CKBOOL startFrozen = false,
                                                   CKBOOL enableColl = true, CKBOOL calcMassCenter = false,
                                                   float linearDamp = 0.1f, float rotDamp = 0.1f,
                                                   const char *collSurface = "", VxVector massCenter = VxVector(0, 0, 0),
                                                   CKMesh *mesh = nullptr);

    BML_EXPORT CKBehavior *CreatePhysicalizeBall(CKBehavior *script, CK3dEntity *target = nullptr, CKBOOL fixed = false,
                                                 float friction = 0.7f, float elasticity = 0.4f, float mass = 1.0f,
                                                 const char *collGroup = "", CKBOOL startFrozen = false,
                                                 CKBOOL enableColl = true, CKBOOL calcMassCenter = false,
                                                 float linearDamp = 0.1f, float rotDamp = 0.1f,
                                                 const char *collSurface = "", VxVector massCenter = VxVector(0, 0, 0),
                                                 VxVector ballCenter = VxVector(0, 0, 0), float ballRadius = 2.0f);

    BML_EXPORT CKBehavior *CreatePhysicalizeConcave(CKBehavior *script, CK3dEntity *target = nullptr, CKBOOL fixed = false,
                                                    float friction = 0.7f, float elasticity = 0.4f, float mass = 1.0f,
                                                    const char *collGroup = "", CKBOOL startFrozen = false,
                                                    CKBOOL enableColl = true, CKBOOL calcMassCenter = false,
                                                    float linearDamp = 0.1f, float rotDamp = 0.1f,
                                                    const char *collSurface = "", VxVector massCenter = VxVector(0, 0, 0),
                                                    CKMesh *mesh = nullptr);

    BML_EXPORT CKBehavior *CreateSetPhysicsForce(CKBehavior *script, CK3dEntity *target = nullptr,
                                                 VxVector position = VxVector(0, 0, 0), CK3dEntity *posRef = nullptr,
                                                 VxVector direction = VxVector(0, 0, 0), CK3dEntity *directionRef = nullptr,
                                                 float force = 0.0f);

    BML_EXPORT CKBehavior *CreatePhysicsImpulse(CKBehavior *script, CK3dEntity *target = nullptr,
                                                VxVector position = VxVector(0, 0, 0), CK3dEntity *posRef = nullptr,
                                                VxVector direction = VxVector(0, 0, 0), CK3dEntity *dirRef = nullptr,
                                                float impulse = 0.0f);

    BML_EXPORT CKBehavior *CreatePhysicsWakeUp(CKBehavior *script, CK3dEntity *target = nullptr);

    BML_EXPORT CKBehavior *CreateObjectLoad(CKBehavior *script, const char *file = "", const char *mastername = "",
                                            CK_CLASSID filter = CKCID_3DOBJECT, CKBOOL addToScene = true,
                                            CKBOOL reuseMesh = true, CKBOOL reuseMtl = true, CKBOOL dynamic = true);

    BML_EXPORT CKBehavior *CreateSendMessage(CKBehavior *script, const char *msg, CKBeObject *dest);

    typedef int (*CKBehaviorCallback)(const CKBehaviorContext *behcontext, void *arg);
    BML_EXPORT CKBehavior *CreateHookBlock(CKBehavior *script, CKBehaviorCallback callback, void *arg = nullptr, int inCount = 1, int outCount = 1);
}

#endif // BML_EXECUTEBB_H