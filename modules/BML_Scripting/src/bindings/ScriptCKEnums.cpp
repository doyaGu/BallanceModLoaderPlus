#include "ScriptCKEnums.h"

#include <cassert>

#include "CKAll.h"

void RegisterCKEnums(asIScriptEngine *engine) {
    int r = 0;

    // VX_EFFECT
    r = engine->RegisterEnum("VX_EFFECT"); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_NONE", VXEFFECT_NONE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_TEXGEN", VXEFFECT_TEXGEN); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_TEXGENREF", VXEFFECT_TEXGENREF); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_BUMPENV", VXEFFECT_BUMPENV); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_DP3", VXEFFECT_DP3); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_2TEXTURES", VXEFFECT_2TEXTURES); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_3TEXTURES", VXEFFECT_3TEXTURES); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECT", "VXEFFECT_MASK", VXEFFECT_MASK); assert(r >= 0);

    // VX_EFFECTTEXGEN
    r = engine->RegisterEnum("VX_EFFECTTEXGEN"); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGNONE", VXEFFECT_TGNONE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGTRANSFORM", VXEFFECT_TGTRANSFORM); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGREFLECT", VXEFFECT_TGREFLECT); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGCHROME", VXEFFECT_TGCHROME); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGPLANAR", VXEFFECT_TGPLANAR); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGCUBEMAP_REFLECT", VXEFFECT_TGCUBEMAP_REFLECT); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGCUBEMAP_SKYMAP", VXEFFECT_TGCUBEMAP_SKYMAP); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGCUBEMAP_NORMALS", VXEFFECT_TGCUBEMAP_NORMALS); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTTEXGEN", "VXEFFECT_TGCUBEMAP_POSITIONS", VXEFFECT_TGCUBEMAP_POSITIONS); assert(r >= 0);

    // VX_EFFECTCALLBACK_RETVAL
    r = engine->RegisterEnum("VX_EFFECTCALLBACK_RETVAL"); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTCALLBACK_RETVAL", "VXEFFECTRETVAL_SKIPNONE", VXEFFECTRETVAL_SKIPNONE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTCALLBACK_RETVAL", "VXEFFECTRETVAL_SKIPTEXMAT", VXEFFECTRETVAL_SKIPTEXMAT); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTCALLBACK_RETVAL", "VXEFFECTRETVAL_SKIPALLTEX", VXEFFECTRETVAL_SKIPALLTEX); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_EFFECTCALLBACK_RETVAL", "VXEFFECTRETVAL_SKIPALL", VXEFFECTRETVAL_SKIPALL); assert(r >= 0);

    // CK_UICALLBACK_REASON
    r = engine->RegisterEnum("CK_UICALLBACK_REASON"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_LOADSAVEPROGRESS", CKUIM_LOADSAVEPROGRESS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_DEBUGMESSAGESEND", CKUIM_DEBUGMESSAGESEND); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_OUTTOCONSOLE", CKUIM_OUTTOCONSOLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_OUTTOINFOBAR", CKUIM_OUTTOINFOBAR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_SHOWSETUP", CKUIM_SHOWSETUP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_SELECT", CKUIM_SELECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_CHOOSEOBJECT", CKUIM_CHOOSEOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_EDITOBJECT", CKUIM_EDITOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_CREATEINTERFACECHUNK", CKUIM_CREATEINTERFACECHUNK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_COPYOBJECTS", CKUIM_COPYOBJECTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_PASTEOBJECTS", CKUIM_PASTEOBJECTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_SCENEADDEDTOLEVEL", CKUIM_SCENEADDEDTOLEVEL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_REFRESHBUILDINGBLOCKS", CKUIM_REFRESHBUILDINGBLOCKS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_UICALLBACK_REASON", "CKUIM_DOMESSAGELOOP", CKUIM_DOMESSAGELOOP); assert(r >= 0);

    // CK_OBJECT_FLAGS
    r = engine->RegisterEnum("CK_OBJECT_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_INTERFACEOBJ", CK_OBJECT_INTERFACEOBJ); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_PRIVATE", CK_OBJECT_PRIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_INTERFACEMARK", CK_OBJECT_INTERFACEMARK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_FREEID", CK_OBJECT_FREEID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_TOBEDELETED", CK_OBJECT_TOBEDELETED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_NOTTOBESAVED", CK_OBJECT_NOTTOBESAVED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_VISIBLE", CK_OBJECT_VISIBLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_NAMESHARED", CK_OBJECT_NAMESHARED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_DYNAMIC", CK_OBJECT_DYNAMIC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_HIERACHICALHIDE", CK_OBJECT_HIERACHICALHIDE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_UPTODATE", CK_OBJECT_UPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_TEMPMARKER", CK_OBJECT_TEMPMARKER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_ONLYFORFILEREFERENCE", CK_OBJECT_ONLYFORFILEREFERENCE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_NOTTOBEDELETED", CK_OBJECT_NOTTOBEDELETED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_APPDATA", CK_OBJECT_APPDATA); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_SINGLEACTIVITY", CK_OBJECT_SINGLEACTIVITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_LOADSKIPBEOBJECT", CK_OBJECT_LOADSKIPBEOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_NOTTOBELISTEDANDSAVED", CK_OBJECT_NOTTOBELISTEDANDSAVED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETEROUT_SETTINGS", CK_PARAMETEROUT_SETTINGS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETEROUT_PARAMOP", CK_PARAMETEROUT_PARAMOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETERIN_DISABLED", CK_PARAMETERIN_DISABLED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETERIN_THIS", CK_PARAMETERIN_THIS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETERIN_SHARED", CK_PARAMETERIN_SHARED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_PARAMETEROUT_DELETEAFTERUSE", CK_PARAMETEROUT_DELETEAFTERUSE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_PARAMMASK", CK_OBJECT_PARAMMASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_BEHAVIORIO_IN", CK_BEHAVIORIO_IN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_BEHAVIORIO_OUT", CK_BEHAVIORIO_OUT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_BEHAVIORIO_ACTIVE", CK_BEHAVIORIO_ACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_IOTYPEMASK", CK_OBJECT_IOTYPEMASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_IOMASK", CK_OBJECT_IOMASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CKBEHAVIORLINK_RESERVED", CKBEHAVIORLINK_RESERVED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CKBEHAVIORLINK_ACTIVATEDLASTFRAME", CKBEHAVIORLINK_ACTIVATEDLASTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_FLAGS", "CK_OBJECT_BEHAVIORLINKMASK", CK_OBJECT_BEHAVIORLINKMASK); assert(r >= 0);

    // CK_3DENTITY_FLAGS
    r = engine->RegisterEnum("CK_3DENTITY_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_DUMMY", CK_3DENTITY_DUMMY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_FRAME", CK_3DENTITY_FRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_RESERVED0", CK_3DENTITY_RESERVED0); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_TARGETLIGHT", CK_3DENTITY_TARGETLIGHT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_TARGETCAMERA", CK_3DENTITY_TARGETCAMERA); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_IGNOREANIMATION", CK_3DENTITY_IGNOREANIMATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_HIERARCHICALOBSTACLE", CK_3DENTITY_HIERARCHICALOBSTACLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_UPDATELASTFRAME", CK_3DENTITY_UPDATELASTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_CAMERAIGNOREASPECT", CK_3DENTITY_CAMERAIGNOREASPECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_DISABLESKINPROCESS", CK_3DENTITY_DISABLESKINPROCESS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_ENABLESKINOFFSET", CK_3DENTITY_ENABLESKINOFFSET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_PLACEVALID", CK_3DENTITY_PLACEVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_PARENTVALID", CK_3DENTITY_PARENTVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_IKJOINTVALID", CK_3DENTITY_IKJOINTVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_PORTAL", CK_3DENTITY_PORTAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_ZORDERVALID", CK_3DENTITY_ZORDERVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_3DENTITY_FLAGS", "CK_3DENTITY_CHARACTERDOPROCESS", CK_3DENTITY_CHARACTERDOPROCESS); assert(r >= 0);

    // VX_MOVEABLE_FLAGS
    r = engine->RegisterEnum("VX_MOVEABLE_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_PICKABLE", VX_MOVEABLE_PICKABLE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_VISIBLE", VX_MOVEABLE_VISIBLE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_UPTODATE", VX_MOVEABLE_UPTODATE); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_RENDERCHANNELS", VX_MOVEABLE_RENDERCHANNELS); assert(r >= 0);
#endif
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_USERBOX", VX_MOVEABLE_USERBOX); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_EXTENTSUPTODATE", VX_MOVEABLE_EXTENTSUPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_BOXVALID", VX_MOVEABLE_BOXVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_RENDERLAST", VX_MOVEABLE_RENDERLAST); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_HASMOVED", VX_MOVEABLE_HASMOVED); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_WORLDALIGNED", VX_MOVEABLE_WORLDALIGNED); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_NOZBUFFERWRITE", VX_MOVEABLE_NOZBUFFERWRITE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_RENDERFIRST", VX_MOVEABLE_RENDERFIRST); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_NOZBUFFERTEST", VX_MOVEABLE_NOZBUFFERTEST); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_INVERSEWORLDMATVALID", VX_MOVEABLE_INVERSEWORLDMATVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_DONTUPDATEFROMPARENT", VX_MOVEABLE_DONTUPDATEFROMPARENT); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_INDIRECTMATRIX", VX_MOVEABLE_INDIRECTMATRIX); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_ZBUFONLY", VX_MOVEABLE_ZBUFONLY); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_STENCILONLY", VX_MOVEABLE_STENCILONLY); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_HIERARCHICALHIDE", VX_MOVEABLE_HIERARCHICALHIDE); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_CHARACTERRENDERED", VX_MOVEABLE_CHARACTERRENDERED); assert(r >= 0);
    r = engine->RegisterEnumValue("VX_MOVEABLE_FLAGS", "VX_MOVEABLE_RESERVED2", VX_MOVEABLE_RESERVED2); assert(r >= 0);

    // VXMESH_FLAGS
    r = engine->RegisterEnum("VXMESH_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_BOUNDINGUPTODATE", VXMESH_BOUNDINGUPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_VISIBLE", VXMESH_VISIBLE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_OPTIMIZED", VXMESH_OPTIMIZED); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_RENDERCHANNELS", VXMESH_RENDERCHANNELS); assert(r >= 0);
#endif
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_HASTRANSPARENCY", VXMESH_HASTRANSPARENCY); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_PRELITMODE", VXMESH_PRELITMODE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_WRAPU", VXMESH_WRAPU); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_WRAPV", VXMESH_WRAPV); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_FORCETRANSPARENCY", VXMESH_FORCETRANSPARENCY); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_TRANSPARENCYUPTODATE", VXMESH_TRANSPARENCYUPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_UV_CHANGED", VXMESH_UV_CHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_NORMAL_CHANGED", VXMESH_NORMAL_CHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_COLOR_CHANGED", VXMESH_COLOR_CHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_POS_CHANGED", VXMESH_POS_CHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_HINTDYNAMIC", VXMESH_HINTDYNAMIC); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_GENNORMALS", VXMESH_GENNORMALS); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_PROCEDURALUV", VXMESH_PROCEDURALUV); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_PROCEDURALPOS", VXMESH_PROCEDURALPOS); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_STRIPIFY", VXMESH_STRIPIFY); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_MONOMATERIAL", VXMESH_MONOMATERIAL); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_PM_BUILDNORM", VXMESH_PM_BUILDNORM); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_BWEIGHTS_CHANGED", VXMESH_BWEIGHTS_CHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_FLAGS", "VXMESH_ALLFLAGS", VXMESH_ALLFLAGS); assert(r >= 0);

    // CKAXIS
    r = engine->RegisterEnum("CKAXIS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKAXIS", "CKAXIS_X", CKAXIS_X); assert(r >= 0);
    r = engine->RegisterEnumValue("CKAXIS", "CKAXIS_Y", CKAXIS_Y); assert(r >= 0);
    r = engine->RegisterEnumValue("CKAXIS", "CKAXIS_Z", CKAXIS_Z); assert(r >= 0);

    // CKSUPPORT
    r = engine->RegisterEnum("CKSUPPORT"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSUPPORT", "CKSUPPORT_FRONT", CKSUPPORT_FRONT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSUPPORT", "CKSUPPORT_CENTER", CKSUPPORT_CENTER); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSUPPORT", "CKSUPPORT_BACK", CKSUPPORT_BACK); assert(r >= 0);

    // CKSPRITETEXT_ALIGNMENT
    r = engine->RegisterEnum("CKSPRITETEXT_ALIGNMENT"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_CENTER", CKSPRITETEXT_CENTER); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_LEFT", CKSPRITETEXT_LEFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_RIGHT", CKSPRITETEXT_RIGHT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_TOP", CKSPRITETEXT_TOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_BOTTOM", CKSPRITETEXT_BOTTOM); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_VCENTER", CKSPRITETEXT_VCENTER); assert(r >= 0);
    r = engine->RegisterEnumValue("CKSPRITETEXT_ALIGNMENT", "CKSPRITETEXT_HCENTER", CKSPRITETEXT_HCENTER); assert(r >= 0);

    // VXTEXTURE_WRAPMODE
    r = engine->RegisterEnum("VXTEXTURE_WRAPMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("VXTEXTURE_WRAPMODE", "VXTEXTUREWRAP_NONE", VXTEXTUREWRAP_NONE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXTEXTURE_WRAPMODE", "VXTEXTUREWRAP_U", VXTEXTUREWRAP_U); assert(r >= 0);
    r = engine->RegisterEnumValue("VXTEXTURE_WRAPMODE", "VXTEXTUREWRAP_V", VXTEXTUREWRAP_V); assert(r >= 0);
    r = engine->RegisterEnumValue("VXTEXTURE_WRAPMODE", "VXTEXTUREWRAP_UV", VXTEXTUREWRAP_UV); assert(r >= 0);

    // VXSPRITE3D_TYPE
    r = engine->RegisterEnum("VXSPRITE3D_TYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("VXSPRITE3D_TYPE", "VXSPRITE3D_BILLBOARD", VXSPRITE3D_BILLBOARD); assert(r >= 0);
    r = engine->RegisterEnumValue("VXSPRITE3D_TYPE", "VXSPRITE3D_XROTATE", VXSPRITE3D_XROTATE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXSPRITE3D_TYPE", "VXSPRITE3D_YROTATE", VXSPRITE3D_YROTATE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXSPRITE3D_TYPE", "VXSPRITE3D_ORIENTABLE", VXSPRITE3D_ORIENTABLE); assert(r >= 0);

    // VXMESH_LITMODE
    r = engine->RegisterEnum("VXMESH_LITMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_LITMODE", "VX_PRELITMESH", VX_PRELITMESH); assert(r >= 0);
    r = engine->RegisterEnumValue("VXMESH_LITMODE", "VX_LITMESH", VX_LITMESH); assert(r >= 0);

    // VXCHANNEL_FLAGS
    r = engine->RegisterEnum("VXCHANNEL_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_ACTIVE", VXCHANNEL_ACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_SAMEUV", VXCHANNEL_SAMEUV); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_NOTLIT", VXCHANNEL_NOTLIT); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_MONO", VXCHANNEL_MONO); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_RESERVED1", VXCHANNEL_RESERVED1); assert(r >= 0);
    r = engine->RegisterEnumValue("VXCHANNEL_FLAGS", "VXCHANNEL_LAST", VXCHANNEL_LAST); assert(r >= 0);

    // VxShadeType
    r = engine->RegisterEnum("VxShadeType"); assert(r >= 0);
    r = engine->RegisterEnumValue("VxShadeType", "WireFrame", WireFrame); assert(r >= 0);
    r = engine->RegisterEnumValue("VxShadeType", "FlatShading", FlatShading); assert(r >= 0);
    r = engine->RegisterEnumValue("VxShadeType", "GouraudShading", GouraudShading); assert(r >= 0);
    r = engine->RegisterEnumValue("VxShadeType", "PhongShading", PhongShading); assert(r >= 0);
    r = engine->RegisterEnumValue("VxShadeType", "MaterialDefault", MaterialDefault); assert(r >= 0);

    // CK_SCENE_FLAGS
    r = engine->RegisterEnum("CK_SCENE_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENE_FLAGS", "CK_SCENE_LAUNCHEDONCE", CK_SCENE_LAUNCHEDONCE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENE_FLAGS", "CK_SCENE_USEENVIRONMENTSETTINGS", CK_SCENE_USEENVIRONMENTSETTINGS); assert(r >= 0);

    // CK_BEHAVIOR_FLAGS
    r = engine->RegisterEnum("CK_BEHAVIOR_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_NONE", CKBEHAVIOR_NONE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_ACTIVE", CKBEHAVIOR_ACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_SCRIPT", CKBEHAVIOR_SCRIPT); assert(r >= 0);
    // r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_RESERVED1", CKBEHAVIOR_RESERVED1); assert(r >= 0); // Not in our SDK
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_USEFUNCTION", CKBEHAVIOR_USEFUNCTION); assert(r >= 0);
    // r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_RESERVED2", CKBEHAVIOR_RESERVED2); assert(r >= 0); // Not in our SDK
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_CUSTOMSETTINGSEDITDIALOG", CKBEHAVIOR_CUSTOMSETTINGSEDITDIALOG); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_WAITSFORMESSAGE", CKBEHAVIOR_WAITSFORMESSAGE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_VARIABLEINPUTS", CKBEHAVIOR_VARIABLEINPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_VARIABLEOUTPUTS", CKBEHAVIOR_VARIABLEOUTPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_VARIABLEPARAMETERINPUTS", CKBEHAVIOR_VARIABLEPARAMETERINPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS", CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_TOPMOST", CKBEHAVIOR_TOPMOST); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_BUILDINGBLOCK", CKBEHAVIOR_BUILDINGBLOCK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_MESSAGESENDER", CKBEHAVIOR_MESSAGESENDER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_MESSAGERECEIVER", CKBEHAVIOR_MESSAGERECEIVER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_TARGETABLE", CKBEHAVIOR_TARGETABLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_CUSTOMEDITDIALOG", CKBEHAVIOR_CUSTOMEDITDIALOG); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_RESERVED0", CKBEHAVIOR_RESERVED0); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_EXECUTEDLASTFRAME", CKBEHAVIOR_EXECUTEDLASTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_DEACTIVATENEXTFRAME", CKBEHAVIOR_DEACTIVATENEXTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_RESETNEXTFRAME", CKBEHAVIOR_RESETNEXTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_INTERNALLYCREATEDINPUTS", CKBEHAVIOR_INTERNALLYCREATEDINPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS", CKBEHAVIOR_INTERNALLYCREATEDOUTPUTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS", CKBEHAVIOR_INTERNALLYCREATEDINPUTPARAMS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS", CKBEHAVIOR_INTERNALLYCREATEDOUTPUTPARAMS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS", CKBEHAVIOR_INTERNALLYCREATEDLOCALPARAMS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_ACTIVATENEXTFRAME", CKBEHAVIOR_ACTIVATENEXTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_LOCKED", CKBEHAVIOR_LOCKED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_FLAGS", "CKBEHAVIOR_LAUNCHEDONCE", CKBEHAVIOR_LAUNCHEDONCE); assert(r >= 0);

    // CK_BEHAVIOR_CALLBACKMASK
    r = engine->RegisterEnum("CK_BEHAVIOR_CALLBACKMASK"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORPRESAVE", CKCB_BEHAVIORPRESAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORDELETE", CKCB_BEHAVIORDELETE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORATTACH", CKCB_BEHAVIORATTACH); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORDETACH", CKCB_BEHAVIORDETACH); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORPAUSE", CKCB_BEHAVIORPAUSE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORRESUME", CKCB_BEHAVIORRESUME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORCREATE", CKCB_BEHAVIORCREATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORRESET", CKCB_BEHAVIORRESET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORPOSTSAVE", CKCB_BEHAVIORPOSTSAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORLOAD", CKCB_BEHAVIORLOAD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIOREDITED", CKCB_BEHAVIOREDITED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORSETTINGSEDITED", CKCB_BEHAVIORSETTINGSEDITED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORREADSTATE", CKCB_BEHAVIORREADSTATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORNEWSCENE", CKCB_BEHAVIORNEWSCENE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORACTIVATESCRIPT", CKCB_BEHAVIORACTIVATESCRIPT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORDEACTIVATESCRIPT", CKCB_BEHAVIORDEACTIVATESCRIPT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORRESETINBREAKPOINT", CKCB_BEHAVIORRESETINBREAKPOINT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORBASE", CKCB_BEHAVIORBASE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORSAVELOAD", CKCB_BEHAVIORSAVELOAD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORPPR", CKCB_BEHAVIORPPR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIOREDITIONS", CKCB_BEHAVIOREDITIONS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_CALLBACKMASK", "CKCB_BEHAVIORALL", CKCB_BEHAVIORALL); assert(r >= 0);

    // CK_BEHAVIOR_RETURN
    r = engine->RegisterEnum("CK_BEHAVIOR_RETURN"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_OK", CKBR_OK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_ACTIVATENEXTFRAME", CKBR_ACTIVATENEXTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_ATTACHFAILED", CKBR_ATTACHFAILED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_DETACHFAILED", CKBR_DETACHFAILED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_LOCKED", CKBR_LOCKED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_INFINITELOOP", CKBR_INFINITELOOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_BREAK", CKBR_BREAK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_GENERICERROR", CKBR_GENERICERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_BEHAVIORERROR", CKBR_BEHAVIORERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_OWNERERROR", CKBR_OWNERERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_PARAMETERERROR", CKBR_PARAMETERERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_GENERICERROR_RETRY", CKBR_GENERICERROR_RETRY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_BEHAVIORERROR_RETRY", CKBR_BEHAVIORERROR_RETRY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_OWNERERROR_RETRY", CKBR_OWNERERROR_RETRY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_RETURN", "CKBR_PARAMETERERROR_RETRY", CKBR_PARAMETERERROR_RETRY); assert(r >= 0);

    // CK_BEHAVIOR_TYPE
    r = engine->RegisterEnum("CK_BEHAVIOR_TYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_TYPE", "CKBEHAVIORTYPE_BASE", CKBEHAVIORTYPE_BASE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_TYPE", "CKBEHAVIORTYPE_SCRIPT", CKBEHAVIORTYPE_SCRIPT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIOR_TYPE", "CKBEHAVIORTYPE_BEHAVIOR", CKBEHAVIORTYPE_BEHAVIOR); assert(r >= 0);

    // CK_2DENTITY_FLAGS
    r = engine->RegisterEnum("CK_2DENTITY_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_RESERVED3", CK_2DENTITY_RESERVED3); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_POSITIONCHANGED", CK_2DENTITY_POSITIONCHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_SIZECHANGED", CK_2DENTITY_SIZECHANGED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_USESIZE", CK_2DENTITY_USESIZE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_VIRTOOLS", CK_2DENTITY_VIRTOOLS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_USESRCRECT", CK_2DENTITY_USESRCRECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_BACKGROUND", CK_2DENTITY_BACKGROUND); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_NOTPICKABLE", CK_2DENTITY_NOTPICKABLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_RATIOOFFSET", CK_2DENTITY_RATIOOFFSET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_USEHOMOGENEOUSCOORD", CK_2DENTITY_USEHOMOGENEOUSCOORD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_CLIPTOCAMERAVIEW", CK_2DENTITY_CLIPTOCAMERAVIEW); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_UPDATEHOMOGENEOUSCOORD", CK_2DENTITY_UPDATEHOMOGENEOUSCOORD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_CLIPTOPARENT", CK_2DENTITY_CLIPTOPARENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_RESERVED0", CK_2DENTITY_RESERVED0); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_RESERVED1", CK_2DENTITY_RESERVED1); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_RESERVED2", CK_2DENTITY_RESERVED2); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_STICKLEFT", CK_2DENTITY_STICKLEFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_STICKRIGHT", CK_2DENTITY_STICKRIGHT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_STICKTOP", CK_2DENTITY_STICKTOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_2DENTITY_FLAGS", "CK_2DENTITY_STICKBOTTOM", CK_2DENTITY_STICKBOTTOM); assert(r >= 0);

    // CK_OBJECTANIMATION_FLAGS
    r = engine->RegisterEnum("CK_OBJECTANIMATION_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_IGNOREPOS", CK_OBJECTANIMATION_IGNOREPOS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_IGNOREROT", CK_OBJECTANIMATION_IGNOREROT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_IGNORESCALE", CK_OBJECTANIMATION_IGNORESCALE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_IGNOREMORPH", CK_OBJECTANIMATION_IGNOREMORPH); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_IGNORESCALEROT", CK_OBJECTANIMATION_IGNORESCALEROT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_MERGED", CK_OBJECTANIMATION_MERGED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_WARPER", CK_OBJECTANIMATION_WARPER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTANIMATION_FLAGS", "CK_OBJECTANIMATION_RESERVED", CK_OBJECTANIMATION_RESERVED); assert(r >= 0);

    // CK_ANIMATION_FLAGS
    r = engine->RegisterEnum("CK_ANIMATION_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_LINKTOFRAMERATE", CKANIMATION_LINKTOFRAMERATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_CANBEBREAK", CKANIMATION_CANBEBREAK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_ALLOWTURN", CKANIMATION_ALLOWTURN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_ALIGNORIENTATION", CKANIMATION_ALIGNORIENTATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARYWARPER", CKANIMATION_SECONDARYWARPER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SUBANIMSSORTED", CKANIMATION_SUBANIMSSORTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_FROMNOW", CKANIMATION_TRANSITION_FROMNOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_FROMWARPFROMCURRENT", CKANIMATION_TRANSITION_FROMWARPFROMCURRENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_TOSTART", CKANIMATION_TRANSITION_TOSTART); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_WARPTOSTART", CKANIMATION_TRANSITION_WARPTOSTART); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_WARPTOBEST", CKANIMATION_TRANSITION_WARPTOBEST); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_WARPTOSAMEPOS", CKANIMATION_TRANSITION_WARPTOSAMEPOS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_USEVELOCITY", CKANIMATION_TRANSITION_USEVELOCITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_LOOPIFEQUAL", CKANIMATION_TRANSITION_LOOPIFEQUAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_ONESHOT", CKANIMATION_SECONDARY_ONESHOT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_LOOP", CKANIMATION_SECONDARY_LOOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_LASTFRAME", CKANIMATION_SECONDARY_LASTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_LOOPNTIMES", CKANIMATION_SECONDARY_LOOPNTIMES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_DOWARP", CKANIMATION_SECONDARY_DOWARP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_TRANSITION_PRESET", CKANIMATION_TRANSITION_PRESET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_FLAGS", "CKANIMATION_SECONDARY_PRESET", CKANIMATION_SECONDARY_PRESET); assert(r >= 0);

    // CK_ANIMATION_TRANSITION_MODE
    r = engine->RegisterEnum("CK_ANIMATION_TRANSITION_MODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_FROMNOW", CK_TRANSITION_FROMNOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_FROMWARPFROMCURRENT", CK_TRANSITION_FROMWARPFROMCURRENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_TOSTART", CK_TRANSITION_TOSTART); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPTOSTART", CK_TRANSITION_WARPTOSTART); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPTOBEST", CK_TRANSITION_WARPTOBEST); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPTOSAMEPOS", CK_TRANSITION_WARPTOSAMEPOS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_USEVELOCITY", CK_TRANSITION_USEVELOCITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_LOOPIFEQUAL", CK_TRANSITION_LOOPIFEQUAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_FROMANIMATION", CK_TRANSITION_FROMANIMATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPSTART", CK_TRANSITION_WARPSTART); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPBEST", CK_TRANSITION_WARPBEST); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPSAMEPOS", CK_TRANSITION_WARPSAMEPOS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ANIMATION_TRANSITION_MODE", "CK_TRANSITION_WARPMASK", CK_TRANSITION_WARPMASK); assert(r >= 0);

    // CK_SECONDARYANIMATION_FLAGS
    r = engine->RegisterEnum("CK_SECONDARYANIMATION_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_ONESHOT", CKSECONDARYANIMATION_ONESHOT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_LOOP", CKSECONDARYANIMATION_LOOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_LASTFRAME", CKSECONDARYANIMATION_LASTFRAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_LOOPNTIMES", CKSECONDARYANIMATION_LOOPNTIMES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_DOWARP", CKSECONDARYANIMATION_DOWARP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SECONDARYANIMATION_FLAGS", "CKSECONDARYANIMATION_FROMANIMATION", CKSECONDARYANIMATION_FROMANIMATION); assert(r >= 0);

    // CK_RENDER_FLAGS
    r = engine->RegisterEnum("CK_RENDER_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_BACKGROUNDSPRITES", CK_RENDER_BACKGROUNDSPRITES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_FOREGROUNDSPRITES", CK_RENDER_FOREGROUNDSPRITES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_USECAMERARATIO", CK_RENDER_USECAMERARATIO); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_CLEARZ", CK_RENDER_CLEARZ); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_CLEARBACK", CK_RENDER_CLEARBACK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_CLEARSTENCIL", CK_RENDER_CLEARSTENCIL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_DOBACKTOFRONT", CK_RENDER_DOBACKTOFRONT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_DEFAULTSETTINGS", CK_RENDER_DEFAULTSETTINGS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_CLEARVIEWPORT", CK_RENDER_CLEARVIEWPORT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_WAITVBL", CK_RENDER_WAITVBL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_SKIPDRAWSCENE", CK_RENDER_SKIPDRAWSCENE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_DONOTUPDATEEXTENTS", CK_RENDER_DONOTUPDATEEXTENTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_SKIP3D", CK_RENDER_SKIP3D); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_OPTIONSMASK", CK_RENDER_OPTIONSMASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_PLAYERCONTEXT", CK_RENDER_PLAYERCONTEXT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RENDER_FLAGS", "CK_RENDER_USECURRENTSETTINGS", CK_RENDER_USECURRENTSETTINGS); assert(r >= 0);

    // CK_PARAMETERTYPE_FLAGS
    r = engine->RegisterEnum("CK_PARAMETERTYPE_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_VARIABLESIZE", CKPARAMETERTYPE_VARIABLESIZE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_RESERVED", CKPARAMETERTYPE_RESERVED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_HIDDEN", CKPARAMETERTYPE_HIDDEN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_FLAGS", CKPARAMETERTYPE_FLAGS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_STRUCT", CKPARAMETERTYPE_STRUCT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_ENUMS", CKPARAMETERTYPE_ENUMS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_USER", CKPARAMETERTYPE_USER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETERTYPE_FLAGS", "CKPARAMETERTYPE_NOENDIANCONV", CKPARAMETERTYPE_NOENDIANCONV); assert(r >= 0);

    // CK_SCENEOBJECTACTIVITY_FLAGS
    r = engine->RegisterEnum("CK_SCENEOBJECTACTIVITY_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTACTIVITY_FLAGS", "CK_SCENEOBJECTACTIVITY_SCENEDEFAULT", CK_SCENEOBJECTACTIVITY_SCENEDEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTACTIVITY_FLAGS", "CK_SCENEOBJECTACTIVITY_ACTIVATE", CK_SCENEOBJECTACTIVITY_ACTIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTACTIVITY_FLAGS", "CK_SCENEOBJECTACTIVITY_DEACTIVATE", CK_SCENEOBJECTACTIVITY_DEACTIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTACTIVITY_FLAGS", "CK_SCENEOBJECTACTIVITY_DONOTHING", CK_SCENEOBJECTACTIVITY_DONOTHING); assert(r >= 0);

    // CK_SCENEOBJECTRESET_FLAGS
    r = engine->RegisterEnum("CK_SCENEOBJECTRESET_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTRESET_FLAGS", "CK_SCENEOBJECTRESET_SCENEDEFAULT", CK_SCENEOBJECTRESET_SCENEDEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTRESET_FLAGS", "CK_SCENEOBJECTRESET_RESET", CK_SCENEOBJECTRESET_RESET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECTRESET_FLAGS", "CK_SCENEOBJECTRESET_DONOTHING", CK_SCENEOBJECTRESET_DONOTHING); assert(r >= 0);

    // CK_SCENEOBJECT_FLAGS
    r = engine->RegisterEnum("CK_SCENEOBJECT_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_START_ACTIVATE", CK_SCENEOBJECT_START_ACTIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_ACTIVE", CK_SCENEOBJECT_ACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_START_DEACTIVATE", CK_SCENEOBJECT_START_DEACTIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_START_LEAVE", CK_SCENEOBJECT_START_LEAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_START_RESET", CK_SCENEOBJECT_START_RESET); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SCENEOBJECT_FLAGS", "CK_SCENEOBJECT_INTERNAL_IC", CK_SCENEOBJECT_INTERNAL_IC); assert(r >= 0);

    // CK_ATTRIBUT_FLAGS
    r = engine->RegisterEnum("CK_ATTRIBUT_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_CAN_MODIFY", CK_ATTRIBUT_CAN_MODIFY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_CAN_DELETE", CK_ATTRIBUT_CAN_DELETE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_HIDDEN", CK_ATTRIBUT_HIDDEN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_DONOTSAVE", CK_ATTRIBUT_DONOTSAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_USER", CK_ATTRIBUT_USER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_SYSTEM", CK_ATTRIBUT_SYSTEM); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ATTRIBUT_FLAGS", "CK_ATTRIBUT_DONOTCOPY", CK_ATTRIBUT_DONOTCOPY); assert(r >= 0);

    // CK_BEHAVIORPROTOTYPE_FLAGS
    r = engine->RegisterEnum("CK_BEHAVIORPROTOTYPE_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIORPROTOTYPE_FLAGS", "CK_BEHAVIORPROTOTYPE_NORMAL", CK_BEHAVIORPROTOTYPE_NORMAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIORPROTOTYPE_FLAGS", "CK_BEHAVIORPROTOTYPE_HIDDEN", CK_BEHAVIORPROTOTYPE_HIDDEN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BEHAVIORPROTOTYPE_FLAGS", "CK_BEHAVIORPROTOTYPE_OBSOLETE", CK_BEHAVIORPROTOTYPE_OBSOLETE); assert(r >= 0);

    // CK_LOADMODE
    r = engine->RegisterEnum("CK_LOADMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOADMODE", "CKLOAD_INVALID", CKLOAD_INVALID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOADMODE", "CKLOAD_OK", CKLOAD_OK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOADMODE", "CKLOAD_REPLACE", CKLOAD_REPLACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOADMODE", "CKLOAD_RENAME", CKLOAD_RENAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOADMODE", "CKLOAD_USECURRENT", CKLOAD_USECURRENT); assert(r >= 0);

    // CK_OBJECTCREATION_OPTIONS
    r = engine->RegisterEnum("CK_OBJECTCREATION_OPTIONS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_NONAMECHECK", CK_OBJECTCREATION_NONAMECHECK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_REPLACE", CK_OBJECTCREATION_REPLACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_RENAME", CK_OBJECTCREATION_RENAME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_USECURRENT", CK_OBJECTCREATION_USECURRENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_ASK", CK_OBJECTCREATION_ASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_FLAGSMASK", CK_OBJECTCREATION_FLAGSMASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_DYNAMIC", CK_OBJECTCREATION_DYNAMIC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_ACTIVATE", CK_OBJECTCREATION_ACTIVATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECTCREATION_OPTIONS", "CK_OBJECTCREATION_NONAMECOPY", CK_OBJECTCREATION_NONAMECOPY); assert(r >= 0);

    // CK_DEPENDENCIES_OPMODE
    r = engine->RegisterEnum("CK_DEPENDENCIES_OPMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_COPY", CK_DEPENDENCIES_COPY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_DELETE", CK_DEPENDENCIES_DELETE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_REPLACE", CK_DEPENDENCIES_REPLACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_SAVE", CK_DEPENDENCIES_SAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_BUILD", CK_DEPENDENCIES_BUILD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_OPMODE", "CK_DEPENDENCIES_OPERATIONMODE", CK_DEPENDENCIES_OPERATIONMODE); assert(r >= 0);

    // CK_FILE_WRITEMODE
    r = engine->RegisterEnum("CK_FILE_WRITEMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FILE_WRITEMODE", "CKFILE_UNCOMPRESSED", CKFILE_UNCOMPRESSED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FILE_WRITEMODE", "CKFILE_CHUNKCOMPRESSED_OLD", CKFILE_CHUNKCOMPRESSED_OLD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FILE_WRITEMODE", "CKFILE_EXTERNALTEXTURES_OLD", CKFILE_EXTERNALTEXTURES_OLD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FILE_WRITEMODE", "CKFILE_FORVIEWER", CKFILE_FORVIEWER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FILE_WRITEMODE", "CKFILE_WHOLECOMPRESSED", CKFILE_WHOLECOMPRESSED); assert(r >= 0);

    // CK_BITMAP_SYSTEMCACHING
    r = engine->RegisterEnum("CK_BITMAP_SYSTEMCACHING"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAP_SYSTEMCACHING", "CKBITMAP_PROCEDURAL", CKBITMAP_PROCEDURAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAP_SYSTEMCACHING", "CKBITMAP_VIDEOSHADOW", CKBITMAP_VIDEOSHADOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAP_SYSTEMCACHING", "CKBITMAP_DISCARD", CKBITMAP_DISCARD); assert(r >= 0);

    // CK_TEXTURE_SAVEOPTIONS
    r = engine->RegisterEnum("CK_TEXTURE_SAVEOPTIONS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_TEXTURE_SAVEOPTIONS", "CKTEXTURE_RAWDATA", CKTEXTURE_RAWDATA); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_TEXTURE_SAVEOPTIONS", "CKTEXTURE_EXTERNAL", CKTEXTURE_EXTERNAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_TEXTURE_SAVEOPTIONS", "CKTEXTURE_IMAGEFORMAT", CKTEXTURE_IMAGEFORMAT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_TEXTURE_SAVEOPTIONS", "CKTEXTURE_USEGLOBAL", CKTEXTURE_USEGLOBAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_TEXTURE_SAVEOPTIONS", "CKTEXTURE_INCLUDEORIGINALFILE", CKTEXTURE_INCLUDEORIGINALFILE); assert(r >= 0);

    // CK_SOUND_SAVEOPTIONS
    r = engine->RegisterEnum("CK_SOUND_SAVEOPTIONS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUND_SAVEOPTIONS", "CKSOUND_EXTERNAL", CKSOUND_EXTERNAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUND_SAVEOPTIONS", "CKSOUND_INCLUDEORIGINALFILE", CKSOUND_INCLUDEORIGINALFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUND_SAVEOPTIONS", "CKSOUND_USEGLOBAL", CKSOUND_USEGLOBAL); assert(r >= 0);

    // CK_CONFIG_FLAGS
    r = engine->RegisterEnum("CK_CONFIG_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_CONFIG_FLAGS", "CK_CONFIG_DISABLEDSOUND", CK_CONFIG_DISABLEDSOUND); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_CONFIG_FLAGS", "CK_CONFIG_DISABLEDINPUT", CK_CONFIG_DISABLEDINPUT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_CONFIG_FLAGS", "CK_CONFIG_DOWARN", CK_CONFIG_DOWARN); assert(r >= 0);

    // CK_DESTROY_FLAGS
    r = engine->RegisterEnum("CK_DESTROY_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DESTROY_FLAGS", "CK_DESTROY_FREEID", CK_DESTROY_FREEID); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DESTROY_FLAGS", "CK_DESTROY_NONOTIFY", CK_DESTROY_NONOTIFY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DESTROY_FLAGS", "CK_DESTROY_TEMPOBJECT", CK_DESTROY_TEMPOBJECT); assert(r >= 0);

    // CK_WAVESOUND_TYPE
    r = engine->RegisterEnum("CK_WAVESOUND_TYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_TYPE", "CK_WAVESOUND_BACKGROUND", CK_WAVESOUND_BACKGROUND); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_TYPE", "CK_WAVESOUND_POINT", CK_WAVESOUND_POINT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_TYPE", "CK_WAVESOUND_CONE", CK_WAVESOUND_CONE); assert(r >= 0);

    // CK_WAVESOUND_LOCKMODE
    r = engine->RegisterEnum("CK_WAVESOUND_LOCKMODE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_LOCKMODE", "CK_WAVESOUND_LOCKFROMWRITE", CK_WAVESOUND_LOCKFROMWRITE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_LOCKMODE", "CK_WAVESOUND_LOCKENTIREBUFFER", CK_WAVESOUND_LOCKENTIREBUFFER); assert(r >= 0);

    // CK_WAVESOUND_STATE
    r = engine->RegisterEnum("CK_WAVESOUND_STATE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_ALLTYPE", CK_WAVESOUND_ALLTYPE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_LOOPED", CK_WAVESOUND_LOOPED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_FADEIN", CK_WAVESOUND_FADEIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_FADEOUT", CK_WAVESOUND_FADEOUT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_FADE", CK_WAVESOUND_FADE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_COULDBERESTARTED", CK_WAVESOUND_COULDBERESTARTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_NODUPLICATE", CK_WAVESOUND_NODUPLICATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_OCCLUSIONS", CK_WAVESOUND_OCCLUSIONS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_MONO", CK_WAVESOUND_MONO); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_WAVESOUND_STATE", "CK_WAVESOUND_FILESTREAMED", CK_WAVESOUND_FILESTREAMED); assert(r >= 0);

    // CK_GEOMETRICPRECISION
    r = engine->RegisterEnum("CK_GEOMETRICPRECISION"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GEOMETRICPRECISION", "CKCOLLISION_NONE", CKCOLLISION_NONE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GEOMETRICPRECISION", "CKCOLLISION_BOX", CKCOLLISION_BOX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GEOMETRICPRECISION", "CKCOLLISION_FACE", CKCOLLISION_FACE); assert(r >= 0);

    // CK_RAYINTERSECTION
    r = engine->RegisterEnum("CK_RAYINTERSECTION"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RAYINTERSECTION", "CKRAYINTERSECTION_DEFAULT", CKRAYINTERSECTION_DEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RAYINTERSECTION", "CKRAYINTERSECTION_SEGMENT", CKRAYINTERSECTION_SEGMENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RAYINTERSECTION", "CKRAYINTERSECTION_IGNOREALPHA", CKRAYINTERSECTION_IGNOREALPHA); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_RAYINTERSECTION", "CKRAYINTERSECTION_FIRSTCONTACT", CKRAYINTERSECTION_FIRSTCONTACT); assert(r >= 0);

    // CK_IMPACTINFO
    r = engine->RegisterEnum("CK_IMPACTINFO"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "OBSTACLETOUCHED", OBSTACLETOUCHED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "SUBOBSTACLETOUCHED", SUBOBSTACLETOUCHED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "TOUCHEDFACE", TOUCHEDFACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "TOUCHINGFACE", TOUCHINGFACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "TOUCHEDVERTEX", TOUCHEDVERTEX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "TOUCHINGVERTEX", TOUCHINGVERTEX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "IMPACTWORLDMATRIX", IMPACTWORLDMATRIX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "IMPACTPOINT", IMPACTPOINT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "IMPACTNORMAL", IMPACTNORMAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IMPACTINFO", "OWNERENTITY", OWNERENTITY); assert(r >= 0);

    // CK_FLOORGEOMETRY
    r = engine->RegisterEnum("CK_FLOORGEOMETRY"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FLOORGEOMETRY", "CKFLOOR_FACES", CKFLOOR_FACES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FLOORGEOMETRY", "CKFLOOR_BOX", CKFLOOR_BOX); assert(r >= 0);

    // CK_FLOORNEAREST
    r = engine->RegisterEnum("CK_FLOORNEAREST"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FLOORNEAREST", "CKFLOOR_NOFLOOR", CKFLOOR_NOFLOOR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FLOORNEAREST", "CKFLOOR_DOWN", CKFLOOR_DOWN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FLOORNEAREST", "CKFLOOR_UP", CKFLOOR_UP); assert(r >= 0);

    // CK_GRIDORIENTATION
    r = engine->RegisterEnum("CK_GRIDORIENTATION"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GRIDORIENTATION", "CKGRID_FREE", CKGRID_FREE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GRIDORIENTATION", "CKGRID_XZ", CKGRID_XZ); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GRIDORIENTATION", "CKGRID_XY", CKGRID_XY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_GRIDORIENTATION", "CKGRID_YZ", CKGRID_YZ); assert(r >= 0);

    // CKGRID_LAYER_FORMAT
    r = engine->RegisterEnum("CKGRID_LAYER_FORMAT"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKGRID_LAYER_FORMAT", "CKGRID_LAYER_FORMAT_NORMAL", CKGRID_LAYER_FORMAT_NORMAL); assert(r >= 0);

    // CK_BINARYOPERATOR
    r = engine->RegisterEnum("CK_BINARYOPERATOR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BINARYOPERATOR", "CKADD", CKADD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BINARYOPERATOR", "CKSUB", CKSUB); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BINARYOPERATOR", "CKMUL", CKMUL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BINARYOPERATOR", "CKDIV", CKDIV); assert(r >= 0);

    // CK_COMPOPERATOR
    r = engine->RegisterEnum("CK_COMPOPERATOR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKEQUAL", CKEQUAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKNOTEQUAL", CKNOTEQUAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKLESSER", CKLESSER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKLESSEREQUAL", CKLESSEREQUAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKGREATER", CKGREATER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_COMPOPERATOR", "CKGREATEREQUAL", CKGREATEREQUAL); assert(r >= 0);

    // CK_SETOPERATOR
    r = engine->RegisterEnum("CK_SETOPERATOR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SETOPERATOR", "CKUNION", CKUNION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SETOPERATOR", "CKINTERSECTION", CKINTERSECTION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SETOPERATOR", "CKSUBTRACTION", CKSUBTRACTION); assert(r >= 0);

    // CK_ARRAYTYPE
    r = engine->RegisterEnum("CK_ARRAYTYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ARRAYTYPE", "CKARRAYTYPE_INT", CKARRAYTYPE_INT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ARRAYTYPE", "CKARRAYTYPE_FLOAT", CKARRAYTYPE_FLOAT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ARRAYTYPE", "CKARRAYTYPE_STRING", CKARRAYTYPE_STRING); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ARRAYTYPE", "CKARRAYTYPE_OBJECT", CKARRAYTYPE_OBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_ARRAYTYPE", "CKARRAYTYPE_PARAMETER", CKARRAYTYPE_PARAMETER); assert(r >= 0);

    // CK_MOUSEBUTTON
    r = engine->RegisterEnum("CK_MOUSEBUTTON"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOUSEBUTTON", "CK_MOUSEBUTTON_LEFT", CK_MOUSEBUTTON_LEFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOUSEBUTTON", "CK_MOUSEBUTTON_RIGHT", CK_MOUSEBUTTON_RIGHT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOUSEBUTTON", "CK_MOUSEBUTTON_MIDDLE", CK_MOUSEBUTTON_MIDDLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOUSEBUTTON", "CK_MOUSEBUTTON_4", CK_MOUSEBUTTON_4); assert(r >= 0);

    // CK_LOAD_FLAGS
    r = engine->RegisterEnum("CK_LOAD_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_ANIMATION", CK_LOAD_ANIMATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_GEOMETRY", CK_LOAD_GEOMETRY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_DEFAULT", CK_LOAD_DEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_ASCHARACTER", CK_LOAD_ASCHARACTER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_DODIALOG", CK_LOAD_DODIALOG); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_AS_DYNAMIC_OBJECT", CK_LOAD_AS_DYNAMIC_OBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_AUTOMATICMODE", CK_LOAD_AUTOMATICMODE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_CHECKDUPLICATES", CK_LOAD_CHECKDUPLICATES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_CHECKDEPENDENCIES", CK_LOAD_CHECKDEPENDENCIES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_LOAD_FLAGS", "CK_LOAD_ONLYBEHAVIORS", CK_LOAD_ONLYBEHAVIORS); assert(r >= 0);

    // CK_PLUGIN_TYPE
    r = engine->RegisterEnum("CK_PLUGIN_TYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_BITMAP_READER", CKPLUGIN_BITMAP_READER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_SOUND_READER", CKPLUGIN_SOUND_READER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_MODEL_READER", CKPLUGIN_MODEL_READER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_MANAGER_DLL", CKPLUGIN_MANAGER_DLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_BEHAVIOR_DLL", CKPLUGIN_BEHAVIOR_DLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_RENDERENGINE_DLL", CKPLUGIN_RENDERENGINE_DLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_MOVIE_READER", CKPLUGIN_MOVIE_READER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PLUGIN_TYPE", "CKPLUGIN_EXTENSION_DLL", CKPLUGIN_EXTENSION_DLL); assert(r >= 0);

    // CK_VIRTOOLS_VERSION
    r = engine->RegisterEnum("CK_VIRTOOLS_VERSION"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV", CK_VIRTOOLS_DEV); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_CREATION", CK_VIRTOOLS_CREATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_EVAL", CK_VIRTOOLS_DEV_EVAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_CREA_EVAL", CK_VIRTOOLS_CREA_EVAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_NFR", CK_VIRTOOLS_DEV_NFR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_CREA_NFR", CK_VIRTOOLS_CREA_NFR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_EDU", CK_VIRTOOLS_DEV_EDU); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_CREA_EDU", CK_VIRTOOLS_CREA_EDU); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_TB", CK_VIRTOOLS_DEV_TB); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_CREA_TB", CK_VIRTOOLS_CREA_TB); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_NCV", CK_VIRTOOLS_DEV_NCV); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_DEV_SE", CK_VIRTOOLS_DEV_SE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_VIRTOOLS_VERSION", "CK_VIRTOOLS_MAXVERSION", CK_VIRTOOLS_MAXVERSION); assert(r >= 0);

    // CKJOY
    r = engine->RegisterEnum("CKJOY"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKJOY", "CKJOY_1", CKJOY_1); assert(r >= 0);
    r = engine->RegisterEnumValue("CKJOY", "CKJOY_2", CKJOY_2); assert(r >= 0);
    r = engine->RegisterEnumValue("CKJOY", "CKJOY_3", CKJOY_3); assert(r >= 0);
    r = engine->RegisterEnumValue("CKJOY", "CKJOY_4", CKJOY_4); assert(r >= 0);
    r = engine->RegisterEnumValue("CKJOY", "CKMAX_JOY", CKMAX_JOY); assert(r >= 0);

    // CKKEYBOARD
    r = engine->RegisterEnum("CKKEYBOARD"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_ESCAPE", CKKEY_ESCAPE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_1", CKKEY_1); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_2", CKKEY_2); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_3", CKKEY_3); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_4", CKKEY_4); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_5", CKKEY_5); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_6", CKKEY_6); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_7", CKKEY_7); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_8", CKKEY_8); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_9", CKKEY_9); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_0", CKKEY_0); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_MINUS", CKKEY_MINUS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_EQUALS", CKKEY_EQUALS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_BACK", CKKEY_BACK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_TAB", CKKEY_TAB); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_Q", CKKEY_Q); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_W", CKKEY_W); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_E", CKKEY_E); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_R", CKKEY_R); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_T", CKKEY_T); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_Y", CKKEY_Y); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_U", CKKEY_U); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_I", CKKEY_I); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_O", CKKEY_O); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_P", CKKEY_P); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LBRACKET", CKKEY_LBRACKET); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RBRACKET", CKKEY_RBRACKET); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RETURN", CKKEY_RETURN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LCONTROL", CKKEY_LCONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_A", CKKEY_A); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_S", CKKEY_S); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_D", CKKEY_D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F", CKKEY_F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_G", CKKEY_G); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_H", CKKEY_H); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_J", CKKEY_J); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_K", CKKEY_K); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_L", CKKEY_L); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SEMICOLON", CKKEY_SEMICOLON); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_APOSTROPHE", CKKEY_APOSTROPHE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_GRAVE", CKKEY_GRAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LSHIFT", CKKEY_LSHIFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_BACKSLASH", CKKEY_BACKSLASH); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_Z", CKKEY_Z); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_X", CKKEY_X); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_C", CKKEY_C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_V", CKKEY_V); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_B", CKKEY_B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_N", CKKEY_N); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_M", CKKEY_M); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_COMMA", CKKEY_COMMA); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_PERIOD", CKKEY_PERIOD); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SLASH", CKKEY_SLASH); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RSHIFT", CKKEY_RSHIFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_MULTIPLY", CKKEY_MULTIPLY); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LMENU", CKKEY_LMENU); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SPACE", CKKEY_SPACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_CAPITAL", CKKEY_CAPITAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F1", CKKEY_F1); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F2", CKKEY_F2); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F3", CKKEY_F3); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F4", CKKEY_F4); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F5", CKKEY_F5); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F6", CKKEY_F6); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F7", CKKEY_F7); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F8", CKKEY_F8); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F9", CKKEY_F9); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F10", CKKEY_F10); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMLOCK", CKKEY_NUMLOCK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SCROLL", CKKEY_SCROLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD7", CKKEY_NUMPAD7); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD8", CKKEY_NUMPAD8); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD9", CKKEY_NUMPAD9); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SUBTRACT", CKKEY_SUBTRACT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD4", CKKEY_NUMPAD4); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD5", CKKEY_NUMPAD5); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD6", CKKEY_NUMPAD6); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_ADD", CKKEY_ADD); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD1", CKKEY_NUMPAD1); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD2", CKKEY_NUMPAD2); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD3", CKKEY_NUMPAD3); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPAD0", CKKEY_NUMPAD0); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_DECIMAL", CKKEY_DECIMAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F11", CKKEY_F11); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F12", CKKEY_F12); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F13", CKKEY_F13); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F14", CKKEY_F14); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_F15", CKKEY_F15); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_KANA", CKKEY_KANA); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_CONVERT", CKKEY_CONVERT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NOCONVERT", CKKEY_NOCONVERT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_YEN", CKKEY_YEN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPADEQUALS", CKKEY_NUMPADEQUALS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_CIRCUMFLEX", CKKEY_CIRCUMFLEX); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_AT", CKKEY_AT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_COLON", CKKEY_COLON); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_UNDERLINE", CKKEY_UNDERLINE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_KANJI", CKKEY_KANJI); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_STOP", CKKEY_STOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_AX", CKKEY_AX); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_UNLABELED", CKKEY_UNLABELED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPADENTER", CKKEY_NUMPADENTER); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RCONTROL", CKKEY_RCONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NUMPADCOMMA", CKKEY_NUMPADCOMMA); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_DIVIDE", CKKEY_DIVIDE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_SYSRQ", CKKEY_SYSRQ); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RMENU", CKKEY_RMENU); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_HOME", CKKEY_HOME); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_UP", CKKEY_UP); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_PRIOR", CKKEY_PRIOR); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LEFT", CKKEY_LEFT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RIGHT", CKKEY_RIGHT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_END", CKKEY_END); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_DOWN", CKKEY_DOWN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_NEXT", CKKEY_NEXT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_INSERT", CKKEY_INSERT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_DELETE", CKKEY_DELETE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_LWIN", CKKEY_LWIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_RWIN", CKKEY_RWIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEYBOARD", "CKKEY_APPS", CKKEY_APPS); assert(r >= 0);

    // CK_PARAMETER_FLAGS
    r = engine->RegisterEnum("CK_PARAMETER_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETER_FLAGS", "CKPARAMETER_LOCAL", CKPARAMETER_LOCAL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETER_FLAGS", "CKPARAMETER_IN", CKPARAMETER_IN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETER_FLAGS", "CKPARAMETER_OUT", CKPARAMETER_OUT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PARAMETER_FLAGS", "CKPARAMETER_SETTING", CKPARAMETER_SETTING); assert(r >= 0);

    // CK_PROFILE_CATEGORY
    r = engine->RegisterEnum("CK_PROFILE_CATEGORY"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PROFILE_CATEGORY", "CK_PROFILE_RENDERTIME", CK_PROFILE_RENDERTIME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PROFILE_CATEGORY", "CK_PROFILE_IKTIME", CK_PROFILE_IKTIME); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PROFILE_CATEGORY", "CK_PROFILE_ANIMATIONTIME", CK_PROFILE_ANIMATIONTIME); assert(r >= 0);

    // CKERROR
    r = engine->RegisterEnum("CKERROR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CK_OK", CK_OK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDPARAMETER", CKERR_INVALIDPARAMETER); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDPARAMETERTYPE", CKERR_INVALIDPARAMETERTYPE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDSIZE", CKERR_INVALIDSIZE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDOPERATION", CKERR_INVALIDOPERATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_OPERATIONNOTIMPLEMENTED", CKERR_OPERATIONNOTIMPLEMENTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_OUTOFMEMORY", CKERR_OUTOFMEMORY); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOTIMPLEMENTED", CKERR_NOTIMPLEMENTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOTFOUND", CKERR_NOTFOUND); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOLEVEL", CKERR_NOLEVEL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_CANCREATERENDERCONTEXT", CKERR_CANCREATERENDERCONTEXT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOTIFICATIONNOTHANDLED", CKERR_NOTIFICATIONNOTHANDLED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_ALREADYPRESENT", CKERR_ALREADYPRESENT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDRENDERCONTEXT", CKERR_INVALIDRENDERCONTEXT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_RENDERCONTEXTINACTIVE", CKERR_RENDERCONTEXTINACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOLOADPLUGINS", CKERR_NOLOADPLUGINS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOSAVEPLUGINS", CKERR_NOSAVEPLUGINS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDFILE", CKERR_INVALIDFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDPLUGIN", CKERR_INVALIDPLUGIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOTINITIALIZED", CKERR_NOTINITIALIZED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDMESSAGE", CKERR_INVALIDMESSAGE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDPROTOTYPE", CKERR_INVALIDPROTOTYPE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NODLLFOUND", CKERR_NODLLFOUND); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_ALREADYREGISTREDDLL", CKERR_ALREADYREGISTREDDLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDDLL", CKERR_INVALIDDLL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDOBJECT", CKERR_INVALIDOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDCONDSOLEWINDOW", CKERR_INVALIDCONDSOLEWINDOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDKINEMATICCHAIN", CKERR_INVALIDKINEMATICCHAIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOKEYBOARD", CKERR_NOKEYBOARD); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOMOUSE", CKERR_NOMOUSE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOJOYSTICK", CKERR_NOJOYSTICK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INCOMPATIBLEPARAMETERS", CKERR_INCOMPATIBLEPARAMETERS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NORENDERENGINE", CKERR_NORENDERENGINE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOCURRENTLEVEL", CKERR_NOCURRENTLEVEL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_SOUNDDISABLED", CKERR_SOUNDDISABLED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_DINPUTDISABLED", CKERR_DINPUTDISABLED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDGUID", CKERR_INVALIDGUID); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOTENOUGHDISKPLACE", CKERR_NOTENOUGHDISKPLACE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_CANTWRITETOFILE", CKERR_CANTWRITETOFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_BEHAVIORADDDENIEDBYCB", CKERR_BEHAVIORADDDENIEDBYCB); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INCOMPATIBLECLASSID", CKERR_INCOMPATIBLECLASSID); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_MANAGERALREADYEXISTS", CKERR_MANAGERALREADYEXISTS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_PAUSED", CKERR_PAUSED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_PLUGINSMISSING", CKERR_PLUGINSMISSING); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_OBSOLETEVIRTOOLS", CKERR_OBSOLETEVIRTOOLS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_FILECRCERROR", CKERR_FILECRCERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_ALREADYFULLSCREEN", CKERR_ALREADYFULLSCREEN); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_CANCELLED", CKERR_CANCELLED); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_NOANIMATIONKEY", CKERR_NOANIMATIONKEY); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDINDEX", CKERR_INVALIDINDEX); assert(r >= 0);
    r = engine->RegisterEnumValue("CKERROR", "CKERR_INVALIDANIMATION", CKERR_INVALIDANIMATION); assert(r >= 0);

    // CK_DATAREADER_FLAGS
    r = engine->RegisterEnum("CK_DATAREADER_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_FILELOAD", CK_DATAREADER_FILELOAD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_FILESAVE", CK_DATAREADER_FILESAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_MEMORYLOAD", CK_DATAREADER_MEMORYLOAD); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_MEMORYSAVE", CK_DATAREADER_MEMORYSAVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_STREAMFILE", CK_DATAREADER_STREAMFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_STREAMURL", CK_DATAREADER_STREAMURL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DATAREADER_FLAGS", "CK_DATAREADER_VXSTREAMLOAD", CK_DATAREADER_VXSTREAMLOAD); assert(r >= 0);

    // CK_BITMAPREADER_ERROR
    r = engine->RegisterEnum("CK_BITMAPREADER_ERROR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_GENERIC", CKBITMAPERROR_GENERIC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_READERROR", CKBITMAPERROR_READERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_UNSUPPORTEDFILE", CKBITMAPERROR_UNSUPPORTEDFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_FILECORRUPTED", CKBITMAPERROR_FILECORRUPTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_SAVEERROR", CKBITMAPERROR_SAVEERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_UNSUPPORTEDSAVEFORMAT", CKBITMAPERROR_UNSUPPORTEDSAVEFORMAT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_BITMAPREADER_ERROR", "CKBITMAPERROR_UNSUPPORTEDFUNCTION", CKBITMAPERROR_UNSUPPORTEDFUNCTION); assert(r >= 0);

    // CK_SOUNDREADER_ERROR
    r = engine->RegisterEnum("CK_SOUNDREADER_ERROR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDREADER_ERROR", "CKSOUND_READER_OK", CKSOUND_READER_OK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDREADER_ERROR", "CKSOUND_READER_GENERICERR", CKSOUND_READER_GENERICERR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDREADER_ERROR", "CKSOUND_READER_EOF", CKSOUND_READER_EOF); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDREADER_ERROR", "CKSOUND_READER_NO_DATA_READY", CKSOUND_READER_NO_DATA_READY); assert(r >= 0);

    // CK_MOVIEREADER_ERROR
    r = engine->RegisterEnum("CK_MOVIEREADER_ERROR"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_GENERIC", CKMOVIEERROR_GENERIC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_READERROR", CKMOVIEERROR_READERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_UNSUPPORTEDFILE", CKMOVIEERROR_UNSUPPORTEDFILE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_FILECORRUPTED", CKMOVIEERROR_FILECORRUPTED); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_SAVEERROR", CKMOVIEERROR_SAVEERROR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MOVIEREADER_ERROR", "CKMOVIEERROR_UNSUPPORTEDSAVEFORMAT", CKMOVIEERROR_UNSUPPORTEDSAVEFORMAT); assert(r >= 0);

    // CK_MESSAGE_SENDINGTYPE
    r = engine->RegisterEnum("CK_MESSAGE_SENDINGTYPE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MESSAGE_SENDINGTYPE", "CK_MESSAGE_BROADCAST", CK_MESSAGE_BROADCAST); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MESSAGE_SENDINGTYPE", "CK_MESSAGE_SINGLE", CK_MESSAGE_SINGLE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_MESSAGE_SENDINGTYPE", "CK_MESSAGE_GROUP", CK_MESSAGE_GROUP); assert(r >= 0);

    // CKMANAGER_FUNCTIONS
    r = engine->RegisterEnum("CKMANAGER_FUNCTIONS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnSequenceToBeDeleted", CKMANAGER_FUNC_OnSequenceToBeDeleted); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnSequenceDeleted", CKMANAGER_FUNC_OnSequenceDeleted); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PreProcess", CKMANAGER_FUNC_PreProcess); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PostProcess", CKMANAGER_FUNC_PostProcess); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PreClearAll", CKMANAGER_FUNC_PreClearAll); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PostClearAll", CKMANAGER_FUNC_PostClearAll); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKInit", CKMANAGER_FUNC_OnCKInit); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKEnd", CKMANAGER_FUNC_OnCKEnd); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKPlay", CKMANAGER_FUNC_OnCKPlay); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKPause", CKMANAGER_FUNC_OnCKPause); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PreLoad", CKMANAGER_FUNC_PreLoad); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PreSave", CKMANAGER_FUNC_PreSave); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PreLaunchScene", CKMANAGER_FUNC_PreLaunchScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PostLaunchScene", CKMANAGER_FUNC_PostLaunchScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKReset", CKMANAGER_FUNC_OnCKReset); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PostLoad", CKMANAGER_FUNC_PostLoad); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_PostSave", CKMANAGER_FUNC_PostSave); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnCKPostReset", CKMANAGER_FUNC_OnCKPostReset); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnSequenceAddedToScene", CKMANAGER_FUNC_OnSequenceAddedToScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnSequenceRemovedFromScene", CKMANAGER_FUNC_OnSequenceRemovedFromScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnPreCopy", CKMANAGER_FUNC_OnPreCopy); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnPostCopy", CKMANAGER_FUNC_OnPostCopy); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnPreRender", CKMANAGER_FUNC_OnPreRender); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnPostRender", CKMANAGER_FUNC_OnPostRender); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS", "CKMANAGER_FUNC_OnPostSpriteRender", CKMANAGER_FUNC_OnPostSpriteRender); assert(r >= 0);

    // CKMANAGER_FUNCTIONS_INDEX
    r = engine->RegisterEnum("CKMANAGER_FUNCTIONS_INDEX"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnSequenceToBeDeleted", CKMANAGER_INDEX_OnSequenceToBeDeleted); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnSequenceDeleted", CKMANAGER_INDEX_OnSequenceDeleted); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PreProcess", CKMANAGER_INDEX_PreProcess); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PostProcess", CKMANAGER_INDEX_PostProcess); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PreClearAll", CKMANAGER_INDEX_PreClearAll); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PostClearAll", CKMANAGER_INDEX_PostClearAll); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKInit", CKMANAGER_INDEX_OnCKInit); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKEnd", CKMANAGER_INDEX_OnCKEnd); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKPlay", CKMANAGER_INDEX_OnCKPlay); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKPause", CKMANAGER_INDEX_OnCKPause); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PreLoad", CKMANAGER_INDEX_PreLoad); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PreSave", CKMANAGER_INDEX_PreSave); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PreLaunchScene", CKMANAGER_INDEX_PreLaunchScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PostLaunchScene", CKMANAGER_INDEX_PostLaunchScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKReset", CKMANAGER_INDEX_OnCKReset); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PostLoad", CKMANAGER_INDEX_PostLoad); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_PostSave", CKMANAGER_INDEX_PostSave); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnCKPostReset", CKMANAGER_INDEX_OnCKPostReset); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnSequenceAddedToScene", CKMANAGER_INDEX_OnSequenceAddedToScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnSequenceRemovedFromScene", CKMANAGER_INDEX_OnSequenceRemovedFromScene); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnPreCopy", CKMANAGER_INDEX_OnPreCopy); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnPostCopy", CKMANAGER_INDEX_OnPostCopy); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnPreRender", CKMANAGER_INDEX_OnPreRender); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnPostRender", CKMANAGER_INDEX_OnPostRender); assert(r >= 0);
    r = engine->RegisterEnumValue("CKMANAGER_FUNCTIONS_INDEX", "CKMANAGER_INDEX_OnPostSpriteRender", CKMANAGER_INDEX_OnPostSpriteRender); assert(r >= 0);

    // CK_OBJECT_SHOWOPTION
    r = engine->RegisterEnum("CK_OBJECT_SHOWOPTION"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_SHOWOPTION", "CKHIDE", CKHIDE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_SHOWOPTION", "CKSHOW", CKSHOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_OBJECT_SHOWOPTION", "CKHIERARCHICALHIDE", CKHIERARCHICALHIDE); assert(r >= 0);

    // CK_FRAMERATE_LIMITS
    r = engine->RegisterEnum("CK_FRAMERATE_LIMITS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_RATE_NOP", CK_RATE_NOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_BEHRATE_SYNC", CK_BEHRATE_SYNC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_BEHRATE_LIMIT", CK_BEHRATE_LIMIT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_BEHRATE_MASK", CK_BEHRATE_MASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_FRAMERATE_SYNC", CK_FRAMERATE_SYNC); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_FRAMERATE_FREE", CK_FRAMERATE_FREE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_FRAMERATE_LIMIT", CK_FRAMERATE_LIMIT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FRAMERATE_LIMITS", "CK_FRAMERATE_MASK", CK_FRAMERATE_MASK); assert(r >= 0);

    // CK_PATHMANAGER_CATEGORY
    r = engine->RegisterEnum("CK_PATHMANAGER_CATEGORY"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATHMANAGER_CATEGORY", "BITMAP_PATH_IDX", BITMAP_PATH_IDX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATHMANAGER_CATEGORY", "DATA_PATH_IDX", DATA_PATH_IDX); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATHMANAGER_CATEGORY", "SOUND_PATH_IDX", SOUND_PATH_IDX); assert(r >= 0);

    // CKLOCKFLAGS
    r = engine->RegisterEnum("CKLOCKFLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKLOCKFLAGS", "CK_LOCK_DEFAULT", CK_LOCK_DEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKLOCKFLAGS", "CK_LOCK_NOOVERWRITE", CK_LOCK_NOOVERWRITE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKLOCKFLAGS", "CK_LOCK_DISCARD", CK_LOCK_DISCARD); assert(r >= 0);

    // CKVB_STATE
    r = engine->RegisterEnum("CKVB_STATE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKVB_STATE", "CK_VB_OK", CK_VB_OK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKVB_STATE", "CK_VB_LOST", CK_VB_LOST); assert(r >= 0);
    r = engine->RegisterEnumValue("CKVB_STATE", "CK_VB_FAILED", CK_VB_FAILED); assert(r >= 0);

    // CK_SOUNDMANAGER_CAPS
    r = engine->RegisterEnum("CK_SOUNDMANAGER_CAPS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_SOUNDMANAGER_ONFLYTYPE", CK_SOUNDMANAGER_ONFLYTYPE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_SOUNDMANAGER_OCCLUSION", CK_SOUNDMANAGER_OCCLUSION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_SOUNDMANAGER_REFLECTION", CK_SOUNDMANAGER_REFLECTION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_SOUNDMANAGER_ALL", CK_SOUNDMANAGER_ALL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_GAIN", CK_WAVESOUND_SETTINGS_GAIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_EQUALIZATION", CK_WAVESOUND_SETTINGS_EQUALIZATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_PITCH", CK_WAVESOUND_SETTINGS_PITCH); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_PRIORITY", CK_WAVESOUND_SETTINGS_PRIORITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_PAN", CK_WAVESOUND_SETTINGS_PAN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_SETTINGS_ALL", CK_WAVESOUND_SETTINGS_ALL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_CONE", CK_WAVESOUND_3DSETTINGS_CONE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_MINMAXDISTANCE", CK_WAVESOUND_3DSETTINGS_MINMAXDISTANCE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_DISTANCEFACTOR", CK_WAVESOUND_3DSETTINGS_DISTANCEFACTOR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_DOPPLERFACTOR", CK_WAVESOUND_3DSETTINGS_DOPPLERFACTOR); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_POSITION", CK_WAVESOUND_3DSETTINGS_POSITION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_VELOCITY", CK_WAVESOUND_3DSETTINGS_VELOCITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_ORIENTATION", CK_WAVESOUND_3DSETTINGS_ORIENTATION); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_HEADRELATIVE", CK_WAVESOUND_3DSETTINGS_HEADRELATIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_WAVESOUND_3DSETTINGS_ALL", CK_WAVESOUND_3DSETTINGS_ALL); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_DISTANCE", CK_LISTENERSETTINGS_DISTANCE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_DOPPLER", CK_LISTENERSETTINGS_DOPPLER); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_UNITS", CK_LISTENERSETTINGS_UNITS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_ROLLOFF", CK_LISTENERSETTINGS_ROLLOFF); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_EQ", CK_LISTENERSETTINGS_EQ); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_GAIN", CK_LISTENERSETTINGS_GAIN); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_PRIORITY", CK_LISTENERSETTINGS_PRIORITY); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_SOFTWARESOURCES", CK_LISTENERSETTINGS_SOFTWARESOURCES); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_SOUNDMANAGER_CAPS", "CK_LISTENERSETTINGS_ALL", CK_LISTENERSETTINGS_ALL); assert(r >= 0);

    // CK_READSUBCHUNK_FLAGS
    r = engine->RegisterEnum("CK_READSUBCHUNK_FLAGS"); assert(r >= 0);
    // r = engine->RegisterEnumValue("CK_READSUBCHUNK_FLAGS", "CK_RSC_DEFAULT", CK_RSC_DEFAULT); assert(r >= 0); // Not in our SDK
    // r = engine->RegisterEnumValue("CK_READSUBCHUNK_FLAGS", "CK_RSC_SKIP", CK_RSC_SKIP); assert(r >= 0); // Not in our SDK
    // r = engine->RegisterEnumValue("CK_READSUBCHUNK_FLAGS", "CK_RSC_SCRATCH", CK_RSC_SCRATCH); assert(r >= 0); // Not in our SDK

    // CK_FO_OPTIONS
    r = engine->RegisterEnum("CK_FO_OPTIONS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FO_OPTIONS", "CK_FO_DEFAULT", CKFileObject::CK_FO_DEFAULT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FO_OPTIONS", "CK_FO_RENAMEOBJECT", CKFileObject::CK_FO_RENAMEOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FO_OPTIONS", "CK_FO_REPLACEOBJECT", CKFileObject::CK_FO_REPLACEOBJECT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_FO_OPTIONS", "CK_FO_DONTLOADOBJECT", CKFileObject::CK_FO_DONTLOADOBJECT); assert(r >= 0);

    // CK_DEPENDENCIES_FLAGS
    r = engine->RegisterEnum("CK_DEPENDENCIES_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_FLAGS", "CK_DEPENDENCIES_CUSTOM", CK_DEPENDENCIES_CUSTOM); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_FLAGS", "CK_DEPENDENCIES_NONE", CK_DEPENDENCIES_NONE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_DEPENDENCIES_FLAGS", "CK_DEPENDENCIES_FULL", CK_DEPENDENCIES_FULL); assert(r >= 0);

    // CKDEBUG_STATE
    r = engine->RegisterEnum("CKDEBUG_STATE"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKDEBUG_STATE", "CKDEBUG_NOP", CKDEBUG_NOP); assert(r >= 0);
    r = engine->RegisterEnumValue("CKDEBUG_STATE", "CKDEBUG_BEHEXECUTE", CKDEBUG_BEHEXECUTE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKDEBUG_STATE", "CKDEBUG_BEHEXECUTEDONE", CKDEBUG_BEHEXECUTEDONE); assert(r >= 0);
    r = engine->RegisterEnumValue("CKDEBUG_STATE", "CKDEBUG_SCRIPTEXECUTEDONE", CKDEBUG_SCRIPTEXECUTEDONE); assert(r >= 0);

    // CKBEZIERKEY_FLAGS
    r = engine->RegisterEnum("CKBEZIERKEY_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_AUTOSMOOTH", BEZIER_KEY_AUTOSMOOTH); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_LINEAR", BEZIER_KEY_LINEAR); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_STEP", BEZIER_KEY_STEP); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_FAST", BEZIER_KEY_FAST); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_SLOW", BEZIER_KEY_SLOW); assert(r >= 0);
    r = engine->RegisterEnumValue("CKBEZIERKEY_FLAGS", "BEZIER_KEY_TANGENTS", BEZIER_KEY_TANGENTS); assert(r >= 0);

    // CKANIMATION_CONTROLLER
    r = engine->RegisterEnum("CKANIMATION_CONTROLLER"); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_POS", CKANIMATION_CONTROLLER_POS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_ROT", CKANIMATION_CONTROLLER_ROT); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_SCL", CKANIMATION_CONTROLLER_SCL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_SCLAXIS", CKANIMATION_CONTROLLER_SCLAXIS); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_MORPH", CKANIMATION_CONTROLLER_MORPH); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_CONTROLLER_MASK", CKANIMATION_CONTROLLER_MASK); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_LINPOS_CONTROL", CKANIMATION_LINPOS_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_LINROT_CONTROL", CKANIMATION_LINROT_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_LINSCL_CONTROL", CKANIMATION_LINSCL_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_LINSCLAXIS_CONTROL", CKANIMATION_LINSCLAXIS_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_TCBPOS_CONTROL", CKANIMATION_TCBPOS_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_TCBROT_CONTROL", CKANIMATION_TCBROT_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_TCBSCL_CONTROL", CKANIMATION_TCBSCL_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_TCBSCLAXIS_CONTROL", CKANIMATION_TCBSCLAXIS_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_BEZIERPOS_CONTROL", CKANIMATION_BEZIERPOS_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_BEZIERSCL_CONTROL", CKANIMATION_BEZIERSCL_CONTROL); assert(r >= 0);
    r = engine->RegisterEnumValue("CKANIMATION_CONTROLLER", "CKANIMATION_MORPH_CONTROL", CKANIMATION_MORPH_CONTROL); assert(r >= 0);

    // CK_PATCHMESH_FLAGS
    r = engine->RegisterEnum("CK_PATCHMESH_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCHMESH_FLAGS", "CK_PATCHMESH_UPTODATE", CK_PATCHMESH_UPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCHMESH_FLAGS", "CK_PATCHMESH_BUILDNORMALS", CK_PATCHMESH_BUILDNORMALS); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCHMESH_FLAGS", "CK_PATCHMESH_MATERIALSUPTODATE", CK_PATCHMESH_MATERIALSUPTODATE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCHMESH_FLAGS", "CK_PATCHMESH_AUTOSMOOTH", CK_PATCHMESH_AUTOSMOOTH); assert(r >= 0);

    // CK_PATCH_FLAGS
    r = engine->RegisterEnum("CK_PATCH_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCH_FLAGS", "CK_PATCH_TRI", CK_PATCH_TRI); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_PATCH_FLAGS", "CK_PATCH_QUAD", CK_PATCH_QUAD); assert(r >= 0);

    // CK_IKJOINT_FLAGS
    r = engine->RegisterEnum("CK_IKJOINT_FLAGS"); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_ACTIVE_X", CK_IKJOINT_ACTIVE_X); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_ACTIVE_Y", CK_IKJOINT_ACTIVE_Y); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_ACTIVE_Z", CK_IKJOINT_ACTIVE_Z); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_ACTIVE",   CK_IKJOINT_ACTIVE); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_LIMIT_X",  CK_IKJOINT_LIMIT_X); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_LIMIT_Y",  CK_IKJOINT_LIMIT_Y); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_LIMIT_Z",  CK_IKJOINT_LIMIT_Z); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_LIMIT",    CK_IKJOINT_LIMIT); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_EASE_X",   CK_IKJOINT_EASE_X); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_EASE_Y",   CK_IKJOINT_EASE_Y); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_EASE_Z",   CK_IKJOINT_EASE_Z); assert(r >= 0);
    r = engine->RegisterEnumValue("CK_IKJOINT_FLAGS", "CK_IKJOINT_EASE",     CK_IKJOINT_EASE); assert(r >= 0);
}