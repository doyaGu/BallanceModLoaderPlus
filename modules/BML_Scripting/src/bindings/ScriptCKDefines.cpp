#include "ScriptCKDefines.h"

#include <string>

#include "CKAll.h"

#include "ScriptUtils.h"
#include "ScriptXArray.h"
#include "ScriptNativePointer.h"

static const int g_MAX_USER_PROFILE = MAX_USER_PROFILE;

static const uint32_t g_CKVERSION = CKVERSION;
static const uint32_t g_DEVVERSION = DEVVERSION;
static const CKGUID g_VIRTOOLS_GUID = VIRTOOLS_GUID;
static const float g_CK_ZERO = CK_ZERO;
static const int g_CKMAX_PATH = CKMAX_PATH;
static const int g_CKMAX_URL = CKMAX_URL;
static const int g_CKMAX_MANAGERFUNCTIONS = CKMAX_MANAGERFUNCTIONS;
static const int g_CKANIMATION_FORCESETSTEP = CKANIMATION_FORCESETSTEP;

static const int g_CKOBJECT_PRIORITYMAX = CKOBJECT_PRIORITYMAX;
static const int g_CKOBJECT_PRIORITYLEVEL = CKOBJECT_PRIORITYLEVEL;
static const int g_CKOBJECT_PRIORITYSCENE = CKOBJECT_PRIORITYSCENE;
static const int g_CKOBJECT_PRIORITYPLACE = CKOBJECT_PRIORITYPLACE;
static const int g_CKOBJECT_PRIORITYDEFAULT = CKOBJECT_PRIORITYDEFAULT;
static const int g_CKOBJECT_PRIORITYMIN = CKOBJECT_PRIORITYMIN;
static const int g_CKBEHAVIOR_PRIORITYMAX = CKBEHAVIOR_PRIORITYMAX;
static const int g_CKBEHAVIOR_PRIORITYMIN = CKBEHAVIOR_PRIORITYMIN;

static const int g_CKM_BASE = CKM_BASE;
static const int g_CKM_BEHAVIORPRESAVE = CKM_BEHAVIORPRESAVE;
static const int g_CKM_BEHAVIORDELETE = CKM_BEHAVIORDELETE;
static const int g_CKM_BEHAVIORATTACH = CKM_BEHAVIORATTACH;
static const int g_CKM_BEHAVIORDETACH = CKM_BEHAVIORDETACH;
static const int g_CKM_BEHAVIORPAUSE = CKM_BEHAVIORPAUSE;
static const int g_CKM_BEHAVIORRESUME = CKM_BEHAVIORRESUME;
static const int g_CKM_BEHAVIORCREATE = CKM_BEHAVIORCREATE;
static const int g_CKM_BEHAVIORRESET = CKM_BEHAVIORRESET;
static const int g_CKM_BEHAVIORPOSTSAVE = CKM_BEHAVIORPOSTSAVE;
static const int g_CKM_BEHAVIORLOAD = CKM_BEHAVIORLOAD;
static const int g_CKM_BEHAVIOREDITED = CKM_BEHAVIOREDITED;
static const int g_CKM_BEHAVIORSETTINGSEDITED = CKM_BEHAVIORSETTINGSEDITED;
static const int g_CKM_BEHAVIORREADSTATE = CKM_BEHAVIORREADSTATE;
static const int g_CKM_BEHAVIORNEWSCENE = CKM_BEHAVIORNEWSCENE;
static const int g_CKM_BEHAVIORACTIVATESCRIPT = CKM_BEHAVIORACTIVATESCRIPT;
static const int g_CKM_BEHAVIORDEACTIVATESCRIPT = CKM_BEHAVIORDEACTIVATESCRIPT;
static const int g_CKM_BEHAVIORRESETINBREAKBPOINT = CKM_BEHAVIORRESETINBREAKBPOINT;
static const int g_CKM_MAX_BEHAVIOR_CALLBACKS = CKM_MAX_BEHAVIOR_CALLBACKS;

static const int g_CHUNKDATA_OLDVERSION = CHUNKDATA_OLDVERSION;
static const int g_CHUNKDATA_BASEVERSION = CHUNKDATA_BASEVERSION;
static const int g_CHUNK_WAVESOUND_VERSION2 = CHUNK_WAVESOUND_VERSION2;
static const int g_CHUNK_WAVESOUND_VERSION3 = CHUNK_WAVESOUND_VERSION3;
static const int g_CHUNK_MATERIAL_VERSION_ZTEST = CHUNK_MATERIAL_VERSION_ZTEST;
static const int g_CHUNK_MAJORCHANGE_VERSION = CHUNK_MAJORCHANGE_VERSION;
static const int g_CHUNK_MACCHANGE_VERSION = CHUNK_MACCHANGE_VERSION;
static const int g_CHUNK_WAVESOUND_VERSION4 = CHUNK_WAVESOUND_VERSION4;
static const int g_CHUNK_SCENECHANGE_VERSION = CHUNK_SCENECHANGE_VERSION;
static const int g_CHUNK_MESHCHANGE_VERSION = CHUNK_MESHCHANGE_VERSION;
static const int g_CHUNK_DEV_2_1 = CHUNK_DEV_2_1;
static const int g_CHUNKDATA_CURRENTVERSION = CHUNKDATA_CURRENTVERSION;

static const int g_CKDLL_BEHAVIORPROTOTYPE = CKDLL_BEHAVIORPROTOTYPE;

static const int g_CKCID_OBJECT = CKCID_OBJECT;
static const int g_CKCID_PARAMETERIN = CKCID_PARAMETERIN;
static const int g_CKCID_PARAMETEROPERATION = CKCID_PARAMETEROPERATION;
static const int g_CKCID_STATE = CKCID_STATE;
static const int g_CKCID_BEHAVIORLINK = CKCID_BEHAVIORLINK;
static const int g_CKCID_BEHAVIOR = CKCID_BEHAVIOR;
static const int g_CKCID_BEHAVIORIO = CKCID_BEHAVIORIO;
static const int g_CKCID_RENDERCONTEXT = CKCID_RENDERCONTEXT;
static const int g_CKCID_KINEMATICCHAIN = CKCID_KINEMATICCHAIN;
static const int g_CKCID_SCENEOBJECT = CKCID_SCENEOBJECT;
static const int g_CKCID_OBJECTANIMATION = CKCID_OBJECTANIMATION;
static const int g_CKCID_ANIMATION = CKCID_ANIMATION;
static const int g_CKCID_KEYEDANIMATION = CKCID_KEYEDANIMATION;
static const int g_CKCID_BEOBJECT = CKCID_BEOBJECT;
static const int g_CKCID_DATAARRAY = CKCID_DATAARRAY;
static const int g_CKCID_SCENE = CKCID_SCENE;
static const int g_CKCID_LEVEL = CKCID_LEVEL;
static const int g_CKCID_PLACE = CKCID_PLACE;
static const int g_CKCID_GROUP = CKCID_GROUP;
static const int g_CKCID_SOUND = CKCID_SOUND;
static const int g_CKCID_WAVESOUND = CKCID_WAVESOUND;
static const int g_CKCID_MIDISOUND = CKCID_MIDISOUND;
static const int g_CKCID_MATERIAL = CKCID_MATERIAL;
static const int g_CKCID_TEXTURE = CKCID_TEXTURE;
static const int g_CKCID_MESH = CKCID_MESH;
static const int g_CKCID_PATCHMESH = CKCID_PATCHMESH;
static const int g_CKCID_RENDEROBJECT = CKCID_RENDEROBJECT;
static const int g_CKCID_2DENTITY = CKCID_2DENTITY;
static const int g_CKCID_SPRITE = CKCID_SPRITE;
static const int g_CKCID_SPRITETEXT = CKCID_SPRITETEXT;
static const int g_CKCID_3DENTITY = CKCID_3DENTITY;
static const int g_CKCID_GRID = CKCID_GRID;
static const int g_CKCID_CURVEPOINT = CKCID_CURVEPOINT;
static const int g_CKCID_SPRITE3D = CKCID_SPRITE3D;
static const int g_CKCID_CURVE = CKCID_CURVE;
static const int g_CKCID_CAMERA = CKCID_CAMERA;
static const int g_CKCID_TARGETCAMERA = CKCID_TARGETCAMERA;
static const int g_CKCID_LIGHT = CKCID_LIGHT;
static const int g_CKCID_TARGETLIGHT = CKCID_TARGETLIGHT;
static const int g_CKCID_CHARACTER = CKCID_CHARACTER;
static const int g_CKCID_3DOBJECT = CKCID_3DOBJECT;
static const int g_CKCID_BODYPART = CKCID_BODYPART;
static const int g_CKCID_PARAMETER = CKCID_PARAMETER;
static const int g_CKCID_PARAMETERLOCAL = CKCID_PARAMETERLOCAL;
// static const int g_CKCID_PARAMETERVARIABLE = CKCID_PARAMETERVARIABLE; // Not in our SDK
static const int g_CKCID_PARAMETEROUT = CKCID_PARAMETEROUT;
static const int g_CKCID_INTERFACEOBJECTMANAGER = CKCID_INTERFACEOBJECTMANAGER;
static const int g_CKCID_CRITICALSECTION = CKCID_CRITICALSECTION;
static const int g_CKCID_LAYER = CKCID_LAYER;
static const int g_CKCID_MAXCLASSID = CKCID_MAXCLASSID;
static const int g_CKCID_SYNCHRO = CKCID_SYNCHRO;

static const int g_CKCID_OBJECTARRAY = CKCID_OBJECTARRAY;
static const int g_CKCID_SCENEOBJECTDESC = CKCID_SCENEOBJECTDESC;
static const int g_CKCID_ATTRIBUTEMANAGER = CKCID_ATTRIBUTEMANAGER;
static const int g_CKCID_MESSAGEMANAGER = CKCID_MESSAGEMANAGER;
static const int g_CKCID_COLLISIONMANAGER = CKCID_COLLISIONMANAGER;
static const int g_CKCID_OBJECTMANAGER = CKCID_OBJECTMANAGER;
static const int g_CKCID_FLOORMANAGER = CKCID_FLOORMANAGER;
static const int g_CKCID_RENDERMANAGER = CKCID_RENDERMANAGER;
static const int g_CKCID_BEHAVIORMANAGER = CKCID_BEHAVIORMANAGER;
static const int g_CKCID_INPUTMANAGER = CKCID_INPUTMANAGER;
static const int g_CKCID_PARAMETERMANAGER = CKCID_PARAMETERMANAGER;
static const int g_CKCID_GRIDMANAGER = CKCID_GRIDMANAGER;
static const int g_CKCID_SOUNDMANAGER = CKCID_SOUNDMANAGER;
static const int g_CKCID_TIMEMANAGER = CKCID_TIMEMANAGER;
static const int g_CKCID_CUIKBEHDATA = CKCID_CUIKBEHDATA;

static const int g_CK_GENERALOPTIONS_NODUPLICATENAMECHECK = CK_GENERALOPTIONS_NODUPLICATENAMECHECK;
static const int g_CK_GENERALOPTIONS_CANUSECURRENTOBJECT = CK_GENERALOPTIONS_CANUSECURRENTOBJECT;
static const int g_CK_GENERALOPTIONS_AUTOMATICUSECURRENT = CK_GENERALOPTIONS_AUTOMATICUSECURRENT;

static const int g_CKCID_MAXMAXCLASSID = CKCID_MAXMAXCLASSID;

static const int g_CKWM_BASE = CKWM_BASE;
static const int g_CKWM_OK = CKWM_OK;
static const int g_CKWM_CANCEL = CKWM_CANCEL;
static const int g_CKWM_SETVALUE = CKWM_SETVALUE;
static const int g_CKWM_GETVALUE = CKWM_GETVALUE;
static const int g_CKWM_INIT = CKWM_INIT;
static const int g_CKWM_PARAMPICK = CKWM_PARAMPICK;
static const int g_CKWM_PICK = CKWM_PICK;
static const int g_CKWM_SETPARAMTEXT = CKWM_SETPARAMTEXT;
static const int g_CKWM_SIZECHANGED = CKWM_SIZECHANGED;
static const int g_CKWM_PARAMMODIFIED = CKWM_PARAMMODIFIED;
static const int g_CKWM_CREATEBEHAVIORLOCALS = CKWM_CREATEBEHAVIORLOCALS;
static const int g_CKWM_SETMARGIN = CKWM_SETMARGIN;
static const int g_CKWM_GETMARGIN = CKWM_GETMARGIN;
static const int g_CKWM_STARTPICK = CKWM_STARTPICK;
static const int g_CKWM_ENDPICK = CKWM_ENDPICK;

static const CKGUID g_OBJECT_MANAGER_GUID = OBJECT_MANAGER_GUID;
static const CKGUID g_ATTRIBUTE_MANAGER_GUID = ATTRIBUTE_MANAGER_GUID;
static const CKGUID g_MESSAGE_MANAGER_GUID = MESSAGE_MANAGER_GUID;
static const CKGUID g_TIME_MANAGER_GUID = TIME_MANAGER_GUID;
static const CKGUID g_SOUND_MANAGER_GUID = SOUND_MANAGER_GUID;
static const CKGUID g_MIDI_MANAGER_GUID = MIDI_MANAGER_GUID;
static const CKGUID g_INPUT_MANAGER_GUID = INPUT_MANAGER_GUID;
static const CKGUID g_BEHAVIOR_MANAGER_GUID = BEHAVIOR_MANAGER_GUID;
static const CKGUID g_FLOOR_MANAGER_GUID = FLOOR_MANAGER_GUID;
static const CKGUID g_COLLISION_MANAGER_GUID = COLLISION_MANAGER_GUID;
static const CKGUID g_GRID_MANAGER_GUID = GRID_MANAGER_GUID;
static const CKGUID g_INTERFACE_MANAGER_GUID = INTERFACE_MANAGER_GUID;
static const CKGUID g_RENDER_MANAGER_GUID = RENDER_MANAGER_GUID;
static const CKGUID g_PARAMETER_MANAGER_GUID = PARAMETER_MANAGER_GUID;
static const CKGUID g_PATH_MANAGER_GUID = PATH_MANAGER_GUID;

static const CKGUID g_CKPGUID_NONE = CKPGUID_NONE;
static const CKGUID g_CKPGUID_VOIDBUF = CKPGUID_VOIDBUF;
static const CKGUID g_CKPGUID_FLOAT = CKPGUID_FLOAT;
static const CKGUID g_CKPGUID_ANGLE = CKPGUID_ANGLE;
static const CKGUID g_CKPGUID_PERCENTAGE = CKPGUID_PERCENTAGE;
static const CKGUID g_CKPGUID_INT = CKPGUID_INT;
static const CKGUID g_CKPGUID_KEY = CKPGUID_KEY;
static const CKGUID g_CKPGUID_BOOL = CKPGUID_BOOL;
static const CKGUID g_CKPGUID_STRING = CKPGUID_STRING;
static const CKGUID g_CKPGUID_RECT = CKPGUID_RECT;
static const CKGUID g_CKPGUID_VECTOR = CKPGUID_VECTOR;
static const CKGUID g_CKPGUID_2DVECTOR = CKPGUID_2DVECTOR;
static const CKGUID g_CKPGUID_QUATERNION = CKPGUID_QUATERNION;
static const CKGUID g_CKPGUID_EULERANGLES = CKPGUID_EULERANGLES;
static const CKGUID g_CKPGUID_MATRIX = CKPGUID_MATRIX;
static const CKGUID g_CKPGUID_COLOR = CKPGUID_COLOR;
static const CKGUID g_CKPGUID_BOX = CKPGUID_BOX;
static const CKGUID g_CKPGUID_OBJECTARRAY = CKPGUID_OBJECTARRAY;
static const CKGUID g_CKPGUID_OBJECT = CKPGUID_OBJECT;
static const CKGUID g_CKPGUID_BEOBJECT = CKPGUID_BEOBJECT;
static const CKGUID g_CKPGUID_MESH = CKPGUID_MESH;
static const CKGUID g_CKPGUID_MATERIAL = CKPGUID_MATERIAL;
static const CKGUID g_CKPGUID_TEXTURE = CKPGUID_TEXTURE;
static const CKGUID g_CKPGUID_SPRITE = CKPGUID_SPRITE;
static const CKGUID g_CKPGUID_3DENTITY = CKPGUID_3DENTITY;
static const CKGUID g_CKPGUID_CURVEPOINT = CKPGUID_CURVEPOINT;
static const CKGUID g_CKPGUID_LIGHT = CKPGUID_LIGHT;
static const CKGUID g_CKPGUID_TARGETLIGHT = CKPGUID_TARGETLIGHT;
static const CKGUID g_CKPGUID_ID = CKPGUID_ID;
static const CKGUID g_CKPGUID_CAMERA = CKPGUID_CAMERA;
static const CKGUID g_CKPGUID_TARGETCAMERA = CKPGUID_TARGETCAMERA;
static const CKGUID g_CKPGUID_SPRITE3D = CKPGUID_SPRITE3D;
static const CKGUID g_CKPGUID_OBJECT3D = CKPGUID_OBJECT3D;
static const CKGUID g_CKPGUID_BODYPART = CKPGUID_BODYPART;
static const CKGUID g_CKPGUID_CHARACTER = CKPGUID_CHARACTER;
static const CKGUID g_CKPGUID_CURVE = CKPGUID_CURVE;
static const CKGUID g_CKPGUID_2DCURVE = CKPGUID_2DCURVE;
static const CKGUID g_CKPGUID_LEVEL = CKPGUID_LEVEL;
static const CKGUID g_CKPGUID_PLACE = CKPGUID_PLACE;
static const CKGUID g_CKPGUID_GROUP = CKPGUID_GROUP;
static const CKGUID g_CKPGUID_2DENTITY = CKPGUID_2DENTITY;
static const CKGUID g_CKPGUID_RENDEROBJECT = CKPGUID_RENDEROBJECT;
static const CKGUID g_CKPGUID_SPRITETEXT = CKPGUID_SPRITETEXT;
static const CKGUID g_CKPGUID_SOUND = CKPGUID_SOUND;
static const CKGUID g_CKPGUID_WAVESOUND = CKPGUID_WAVESOUND;
static const CKGUID g_CKPGUID_MIDISOUND = CKPGUID_MIDISOUND;
static const CKGUID g_CKPGUID_OBJECTANIMATION = CKPGUID_OBJECTANIMATION;
static const CKGUID g_CKPGUID_ANIMATION = CKPGUID_ANIMATION;
static const CKGUID g_CKPGUID_KINEMATICCHAIN = CKPGUID_KINEMATICCHAIN;
static const CKGUID g_CKPGUID_SCENE = CKPGUID_SCENE;
static const CKGUID g_CKPGUID_BEHAVIOR = CKPGUID_BEHAVIOR;
static const CKGUID g_CKPGUID_MESSAGE = CKPGUID_MESSAGE;
static const CKGUID g_CKPGUID_SYNCHRO = CKPGUID_SYNCHRO;
static const CKGUID g_CKPGUID_CRITICALSECTION = CKPGUID_CRITICALSECTION;
static const CKGUID g_CKPGUID_STATE = CKPGUID_STATE;
static const CKGUID g_CKPGUID_ATTRIBUTE = CKPGUID_ATTRIBUTE;
static const CKGUID g_CKPGUID_CLASSID = CKPGUID_CLASSID;
static const CKGUID g_CKPGUID_DIRECTION = CKPGUID_DIRECTION;
static const CKGUID g_CKPGUID_BLENDMODE = CKPGUID_BLENDMODE;
static const CKGUID g_CKPGUID_FILTERMODE = CKPGUID_FILTERMODE;
static const CKGUID g_CKPGUID_BLENDFACTOR = CKPGUID_BLENDFACTOR;
static const CKGUID g_CKPGUID_FILLMODE = CKPGUID_FILLMODE;
static const CKGUID g_CKPGUID_LITMODE = CKPGUID_LITMODE;
static const CKGUID g_CKPGUID_SHADEMODE = CKPGUID_SHADEMODE;
static const CKGUID g_CKPGUID_GLOBALEXMODE = CKPGUID_GLOBALEXMODE;
static const CKGUID g_CKPGUID_ZFUNC = CKPGUID_ZFUNC;
static const CKGUID g_CKPGUID_ADDRESSMODE = CKPGUID_ADDRESSMODE;
static const CKGUID g_CKPGUID_WRAPMODE = CKPGUID_WRAPMODE;
static const CKGUID g_CKPGUID_3DSPRITEMODE = CKPGUID_3DSPRITEMODE;
static const CKGUID g_CKPGUID_FOGMODE = CKPGUID_FOGMODE;
static const CKGUID g_CKPGUID_LIGHTTYPE = CKPGUID_LIGHTTYPE;
static const CKGUID g_CKPGUID_SPRITEALIGN = CKPGUID_SPRITEALIGN;
static const CKGUID g_CKPGUID_SCRIPT = CKPGUID_SCRIPT;
static const CKGUID g_CKPGUID_LAYERTYPE = CKPGUID_LAYERTYPE;
static const CKGUID g_CKPGUID_STATECHUNK = CKPGUID_STATECHUNK;
static const CKGUID g_CKPGUID_DATAARRAY = CKPGUID_DATAARRAY;
static const CKGUID g_CKPGUID_COMPOPERATOR = CKPGUID_COMPOPERATOR;
static const CKGUID g_CKPGUID_BINARYOPERATOR = CKPGUID_BINARYOPERATOR;
static const CKGUID g_CKPGUID_SETOPERATOR = CKPGUID_SETOPERATOR;
static const CKGUID g_CKPGUID_SPRITETEXTALIGNMENT = CKPGUID_SPRITETEXTALIGNMENT;
static const CKGUID g_CKPGUID_OBSTACLEPRECISION = CKPGUID_OBSTACLEPRECISION;
static const CKGUID g_CKPGUID_OBSTACLEPRECISIONBEH = CKPGUID_OBSTACLEPRECISIONBEH;
static const CKGUID g_CKPGUID_OBSTACLE = CKPGUID_OBSTACLE;
static const CKGUID g_CKPGUID_PATCHMESH = CKPGUID_PATCHMESH;
static const CKGUID g_CKPGUID_POINTER = CKPGUID_POINTER;
static const CKGUID g_CKPGUID_ENUMS = CKPGUID_ENUMS;
static const CKGUID g_CKPGUID_STRUCTS = CKPGUID_STRUCTS;
static const CKGUID g_CKPGUID_FLAGS = CKPGUID_FLAGS;
static const CKGUID g_CKPGUID_FILTER = CKPGUID_FILTER;
static const CKGUID g_CKPGUID_TIME = CKPGUID_TIME;
static const CKGUID g_CKPGUID_OLDTIME = CKPGUID_OLDTIME;
static const CKGUID g_CKPGUID_COPYDEPENDENCIES = CKPGUID_COPYDEPENDENCIES;
static const CKGUID g_CKPGUID_DELETEDEPENDENCIES = CKPGUID_DELETEDEPENDENCIES;
static const CKGUID g_CKPGUID_SAVEDEPENDENCIES = CKPGUID_SAVEDEPENDENCIES;
static const CKGUID g_CKPGUID_REPLACEDEPENDENCIES = CKPGUID_REPLACEDEPENDENCIES;
static const CKGUID g_CKPGUID_SCENEACTIVITYFLAGS = CKPGUID_SCENEACTIVITYFLAGS;
static const CKGUID g_CKPGUID_SCENEOBJECT = CKPGUID_SCENEOBJECT;
static const CKGUID g_CKPGUID_SCENERESETFLAGS = CKPGUID_SCENERESETFLAGS;
static const CKGUID g_CKPGUID_ARRAYTYPE = CKPGUID_ARRAYTYPE;
static const CKGUID g_CKPGUID_RENDEROPTIONS = CKPGUID_RENDEROPTIONS;
static const CKGUID g_CKPGUID_PARAMETERTYPE = CKPGUID_PARAMETERTYPE;
static const CKGUID g_CKPGUID_MATERIALEFFECT = CKPGUID_MATERIALEFFECT;
static const CKGUID g_CKPGUID_TEXGENEFFECT = CKPGUID_TEXGENEFFECT;
static const CKGUID g_CKPGUID_TEXGENREFEFFECT = CKPGUID_TEXGENREFEFFECT;
static const CKGUID g_CKPGUID_COMBINE2TEX = CKPGUID_COMBINE2TEX;
static const CKGUID g_CKPGUID_COMBINE3TEX = CKPGUID_COMBINE3TEX;
static const CKGUID g_CKPGUID_BUMPMAPPARAM = CKPGUID_BUMPMAPPARAM;
static const CKGUID g_CKPGUID_TEXCOMBINE = CKPGUID_TEXCOMBINE;
static const CKGUID g_CKPGUID_PIXELFORMAT = CKPGUID_PIXELFORMAT;
static const CKGUID g_CKPGUID_AXIS = CKPGUID_AXIS;
static const CKGUID g_CKPGUID_SUPPORT = CKPGUID_SUPPORT;
static const CKGUID g_CKPGUID_BITMAP_SYSTEMCACHING = CKPGUID_BITMAP_SYSTEMCACHING;
static const CKGUID g_CKPGUID_OLDMESSAGE = CKPGUID_OLDMESSAGE;
static const CKGUID g_CKPGUID_OLDATTRIBUTE = CKPGUID_OLDATTRIBUTE;

void RegisterCKGlobalVariables(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterGlobalProperty("const int MAX_USER_PROFILE", (void *)&g_MAX_USER_PROFILE); assert(r >= 0);

    r = engine->RegisterGlobalProperty("const uint CKVERSION", (void *)&g_CKVERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const uint DEVVERSION", (void *)&g_DEVVERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID VIRTOOLS_GUID", (void *)&g_VIRTOOLS_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const float CK_ZERO", (void *)&g_CK_ZERO); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKMAX_PATH", (void *)&g_CKMAX_PATH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKMAX_URL", (void *)&g_CKMAX_URL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKMAX_MANAGERFUNCTIONS", (void *)&g_CKMAX_MANAGERFUNCTIONS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKANIMATION_FORCESETSTEP", (void *)&g_CKANIMATION_FORCESETSTEP); assert(r >= 0);

    // Default priority values
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYMAX", (void *)&g_CKOBJECT_PRIORITYMAX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYLEVEL", (void *)&g_CKOBJECT_PRIORITYLEVEL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYSCENE", (void *)&g_CKOBJECT_PRIORITYSCENE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYPLACE", (void *)&g_CKOBJECT_PRIORITYPLACE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYDEFAULT", (void *)&g_CKOBJECT_PRIORITYDEFAULT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKOBJECT_PRIORITYMIN", (void *)&g_CKOBJECT_PRIORITYMIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKBEHAVIOR_PRIORITYMAX", (void *)&g_CKBEHAVIOR_PRIORITYMAX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKBEHAVIOR_PRIORITYMIN", (void *)&g_CKBEHAVIOR_PRIORITYMIN); assert(r >= 0);

    // Behavior callback messages
    r = engine->RegisterGlobalProperty("const int CKM_BASE", (void *)&g_CKM_BASE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORPRESAVE", (void *)&g_CKM_BEHAVIORPRESAVE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORDELETE", (void *)&g_CKM_BEHAVIORDELETE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORATTACH", (void *)&g_CKM_BEHAVIORATTACH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORDETACH", (void *)&g_CKM_BEHAVIORDETACH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORPAUSE", (void *)&g_CKM_BEHAVIORPAUSE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORRESUME", (void *)&g_CKM_BEHAVIORRESUME); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORCREATE", (void *)&g_CKM_BEHAVIORCREATE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORRESET", (void *)&g_CKM_BEHAVIORRESET); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORPOSTSAVE", (void *)&g_CKM_BEHAVIORPOSTSAVE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORLOAD", (void *)&g_CKM_BEHAVIORLOAD); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIOREDITED", (void *)&g_CKM_BEHAVIOREDITED); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORSETTINGSEDITED", (void *)&g_CKM_BEHAVIORSETTINGSEDITED); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORREADSTATE", (void *)&g_CKM_BEHAVIORREADSTATE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORNEWSCENE", (void *)&g_CKM_BEHAVIORNEWSCENE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORACTIVATESCRIPT", (void *)&g_CKM_BEHAVIORACTIVATESCRIPT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORDEACTIVATESCRIPT", (void *)&g_CKM_BEHAVIORDEACTIVATESCRIPT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_BEHAVIORRESETINBREAKBPOINT", (void *)&g_CKM_BEHAVIORRESETINBREAKBPOINT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKM_MAX_BEHAVIOR_CALLBACKS", (void *)&g_CKM_MAX_BEHAVIOR_CALLBACKS); assert(r >= 0);

    r = engine->RegisterGlobalProperty("const int CHUNKDATA_OLDVERSION", (void *)&g_CHUNKDATA_OLDVERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNKDATA_BASEVERSION", (void *)&g_CHUNKDATA_BASEVERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_WAVESOUND_VERSION2", (void *)&g_CHUNK_WAVESOUND_VERSION2); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_WAVESOUND_VERSION3", (void *)&g_CHUNK_WAVESOUND_VERSION3); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_MATERIAL_VERSION_ZTEST", (void *)&g_CHUNK_MATERIAL_VERSION_ZTEST); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_MAJORCHANGE_VERSION", (void *)&g_CHUNK_MAJORCHANGE_VERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_MACCHANGE_VERSION", (void *)&g_CHUNK_MACCHANGE_VERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_WAVESOUND_VERSION4", (void *)&g_CHUNK_WAVESOUND_VERSION4); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_SCENECHANGE_VERSION", (void *)&g_CHUNK_SCENECHANGE_VERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_MESHCHANGE_VERSION", (void *)&g_CHUNK_MESHCHANGE_VERSION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNK_DEV_2_1", (void *)&g_CHUNK_DEV_2_1); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CHUNKDATA_CURRENTVERSION", (void *)&g_CHUNKDATA_CURRENTVERSION); assert(r >= 0);

    r = engine->RegisterGlobalProperty("const int CKDLL_BEHAVIORPROTOTYPE", (void *)&g_CKDLL_BEHAVIORPROTOTYPE); assert(r >= 0);

    // Class Identifier List
    r = engine->RegisterGlobalProperty("const int CKCID_OBJECT", (void *)&g_CKCID_OBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETERIN", (void *)&g_CKCID_PARAMETERIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETEROPERATION", (void *)&g_CKCID_PARAMETEROPERATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_STATE", (void *)&g_CKCID_STATE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BEHAVIORLINK", (void *)&g_CKCID_BEHAVIORLINK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BEHAVIOR", (void *)&g_CKCID_BEHAVIOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BEHAVIORIO", (void *)&g_CKCID_BEHAVIORIO); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_RENDERCONTEXT", (void *)&g_CKCID_RENDERCONTEXT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_KINEMATICCHAIN", (void *)&g_CKCID_KINEMATICCHAIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SCENEOBJECT", (void *)&g_CKCID_SCENEOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_OBJECTANIMATION", (void *)&g_CKCID_OBJECTANIMATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_ANIMATION", (void *)&g_CKCID_ANIMATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_KEYEDANIMATION", (void *)&g_CKCID_KEYEDANIMATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BEOBJECT", (void *)&g_CKCID_BEOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_DATAARRAY", (void *)&g_CKCID_DATAARRAY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SCENE", (void *)&g_CKCID_SCENE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_LEVEL", (void *)&g_CKCID_LEVEL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PLACE", (void *)&g_CKCID_PLACE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_GROUP", (void *)&g_CKCID_GROUP); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SOUND", (void *)&g_CKCID_SOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_WAVESOUND", (void *)&g_CKCID_WAVESOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_MIDISOUND", (void *)&g_CKCID_MIDISOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_MATERIAL", (void *)&g_CKCID_MATERIAL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_TEXTURE", (void *)&g_CKCID_TEXTURE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_MESH", (void *)&g_CKCID_MESH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PATCHMESH", (void *)&g_CKCID_PATCHMESH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_RENDEROBJECT", (void *)&g_CKCID_RENDEROBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_2DENTITY", (void *)&g_CKCID_2DENTITY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SPRITE", (void *)&g_CKCID_SPRITE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SPRITETEXT", (void *)&g_CKCID_SPRITETEXT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_3DENTITY", (void *)&g_CKCID_3DENTITY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_GRID", (void *)&g_CKCID_GRID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CURVEPOINT", (void *)&g_CKCID_CURVEPOINT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SPRITE3D", (void *)&g_CKCID_SPRITE3D); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CURVE", (void *)&g_CKCID_CURVE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CAMERA", (void *)&g_CKCID_CAMERA); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_TARGETCAMERA", (void *)&g_CKCID_TARGETCAMERA); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_LIGHT", (void *)&g_CKCID_LIGHT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_TARGETLIGHT", (void *)&g_CKCID_TARGETLIGHT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CHARACTER", (void *)&g_CKCID_CHARACTER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_3DOBJECT", (void *)&g_CKCID_3DOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BODYPART", (void *)&g_CKCID_BODYPART); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETER", (void *)&g_CKCID_PARAMETER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETERLOCAL", (void *)&g_CKCID_PARAMETERLOCAL); assert(r >= 0);
    // r = engine->RegisterGlobalProperty("const int CKCID_PARAMETERVARIABLE", (void *)&g_CKCID_PARAMETERVARIABLE); assert(r >= 0); // Not in our SDK
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETEROUT", (void *)&g_CKCID_PARAMETEROUT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_INTERFACEOBJECTMANAGER", (void *)&g_CKCID_INTERFACEOBJECTMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CRITICALSECTION", (void *)&g_CKCID_CRITICALSECTION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_LAYER", (void *)&g_CKCID_LAYER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_MAXCLASSID", (void *)&g_CKCID_MAXCLASSID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SYNCHRO", (void *)&g_CKCID_SYNCHRO); assert(r >= 0);

    // Not CKObject derived classes
    r = engine->RegisterGlobalProperty("const int CKCID_OBJECTARRAY", (void *)&g_CKCID_OBJECTARRAY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SCENEOBJECTDESC", (void *)&g_CKCID_SCENEOBJECTDESC); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_ATTRIBUTEMANAGER", (void *)&g_CKCID_ATTRIBUTEMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_MESSAGEMANAGER", (void *)&g_CKCID_MESSAGEMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_COLLISIONMANAGER", (void *)&g_CKCID_COLLISIONMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_OBJECTMANAGER", (void *)&g_CKCID_OBJECTMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_FLOORMANAGER", (void *)&g_CKCID_FLOORMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_RENDERMANAGER", (void *)&g_CKCID_RENDERMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_BEHAVIORMANAGER", (void *)&g_CKCID_BEHAVIORMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_INPUTMANAGER", (void *)&g_CKCID_INPUTMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_PARAMETERMANAGER", (void *)&g_CKCID_PARAMETERMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_GRIDMANAGER", (void *)&g_CKCID_GRIDMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_SOUNDMANAGER", (void *)&g_CKCID_SOUNDMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_TIMEMANAGER", (void *)&g_CKCID_TIMEMANAGER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKCID_CUIKBEHDATA", (void *)&g_CKCID_CUIKBEHDATA); assert(r >= 0);

    r = engine->RegisterGlobalProperty("const int CK_GENERALOPTIONS_NODUPLICATENAMECHECK", (void *)&g_CK_GENERALOPTIONS_NODUPLICATENAMECHECK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CK_GENERALOPTIONS_CANUSECURRENTOBJECT", (void *)&g_CK_GENERALOPTIONS_CANUSECURRENTOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CK_GENERALOPTIONS_AUTOMATICUSECURRENT", (void *)&g_CK_GENERALOPTIONS_AUTOMATICUSECURRENT); assert(r >= 0);

    r = engine->RegisterGlobalProperty("const int CKCID_MAXMAXCLASSID", (void *)&g_CKCID_MAXMAXCLASSID); assert(r >= 0);

    // Windows messages
    r = engine->RegisterGlobalProperty("const int CKWM_BASE", (void *)&g_CKWM_BASE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_OK", (void *)&g_CKWM_OK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_CANCEL", (void *)&g_CKWM_CANCEL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_SETVALUE", (void *)&g_CKWM_SETVALUE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_GETVALUE", (void *)&g_CKWM_GETVALUE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_INIT", (void *)&g_CKWM_INIT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_PARAMPICK", (void *)&g_CKWM_PARAMPICK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_PICK", (void *)&g_CKWM_PICK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_SETPARAMTEXT", (void *)&g_CKWM_SETPARAMTEXT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_SIZECHANGED", (void *)&g_CKWM_SIZECHANGED); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_PARAMMODIFIED", (void *)&g_CKWM_PARAMMODIFIED); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_CREATEBEHAVIORLOCALS", (void *)&g_CKWM_CREATEBEHAVIORLOCALS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_SETMARGIN", (void *)&g_CKWM_SETMARGIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_GETMARGIN", (void *)&g_CKWM_GETMARGIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_STARTPICK", (void *)&g_CKWM_STARTPICK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const int CKWM_ENDPICK", (void *)&g_CKWM_ENDPICK); assert(r >= 0);

    // Preregistered Managers
    r = engine->RegisterGlobalProperty("const CKGUID OBJECT_MANAGER_GUID", (void *)&g_OBJECT_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID ATTRIBUTE_MANAGER_GUID", (void *)&g_ATTRIBUTE_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID MESSAGE_MANAGER_GUID", (void *)&g_MESSAGE_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID TIME_MANAGER_GUID", (void *)&g_TIME_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID SOUND_MANAGER_GUID", (void *)&g_SOUND_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID MIDI_MANAGER_GUID", (void *)&g_MIDI_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID INPUT_MANAGER_GUID", (void *)&g_INPUT_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID BEHAVIOR_MANAGER_GUID", (void *)&g_BEHAVIOR_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID FLOOR_MANAGER_GUID", (void *)&g_FLOOR_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID COLLISION_MANAGER_GUID", (void *)&g_COLLISION_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID GRID_MANAGER_GUID", (void *)&g_GRID_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID INTERFACE_MANAGER_GUID", (void *)&g_INTERFACE_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID RENDER_MANAGER_GUID", (void *)&g_RENDER_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID PARAMETER_MANAGER_GUID", (void *)&g_PARAMETER_MANAGER_GUID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID PATH_MANAGER_GUID", (void *)&g_PATH_MANAGER_GUID); assert(r >= 0);

    // Preregistered parameter types
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_NONE", (void *)&g_CKPGUID_NONE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_VOIDBUF", (void *)&g_CKPGUID_VOIDBUF); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FLOAT", (void *)&g_CKPGUID_FLOAT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ANGLE", (void *)&g_CKPGUID_ANGLE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_PERCENTAGE", (void *)&g_CKPGUID_PERCENTAGE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_INT", (void *)&g_CKPGUID_INT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_KEY", (void *)&g_CKPGUID_KEY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BOOL", (void *)&g_CKPGUID_BOOL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_STRING", (void *)&g_CKPGUID_STRING); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_RECT", (void *)&g_CKPGUID_RECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_VECTOR", (void *)&g_CKPGUID_VECTOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_2DVECTOR", (void *)&g_CKPGUID_2DVECTOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_QUATERNION", (void *)&g_CKPGUID_QUATERNION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_EULERANGLES", (void *)&g_CKPGUID_EULERANGLES); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MATRIX", (void *)&g_CKPGUID_MATRIX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_COLOR", (void *)&g_CKPGUID_COLOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BOX", (void *)&g_CKPGUID_BOX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBJECTARRAY", (void *)&g_CKPGUID_OBJECTARRAY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBJECT", (void *)&g_CKPGUID_OBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BEOBJECT", (void *)&g_CKPGUID_BEOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MESH", (void *)&g_CKPGUID_MESH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MATERIAL", (void *)&g_CKPGUID_MATERIAL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TEXTURE", (void *)&g_CKPGUID_TEXTURE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SPRITE", (void *)&g_CKPGUID_SPRITE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_3DENTITY", (void *)&g_CKPGUID_3DENTITY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CURVEPOINT", (void *)&g_CKPGUID_CURVEPOINT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_LIGHT", (void *)&g_CKPGUID_LIGHT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TARGETLIGHT", (void *)&g_CKPGUID_TARGETLIGHT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ID", (void *)&g_CKPGUID_ID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CAMERA", (void *)&g_CKPGUID_CAMERA); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TARGETCAMERA", (void *)&g_CKPGUID_TARGETCAMERA); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SPRITE3D", (void *)&g_CKPGUID_SPRITE3D); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBJECT3D", (void *)&g_CKPGUID_OBJECT3D); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BODYPART", (void *)&g_CKPGUID_BODYPART); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CHARACTER", (void *)&g_CKPGUID_CHARACTER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CURVE", (void *)&g_CKPGUID_CURVE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_2DCURVE", (void *)&g_CKPGUID_2DCURVE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_LEVEL", (void *)&g_CKPGUID_LEVEL); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_PLACE", (void *)&g_CKPGUID_PLACE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_GROUP", (void *)&g_CKPGUID_GROUP); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_2DENTITY", (void *)&g_CKPGUID_2DENTITY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_RENDEROBJECT", (void *)&g_CKPGUID_RENDEROBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SPRITETEXT", (void *)&g_CKPGUID_SPRITETEXT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SOUND", (void *)&g_CKPGUID_SOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_WAVESOUND", (void *)&g_CKPGUID_WAVESOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MIDISOUND", (void *)&g_CKPGUID_MIDISOUND); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBJECTANIMATION", (void *)&g_CKPGUID_OBJECTANIMATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ANIMATION", (void *)&g_CKPGUID_ANIMATION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_KINEMATICCHAIN", (void *)&g_CKPGUID_KINEMATICCHAIN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SCENE", (void *)&g_CKPGUID_SCENE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BEHAVIOR", (void *)&g_CKPGUID_BEHAVIOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MESSAGE", (void *)&g_CKPGUID_MESSAGE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SYNCHRO", (void *)&g_CKPGUID_SYNCHRO); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CRITICALSECTION", (void *)&g_CKPGUID_CRITICALSECTION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_STATE", (void *)&g_CKPGUID_STATE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ATTRIBUTE", (void *)&g_CKPGUID_ATTRIBUTE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_CLASSID", (void *)&g_CKPGUID_CLASSID); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_DIRECTION", (void *)&g_CKPGUID_DIRECTION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BLENDMODE", (void *)&g_CKPGUID_BLENDMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FILTERMODE", (void *)&g_CKPGUID_FILTERMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BLENDFACTOR", (void *)&g_CKPGUID_BLENDFACTOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FILLMODE", (void *)&g_CKPGUID_FILLMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_LITMODE", (void *)&g_CKPGUID_LITMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SHADEMODE", (void *)&g_CKPGUID_SHADEMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_GLOBALEXMODE", (void *)&g_CKPGUID_GLOBALEXMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ZFUNC", (void *)&g_CKPGUID_ZFUNC); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ADDRESSMODE", (void *)&g_CKPGUID_ADDRESSMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_WRAPMODE", (void *)&g_CKPGUID_WRAPMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_3DSPRITEMODE", (void *)&g_CKPGUID_3DSPRITEMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FOGMODE", (void *)&g_CKPGUID_FOGMODE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_LIGHTTYPE", (void *)&g_CKPGUID_LIGHTTYPE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SPRITEALIGN", (void *)&g_CKPGUID_SPRITEALIGN); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SCRIPT", (void *)&g_CKPGUID_SCRIPT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_LAYERTYPE", (void *)&g_CKPGUID_LAYERTYPE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_STATECHUNK", (void *)&g_CKPGUID_STATECHUNK); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_DATAARRAY", (void *)&g_CKPGUID_DATAARRAY); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_COMPOPERATOR", (void *)&g_CKPGUID_COMPOPERATOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BINARYOPERATOR", (void *)&g_CKPGUID_BINARYOPERATOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SETOPERATOR", (void *)&g_CKPGUID_SETOPERATOR); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SPRITETEXTALIGNMENT", (void *)&g_CKPGUID_SPRITETEXTALIGNMENT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBSTACLEPRECISION", (void *)&g_CKPGUID_OBSTACLEPRECISION); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBSTACLEPRECISIONBEH", (void *)&g_CKPGUID_OBSTACLEPRECISIONBEH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OBSTACLE", (void *)&g_CKPGUID_OBSTACLE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_PATCHMESH", (void *)&g_CKPGUID_PATCHMESH); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_POINTER", (void *)&g_CKPGUID_POINTER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ENUMS", (void *)&g_CKPGUID_ENUMS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_STRUCTS", (void *)&g_CKPGUID_STRUCTS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FLAGS", (void *)&g_CKPGUID_FLAGS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_FILTER", (void *)&g_CKPGUID_FILTER); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TIME", (void *)&g_CKPGUID_TIME); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OLDTIME", (void *)&g_CKPGUID_OLDTIME); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_COPYDEPENDENCIES", (void *)&g_CKPGUID_COPYDEPENDENCIES); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_DELETEDEPENDENCIES", (void *)&g_CKPGUID_DELETEDEPENDENCIES); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SAVEDEPENDENCIES", (void *)&g_CKPGUID_SAVEDEPENDENCIES); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_REPLACEDEPENDENCIES", (void *)&g_CKPGUID_REPLACEDEPENDENCIES); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SCENEACTIVITYFLAGS", (void *)&g_CKPGUID_SCENEACTIVITYFLAGS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SCENEOBJECT", (void *)&g_CKPGUID_SCENEOBJECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SCENERESETFLAGS", (void *)&g_CKPGUID_SCENERESETFLAGS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_ARRAYTYPE", (void *)&g_CKPGUID_ARRAYTYPE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_RENDEROPTIONS", (void *)&g_CKPGUID_RENDEROPTIONS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_PARAMETERTYPE", (void *)&g_CKPGUID_PARAMETERTYPE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_MATERIALEFFECT", (void *)&g_CKPGUID_MATERIALEFFECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TEXGENEFFECT", (void *)&g_CKPGUID_TEXGENEFFECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TEXGENREFEFFECT", (void *)&g_CKPGUID_TEXGENREFEFFECT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_COMBINE2TEX", (void *)&g_CKPGUID_COMBINE2TEX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_COMBINE3TEX", (void *)&g_CKPGUID_COMBINE3TEX); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BUMPMAPPARAM", (void *)&g_CKPGUID_BUMPMAPPARAM); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_TEXCOMBINE", (void *)&g_CKPGUID_TEXCOMBINE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_PIXELFORMAT", (void *)&g_CKPGUID_PIXELFORMAT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_AXIS", (void *)&g_CKPGUID_AXIS); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_SUPPORT", (void *)&g_CKPGUID_SUPPORT); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_BITMAP_SYSTEMCACHING", (void *)&g_CKPGUID_BITMAP_SYSTEMCACHING); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OLDMESSAGE", (void *)&g_CKPGUID_OLDMESSAGE); assert(r >= 0);
    r = engine->RegisterGlobalProperty("const CKGUID CKPGUID_OLDATTRIBUTE", (void *)&g_CKPGUID_OLDATTRIBUTE); assert(r >= 0);
}

void RegisterCKGlobalFunctions(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterGlobalFunction("string CKErrorToString(CKERROR err)", asFUNCTIONPR([](CKERROR err) -> std::string { return ScriptStringify(CKErrorToString(err)); }, (CKERROR), std::string), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKERROR CKStartUp()", asFUNCTION(CKStartUp), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKShutdown()", asFUNCTION(CKShutdown), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKContext@ GetCKContext(int pos)", asFUNCTION(GetCKContext), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKObject@ CKGetObject(CKContext@ context, CK_ID id)", asFUNCTION(CKGetObject), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKCreateContext(CKContext@ &out context, WIN_HANDLE win, int renderEngine, CKDWORD flags)", asFUNCTION(CKCreateContext), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKCloseContext(CKContext@ context)", asFUNCTION(CKCloseContext), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("string CKGetStartPath()", asFUNCTIONPR([]() -> std::string { return ScriptStringify(CKGetStartPath()); }, (), std::string), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("string CKGetPluginsPath()", asFUNCTIONPR([]() -> std::string { return ScriptStringify(CKGetPluginsPath()); }, (), std::string), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in dep = void)", asFUNCTION(CKDestroyObject), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKDWORD CKGetVersion()", asFUNCTION(CKGetVersion), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKBuildClassHierarchyTable()", asFUNCTION(CKBuildClassHierarchyTable), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKPluginManager@ CKGetPluginManager()", asFUNCTION(CKGetPluginManager), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("int CKGetPrototypeDeclarationCount()", asFUNCTION(CKGetPrototypeDeclarationCount), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKObjectDeclaration@ CKGetPrototypeDeclaration(int n)", asFUNCTION(CKGetPrototypeDeclaration), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("XObjDeclHashTableIt CKGetPrototypeDeclarationStartIterator()", asFUNCTION(CKGetPrototypeDeclarationStartIterator), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("XObjDeclHashTableIt CKGetPrototypeDeclarationEndIterator()", asFUNCTION(CKGetPrototypeDeclarationEndIterator), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKObjectDeclaration@ CKGetObjectDeclarationFromGuid(CKGUID guid)", asFUNCTION(CKGetObjectDeclarationFromGuid), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKBehaviorPrototype@ CKGetPrototypeFromGuid(CKGUID guid)", asFUNCTION(CKGetPrototypeFromGuid), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKRemovePrototypeDeclaration(CKObjectDeclaration@ decl)", asFUNCTION(CKRemovePrototypeDeclaration), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKObjectDeclaration@ CreateCKObjectDeclaration(const string &in name)", asFUNCTIONPR([](const std::string &name) { return CreateCKObjectDeclaration(const_cast<CKSTRING>(name.c_str())); }, (const std::string &), CKObjectDeclaration *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKBehaviorPrototype@ CreateCKBehaviorPrototype(const string &in name)", asFUNCTIONPR([](const std::string &name) { return CreateCKBehaviorPrototype(const_cast<CKSTRING>(name.c_str())); }, (const std::string &), CKBehaviorPrototype *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKBehaviorPrototype@ CreateCKBehaviorPrototypeRunTime(const string &in name)", asFUNCTIONPR([](const std::string &name) { return CreateCKBehaviorPrototypeRunTime(const_cast<CKSTRING>(name.c_str())); }, (const std::string &), CKBehaviorPrototype *), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("int CKGetClassCount()", asFUNCTION(CKGetClassCount), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKClassDesc &CKGetClassDesc(CK_CLASSID cid)", asFUNCTION(CKGetClassDesc), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("string CKClassIDToString(CK_CLASSID cid)", asFUNCTIONPR([](CK_CLASSID cid) -> std::string { return ScriptStringify(CKClassIDToString(cid)); }, (CK_CLASSID), std::string), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK_CLASSID CKStringToClassID(const string &in className)", asFUNCTIONPR([](const std::string &className) { return CKStringToClassID(const_cast<CKSTRING>(className.c_str())); }, (const std::string &), CK_CLASSID), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("bool CKIsChildClassOf(CK_CLASSID child, CK_CLASSID parent)", asFUNCTIONPR([](CK_CLASSID child, CK_CLASSID parent) -> bool { return CKIsChildClassOf(child, parent); }, (CK_CLASSID, CK_CLASSID), bool), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool CKIsChildClassOf(CKObject@ obj, CK_CLASSID parent)", asFUNCTIONPR([](CKObject *obj, CK_CLASSID parent) -> bool { return CKIsChildClassOf(obj, parent); }, (CKObject *, CK_CLASSID), bool), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK_CLASSID CKGetParentClassID(CK_CLASSID child)", asFUNCTIONPR(CKGetParentClassID, (CK_CLASSID), CK_CLASSID), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK_CLASSID CKGetParentClassID(CKObject@ obj)", asFUNCTIONPR(CKGetParentClassID, (CKObject *), CK_CLASSID), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK_CLASSID CKGetCommonParent(CK_CLASSID cid1, CK_CLASSID cid2)", asFUNCTION(CKGetCommonParent), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKObjectArray@ CreateCKObjectArray()", asFUNCTION(CreateCKObjectArray), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void DeleteCKObjectArray(CKObjectArray@ obj)", asFUNCTION(DeleteCKObjectArray), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKStateChunk@ CreateCKStateChunk(CK_CLASSID id, CKFile@ file = null)", asFUNCTIONPR(CreateCKStateChunk, (CK_CLASSID, CKFile *), CKStateChunk *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKStateChunk@ CreateCKStateChunk(CKStateChunk@ chunk)", asFUNCTIONPR(CreateCKStateChunk, (CKStateChunk *), CKStateChunk *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void DeleteCKStateChunk(CKStateChunk@ chunk)", asFUNCTIONPR(DeleteCKStateChunk, (CKStateChunk *), void), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKStateChunk@ CKSaveObjectState(CKObject@ obj, CKDWORD flags = CK_STATESAVE_ALL)", asFUNCTIONPR(CKSaveObjectState, (CKObject *, CKDWORD), CKStateChunk *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKReadObjectState(CKObject@ obj, CKStateChunk@ chunk)", asFUNCTIONPR(CKReadObjectState, (CKObject *, CKStateChunk *), CKERROR), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("BITMAP_HANDLE CKLoadBitmap(const string &in filename)", asFUNCTIONPR([](const std::string &filename) { return CKLoadBitmap(const_cast<CKSTRING>(filename.c_str())); }, (const std::string &), BITMAP_HANDLE), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool CKSaveBitmap(const string &in filename, BITMAP_HANDLE bm)", asFUNCTIONPR([](const std::string &filename, BITMAP_HANDLE bm) -> bool { return CKSaveBitmap(const_cast<CKSTRING>(filename.c_str()), bm); }, (const std::string &, BITMAP_HANDLE), bool), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool CKSaveBitmap(const string &in filename, VxImageDescEx &in desc)", asFUNCTIONPR([](const std::string &filename, VxImageDescEx &desc) -> bool { return CKSaveBitmap(const_cast<CKSTRING>(filename.c_str()), desc); }, (const std::string &, VxImageDescEx &), bool), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKConvertEndianArray32(NativePointer buf, int dwordCount)", asFUNCTIONPR([](NativePointer buf, int dwordCount) { CKConvertEndianArray32(buf.Get(), dwordCount); }, (NativePointer, int), void), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void CKConvertEndianArray16(NativePointer buf, int dwordCount)", asFUNCTIONPR([](NativePointer buf, int dwordCount) { CKConvertEndianArray16(buf.Get(), dwordCount); }, (NativePointer, int), void), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKDWORD CKConvertEndian32(CKDWORD dw)", asFUNCTION(CKConvertEndian32), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKWORD CKConvertEndian16(CKWORD w)", asFUNCTION(CKConvertEndian16), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKDWORD CKComputeDataCRC(NativePointer data, int size, CKDWORD previousCRC = 0)", asFUNCTIONPR([](NativePointer data, int size, CKDWORD previousCRC) { return CKComputeDataCRC(data.Get(), size, previousCRC); }, (NativePointer, int, CKDWORD), CKDWORD), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKPackData(NativePointer data, int size, int &out newSize, int compressionLevel)", asFUNCTIONPR([](NativePointer data, int size, int &newSize, int compressionlevel) { return NativePointer(CKPackData(data.Get(), size, newSize, compressionlevel)); }, (NativePointer, int, int&, int), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKUnPackData(int destSize, NativePointer srcBuffer, int srcSize)", asFUNCTIONPR([](int destSize, NativePointer srcBuffer, int srcSize) { return NativePointer(CKUnPackData(destSize, srcBuffer.Get(), srcSize)); }, (int, NativePointer, int), NativePointer), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("NativePointer CKStrdup(NativePointer str)", asFUNCTIONPR([](NativePointer str) { return NativePointer(CKStrdup(str.Get())); }, (NativePointer), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKStrdup(const string &in str)", asFUNCTIONPR([](const std::string &str) { return NativePointer(CKStrdup(const_cast<CKSTRING>(str.c_str()))); }, (const std::string &), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKStrupr(NativePointer str)", asFUNCTIONPR([](NativePointer str) { return NativePointer(CKStrupr(str.Get())); }, (NativePointer), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKStrupr(const string &in str)", asFUNCTIONPR([](const std::string &str) { return NativePointer(CKStrupr(const_cast<CKSTRING>(str.c_str()))); }, (const std::string &), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKStrlwr(NativePointer str)", asFUNCTIONPR([](NativePointer str) { return NativePointer(CKStrlwr(str.Get())); }, (NativePointer), NativePointer), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("NativePointer CKStrlwr(const string &in str)", asFUNCTIONPR([](const std::string &str) { return NativePointer(CKStrlwr(const_cast<CKSTRING>(str.c_str()))); }, (const std::string &), NativePointer), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKBitmapProperties@ CKCopyBitmapProperties(CKBitmapProperties@ bp)", asFUNCTION(CKCopyBitmapProperties), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void CKDeleteBitmapProperties(CKBitmapProperties@ bp)", asFUNCTION(CKDeletePointer), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKCopyDefaultClassDependencies(CKDependencies &out d, CK_DEPENDENCIES_OPMODE mode)", asFUNCTION(CKCopyDefaultClassDependencies), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKDependencies &CKGetDefaultClassDependencies(CK_DEPENDENCIES_OPMODE mode)", asFUNCTION(CKGetDefaultClassDependencies), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKDeletePointer(NativePointer ptr)", asFUNCTIONPR([](NativePointer ptr) { CKDeletePointer(ptr.Get()); }, (NativePointer), void), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKERROR CKCopyAllAttributes(CKBeObject@ src, CKBeObject@ dest)", asFUNCTION(CKCopyAllAttributes), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKMoveAllScripts(CKBeObject@ src, CKBeObject@ dest)", asFUNCTION(CKMoveAllScripts), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKERROR CKMoveScript(CKBeObject@ src, CKBeObject@ dest, CKBehavior@ beh)", asFUNCTION(CKMoveScript), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void CKRemapObjectParameterValue(CKContext@ context, CK_ID oldID, CK_ID newID, CK_CLASSID cid = CKCID_OBJECT, bool derived = true)", asFUNCTIONPR([](CKContext *context, CK_ID oldID, CK_ID newID, CK_CLASSID cid, bool derived) { CKRemapObjectParameterValue(context, oldID, newID, cid, derived); }, (CKContext *, CK_ID, CK_ID, CK_CLASSID, bool), void), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("void CKStoreDeclaration(XObjectDeclarationArray &out reg, CKObjectDeclaration@ a)", asFUNCTION(CKStoreDeclaration), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterGlobalFunction("CKDWORD GetCurrentFileLoadOption()", asFUNCTION(GetCurrentFileLoadOption), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKDWORD GetCurrentFileVersion()", asFUNCTION(GetCurrentFileVersion), asCALL_CDECL); assert(r >= 0);

    // r = engine->RegisterGlobalFunction("void CKClassNeedNotificationFrom(CK_CLASSID cid1, CK_CLASSID cid2)", asFUNCTION(CKClassNeedNotificationFrom), asCALL_CDECL); assert(r >= 0);
    // r = engine->RegisterGlobalFunction("void CKClassRegisterAssociatedParameter(CK_CLASSID cid, CKGUID guid)", asFUNCTION(CKClassRegisterAssociatedParameter), asCALL_CDECL); assert(r >= 0);
    // r = engine->RegisterGlobalFunction("void CKClassRegisterDefaultDependencies(CK_CLASSID cid, CKDWORD dependMask, int mode)", asFUNCTION(CKClassRegisterDefaultDependencies), asCALL_CDECL); assert(r >= 0);
    // r = engine->RegisterGlobalFunction("void CKClassRegisterDefaultOptions(CK_CLASSID cid, CKDWORD optionsMask)", asFUNCTION(CKClassRegisterDefaultOptions), asCALL_CDECL); assert(r >= 0);
    // r = engine->RegisterGlobalFunction("CK_CLASSID CKClassGetNewIdentifier()", asFUNCTION(CKClassGetNewIdentifier), asCALL_CDECL); assert(r >= 0);
    // r = engine->RegisterGlobalFunction("void CKClassRegister(CK_CLASSID cid, CK_CLASSID parentCid, CKCLASSREGISTERFCT, CKCLASSCREATIONFCT, CKCLASSNAMEFCT, CKCLASSDEPENDENCIESFCT, CKCLASSDEPENDENCIESCOUNTFCT)", asFUNCTION(CKClassRegister), asCALL_CDECL); assert(r >= 0);
}

// CKFileInfo

void RegisterCKFileInfo(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD ProductVersion", offsetof(CKFileInfo, ProductVersion)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD ProductBuild", offsetof(CKFileInfo, ProductBuild)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD FileWriteMode", offsetof(CKFileInfo, FileWriteMode)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD FileVersion", offsetof(CKFileInfo, FileVersion)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD CKVersion", offsetof(CKFileInfo, CKVersion)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD FileSize", offsetof(CKFileInfo, FileSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD ObjectCount", offsetof(CKFileInfo, ObjectCount)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD ManagerCount", offsetof(CKFileInfo, ManagerCount)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD MaxIDSaved", offsetof(CKFileInfo, MaxIDSaved)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD Crc", offsetof(CKFileInfo, Crc)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD Hdr1PackSize", offsetof(CKFileInfo, Hdr1PackSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD Hdr1UnPackSize", offsetof(CKFileInfo, Hdr1UnPackSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD DataPackSize", offsetof(CKFileInfo, DataPackSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileInfo", "CKDWORD DataUnPackSize", offsetof(CKFileInfo, DataUnPackSize)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileInfo", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFileInfo *self) { new(self) CKFileInfo(); }, (CKFileInfo*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFileInfo", asBEHAVE_CONSTRUCT, "void f(const CKFileInfo &in other)", asFUNCTIONPR([](const CKFileInfo &info, CKFileInfo *self) { new(self) CKFileInfo(info); }, (const CKFileInfo &, CKFileInfo *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileInfo", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFileInfo *self) { self->~CKFileInfo(); }, (CKFileInfo *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFileInfo", "CKFileInfo &opAssign(const CKFileInfo &in other)", asMETHODPR(CKFileInfo, operator=, (const CKFileInfo &), CKFileInfo &), asCALL_THISCALL); assert(r >= 0);
}

// CKStats

static float CKStatsGetUserProfile(const CKStats &stats, int index) {
    if (index < 0 || index >= MAX_USER_PROFILE) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return 0.0f;
    }
    return stats.UserProfiles[index];
}

static void CKStatsSetUserProfile(CKStats &stats, int index, float value) {
    if (index < 0 || index >= MAX_USER_PROFILE) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return;
    }
    stats.UserProfiles[index] = value;
}

void RegisterCKStats(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKStats", "float TotalFrameTime", offsetof(CKStats, TotalFrameTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float EstimatedInterfaceTime", offsetof(CKStats, EstimatedInterfaceTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float ProcessTime", offsetof(CKStats, ProcessTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float RenderTime", offsetof(CKStats, RenderTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float ParametricOperations", offsetof(CKStats, ParametricOperations)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float TotalBehaviorExecution", offsetof(CKStats, TotalBehaviorExecution)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float AnimationManagement", offsetof(CKStats, AnimationManagement)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float IKManagement", offsetof(CKStats, IKManagement)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "float BehaviorCodeExecution", offsetof(CKStats, BehaviorCodeExecution)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "int ActiveObjectsExecuted", offsetof(CKStats, ActiveObjectsExecuted)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "int BehaviorsExecuted", offsetof(CKStats, BehaviorsExecuted)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "int BuildingBlockExecuted", offsetof(CKStats, BuildingBlockExecuted)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "int BehaviorLinksParsed", offsetof(CKStats, BehaviorLinksParsed)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKStats", "int BehaviorDelayedLinks", offsetof(CKStats, BehaviorDelayedLinks)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKStats", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKStats *self) { new(self) CKStats(); }, (CKStats*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKStats", asBEHAVE_CONSTRUCT, "void f(const CKStats &in other)", asFUNCTIONPR([](const CKStats &stats, CKStats *self) { new(self) CKStats(stats); }, (const CKStats &, CKStats *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKStats", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKStats *self) { self->~CKStats(); }, (CKStats *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStats", "CKStats &opAssign(const CKStats &in other)", asMETHODPR(CKStats, operator=, (const CKStats &), CKStats &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStats", "float GetUserProfile(int index) const", asFUNCTION(CKStatsGetUserProfile), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStats", "void SetUserProfile(int index, float value)", asFUNCTION(CKStatsSetUserProfile), asCALL_CDECL_OBJFIRST); assert(r >= 0);

}

// VxDriverDesc

static std::string VxDriverDescGetDriverDesc(const VxDriverDesc &desc) {
#if CKVERSION == 0x13022002
    return desc.DriverDesc;
#else
    return desc.DriverDesc.CStr();
#endif
}

static void VxDriverDescSetDriverDesc(VxDriverDesc &desc, const std::string &value) {
#if CKVERSION == 0x13022002
    strncpy(desc.DriverDesc, value.c_str(), sizeof(desc.DriverDesc) - 1);
    desc.DriverDesc[value.length()] = '\0';
#else
    desc.DriverDesc = value.c_str();
#endif
}

static std::string VxDriverDescGetDriverName(const VxDriverDesc &desc) {
#if CKVERSION == 0x13022002
    return desc.DriverName;
#else
    return desc.DriverName.CStr();
#endif
}

static void VxDriverDescSetDriverName(VxDriverDesc &desc, const std::string &value) {
#if CKVERSION == 0x13022002
    strncpy(desc.DriverName, value.c_str(), sizeof(desc.DriverName) - 1);
    desc.DriverName[value.length()] = '\0';
#else
    desc.DriverName = value.c_str();
#endif
}

static VxDisplayMode *VxDriverDescGetDisplayMode(VxDriverDesc &desc, int index) {
#if CKVERSION == 0x13022002
    if (index < 0 || index >= desc.DisplayModeCount) {
#else
    if (index < 0 || index >= desc.DisplayModes.Size()) {
#endif
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return nullptr;
    }
    return &desc.DisplayModes[index];
}

static void VxDriverDescSetDisplayMode(VxDriverDesc &desc, int index, const VxDisplayMode &value) {
#if CKVERSION == 0x13022002
    if (index < 0 || index >= desc.DisplayModeCount) {
#else
    if (index < 0 || index >= desc.DisplayModes.Size()) {
#endif
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return;
    }
    desc.DisplayModes[index] = value;
}

static VxImageDescEx *VxDriverDescGetTextureFormat(const VxDriverDesc &desc, int index) {
    if (index < 0 || index >= desc.TextureFormats.Size()) {
        // Set a script exception if the index is out of range
        asIScriptContext *ctx = asGetActiveContext();
        if (ctx) ctx->SetException("Index out of range");
        return nullptr;
    }
    return &desc.TextureFormats[index];
}

static void VxDriverDescSetTextureFormat(VxDriverDesc &desc, int index, const VxImageDescEx &value) {
    if (index < 0 || index >= desc.TextureFormats.Size()) {
        // Set a script exception if the index is out of range
        asIScriptContext *ctx = asGetActiveContext();
        if (ctx) ctx->SetException("Index out of range");
        return;
    }
    desc.TextureFormats[index] = value;
}

void RegisterVxDriverDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("VxDriverDesc", "int IsHardware", offsetof(VxDriverDesc, IsHardware)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxDriverDesc", "Vx2DCapsDesc Caps2D", offsetof(VxDriverDesc, Caps2D)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxDriverDesc", "Vx3DCapsDesc Caps3D", offsetof(VxDriverDesc, Caps3D)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxDriverDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](VxDriverDesc *self) { new(self) VxDriverDesc(); }, (VxDriverDesc*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("VxDriverDesc", asBEHAVE_CONSTRUCT, "void f(const VxDriverDesc &in other)", asFUNCTIONPR([](const VxDriverDesc &desc, VxDriverDesc *self) { new(self) VxDriverDesc(desc); }, (const VxDriverDesc &, VxDriverDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxDriverDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](VxDriverDesc *self) { self->~VxDriverDesc(); }, (VxDriverDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxDriverDesc", "VxDriverDesc &opAssign(const VxDriverDesc &in other)", asMETHODPR(VxDriverDesc, operator=, (const VxDriverDesc &), VxDriverDesc &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxDriverDesc", "string GetDriverDesc() const", asFUNCTION(VxDriverDescGetDriverDesc), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "void SetDriverDesc(const string &in desc)", asFUNCTION(VxDriverDescSetDriverDesc), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "string GetDriverName() const", asFUNCTION(VxDriverDescGetDriverName), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "void SetDriverName(const string &in name)", asFUNCTION(VxDriverDescSetDriverName), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "VxDisplayMode &GetDisplayMode(int index) const", asFUNCTION(VxDriverDescGetDisplayMode), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "void SetDisplayMode(int index, const VxDisplayMode &in mode)", asFUNCTION(VxDriverDescSetDisplayMode), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "VxImageDescEx &GetTextureFormat(int index) const", asFUNCTION(VxDriverDescGetTextureFormat), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxDriverDesc", "void SetTextureFormat(int index, const VxImageDescEx &in format)", asFUNCTION(VxDriverDescSetTextureFormat), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// VxIntersectionDesc

void RegisterVxIntersectionDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("VxIntersectionDesc", "CKRenderObject@ Object", offsetof(VxIntersectionDesc, Object)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "VxVector IntersectionPoint", offsetof(VxIntersectionDesc, IntersectionPoint)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "VxVector IntersectionNormal", offsetof(VxIntersectionDesc, IntersectionNormal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "float TexU", offsetof(VxIntersectionDesc, TexU)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "float TexV", offsetof(VxIntersectionDesc, TexV)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "float Distance", offsetof(VxIntersectionDesc, Distance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxIntersectionDesc", "int FaceIndex", offsetof(VxIntersectionDesc, FaceIndex)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxIntersectionDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](VxIntersectionDesc *self) { new(self) VxIntersectionDesc(); }, (VxIntersectionDesc*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("VxIntersectionDesc", asBEHAVE_CONSTRUCT, "void f(const VxIntersectionDesc &in other)", asFUNCTIONPR([](const VxIntersectionDesc &desc, VxIntersectionDesc *self) { new(self) VxIntersectionDesc(desc); }, (const VxIntersectionDesc &, VxIntersectionDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxIntersectionDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](VxIntersectionDesc *self) { self->~VxIntersectionDesc(); }, (VxIntersectionDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxIntersectionDesc", "VxIntersectionDesc &opAssign(const VxIntersectionDesc &in other)", asMETHODPR(VxIntersectionDesc, operator=, (const VxIntersectionDesc &), VxIntersectionDesc &), asCALL_THISCALL); assert(r >= 0);
}

// VxStats

void RegisterVxStats(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("VxStats", "int NbTrianglesDrawn", offsetof(VxStats, NbTrianglesDrawn)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int NbPointsDrawn", offsetof(VxStats, NbPointsDrawn)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int NbLinesDrawn", offsetof(VxStats, NbLinesDrawn)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int NbVerticesProcessed", offsetof(VxStats, NbVerticesProcessed)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int NbObjectDrawn", offsetof(VxStats, NbObjectDrawn)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float SmoothedFps", offsetof(VxStats, SmoothedFps)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int RenderStateCacheHit", offsetof(VxStats, RenderStateCacheHit)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "int RenderStateCacheMiss", offsetof(VxStats, RenderStateCacheMiss)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float DevicePreCallbacks", offsetof(VxStats, DevicePreCallbacks)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float SceneTraversalTime", offsetof(VxStats, SceneTraversalTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float TransparentObjectsSortTime", offsetof(VxStats, TransparentObjectsSortTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float ObjectsRenderTime", offsetof(VxStats, ObjectsRenderTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float ObjectsCallbacksTime", offsetof(VxStats, ObjectsCallbacksTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float SkinTime", offsetof(VxStats, SkinTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float SpriteTime", offsetof(VxStats, SpriteTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float SpriteCallbacksTime", offsetof(VxStats, SpriteCallbacksTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float DevicePostCallbacks", offsetof(VxStats, DevicePostCallbacks)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxStats", "float BackToFrontTime", offsetof(VxStats, BackToFrontTime)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxStats", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](VxStats *self) { new(self) VxStats(); }, (VxStats*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("VxStats", asBEHAVE_CONSTRUCT, "void f(const VxStats &in other)", asFUNCTIONPR([](const VxStats &stats, VxStats *self) { new(self) VxStats(stats); }, (const VxStats &, VxStats *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxStats", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](VxStats *self) { self->~VxStats(); }, (VxStats *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxStats", "VxStats &opAssign(const VxStats &in other)", asMETHODPR(VxStats, operator=, (const VxStats &), VxStats &), asCALL_THISCALL); assert(r >= 0);
}

// CKGUID

void RegisterCKGUID(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKGUID", "CKDWORD d1", offsetof(CKGUID, d1)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKGUID", "CKDWORD d2", offsetof(CKGUID, d2)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKGUID", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKGUID *self) { new(self) CKGUID(); }, (CKGUID *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKGUID", asBEHAVE_CONSTRUCT, "void f(CKDWORD gd1 = 0, CKDWORD gd2 = 0)", asFUNCTIONPR([](CKDWORD gd1, CKDWORD gd2, CKGUID *self) { new(self) CKGUID(gd1, gd2); }, (CKDWORD, CKDWORD, CKGUID *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKGUID", asBEHAVE_CONSTRUCT, "void f(const CKGUID &in other)", asFUNCTIONPR([](const CKGUID &guid, CKGUID *self) { new(self) CKGUID(guid); }, (const CKGUID &, CKGUID *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKGUID", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {CKDWORD, CKDWORD}", asFUNCTIONPR([](CKDWORD *list, CKGUID *self) { new(self) CKGUID(list[0], list[1]); }, (CKDWORD *, CKGUID *), void), asCALL_CDECL_OBJLAST); assert( r >= 0 );

    r = engine->RegisterObjectBehaviour("CKGUID", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKGUID *self) { self->~CKGUID(); }, (CKGUID *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGUID", "CKGUID &opAssign(const CKGUID &in other)", asMETHODPR(CKGUID, operator=, (const CKGUID &), CKGUID &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGUID", "int opCmp(const CKGUID &in other) const", asFUNCTIONPR([](const CKGUID &lhs, const CKGUID &rhs) -> int { if (lhs == rhs) return 0; else if (lhs < rhs) return -1; else return 1; }, (const CKGUID &, const CKGUID &), int), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    r = engine->RegisterObjectMethod("CKGUID", "bool IsValid() const", asFUNCTIONPR([](CKGUID &guid) -> bool { return guid.IsValid(); }, (CKGUID &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// VxEffectDescription

void RegisterVxEffectDescription(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("VxEffectDescription", "VX_EFFECT EffectIndex", asOFFSET(VxEffectDescription, EffectIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString Summary", asOFFSET(VxEffectDescription, Summary)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString Description", asOFFSET(VxEffectDescription, Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString DescImage", asOFFSET(VxEffectDescription, DescImage)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "int MaxTextureCount", asOFFSET(VxEffectDescription, MaxTextureCount)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "int NeededTextureCoordsCount", asOFFSET(VxEffectDescription, NeededTextureCoordsCount)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString Tex1Description", asOFFSET(VxEffectDescription, Tex1Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString Tex2Description", asOFFSET(VxEffectDescription, Tex2Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString Tex3Description", asOFFSET(VxEffectDescription, Tex3Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "uintptr_t SetCallback", asOFFSET(VxEffectDescription, SetCallback)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "uintptr_t CallbackArg", asOFFSET(VxEffectDescription, CallbackArg)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "CKGUID ParameterType", asOFFSET(VxEffectDescription, ParameterType)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString ParameterDescription", asOFFSET(VxEffectDescription, ParameterDescription)); assert(r >= 0);
    r = engine->RegisterObjectProperty("VxEffectDescription", "XString ParameterDefaultValue", asOFFSET(VxEffectDescription, ParameterDefaultValue)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxEffectDescription", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](VxEffectDescription *self) { new(self) VxEffectDescription(); }, (VxEffectDescription *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("VxEffectDescription", asBEHAVE_CONSTRUCT, "void f(const VxEffectDescription &in other)", asFUNCTIONPR([](const VxEffectDescription &desc, VxEffectDescription *self) { new(self) VxEffectDescription(desc); }, (const VxEffectDescription &, VxEffectDescription *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("VxEffectDescription", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](VxEffectDescription *self) { self->~VxEffectDescription(); }, (VxEffectDescription *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxEffectDescription", "VxEffectDescription &opAssign(const VxEffectDescription &in other)", asMETHODPR(VxEffectDescription, operator=, (const VxEffectDescription &), VxEffectDescription &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxEffectDescription", "NativePointer get_SetCallback() const", asFUNCTIONPR([](const VxEffectDescription *self) { return NativePointer(self->SetCallback); }, (const VxEffectDescription *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxEffectDescription", "void set_SetCallback(NativePointer ptr)", asFUNCTIONPR([](VxEffectDescription *self, NativePointer ptr) { self->SetCallback = reinterpret_cast<CK_EFFECTCALLBACK>(ptr.Get()); }, (VxEffectDescription *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("VxEffectDescription", "NativePointer get_CallbackArg() const", asFUNCTIONPR([](const VxEffectDescription *self) { return NativePointer(self->CallbackArg); }, (const VxEffectDescription *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("VxEffectDescription", "void set_CallbackArg(NativePointer ptr)", asFUNCTIONPR([](VxEffectDescription *self, NativePointer ptr) { self->CallbackArg = ptr.Get(); }, (VxEffectDescription *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKBehaviorContext

void RegisterCKBehaviorContext(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKBehavior@ Behavior", asOFFSET(CKBehaviorContext, Behavior)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "float DeltaTime", asOFFSET(CKBehaviorContext, DeltaTime)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKContext@ Context", asOFFSET(CKBehaviorContext, Context)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKLevel@ CurrentLevel", asOFFSET(CKBehaviorContext, CurrentLevel)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKScene@ CurrentScene", asOFFSET(CKBehaviorContext, CurrentScene)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKScene@ PreviousScene", asOFFSET(CKBehaviorContext, PreviousScene)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKRenderContext@ CurrentRenderContext", asOFFSET(CKBehaviorContext, CurrentRenderContext)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKParameterManager@ ParameterManager", asOFFSET(CKBehaviorContext, ParameterManager)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKMessageManager@ MessageManager", asOFFSET(CKBehaviorContext, MessageManager)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKAttributeManager@ AttributeManager", asOFFSET(CKBehaviorContext, AttributeManager)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKTimeManager@ TimeManager", asOFFSET(CKBehaviorContext, TimeManager)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "CKDWORD CallbackMessage", asOFFSET(CKBehaviorContext, CallbackMessage)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBehaviorContext", "uintptr_t CallbackArg", asOFFSET(CKBehaviorContext, CallbackArg)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBehaviorContext", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKBehaviorContext *self) { new(self) CKBehaviorContext(); }, (CKBehaviorContext *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBehaviorContext", asBEHAVE_CONSTRUCT, "void f(const CKBehaviorContext &in other)", asFUNCTIONPR([](const CKBehaviorContext &ctx, CKBehaviorContext *self) { new(self) CKBehaviorContext(ctx); }, (const CKBehaviorContext &, CKBehaviorContext *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBehaviorContext", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKBehaviorContext *self) { self->~CKBehaviorContext(); }, (CKBehaviorContext *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorContext", "CKBehaviorContext &opAssign(const CKBehaviorContext &in other)", asMETHODPR(CKBehaviorContext, operator=, (const CKBehaviorContext &), CKBehaviorContext &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorContext", "NativePointer get_CallbackArg() const", asFUNCTIONPR([](const CKBehaviorContext *self) { return NativePointer(self->CallbackArg); }, (const CKBehaviorContext *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorContext", "void set_CallbackArg(NativePointer ptr)", asFUNCTIONPR([](CKBehaviorContext *self, NativePointer ptr) { self->CallbackArg = ptr.Get(); }, (CKBehaviorContext *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKUICallbackStruct

void RegisterCKUICallbackStruct(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKUICallbackStruct", "CKDWORD Reason", asOFFSET(CKUICallbackStruct, Reason)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKUICallbackStruct", "CKDWORD Param3", asOFFSET(CKUICallbackStruct, Param3)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKUICallbackStruct", "CKDWORD get_Param1() const", asFUNCTIONPR([](const CKUICallbackStruct *self) { return self->Param1; }, (const CKUICallbackStruct *), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKUICallbackStruct", "void set_Param1(CKDWORD value)", asFUNCTIONPR([](CKUICallbackStruct *self, CKDWORD value) { self->Param1 = value; }, (CKUICallbackStruct *, CKDWORD), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKUICallbackStruct", "int get_NbObjectsLoaded() const", asFUNCTIONPR([](const CKUICallbackStruct *self) { return self->NbObjetsLoaded; }, (const CKUICallbackStruct *), int), asCALL_CDECL_OBJFIRST); assert(r >= 0); // NbObjetsLoaded not in our SDK
    // r = engine->RegisterObjectMethod("CKUICallbackStruct", "void set_NbObjectsLoaded(int value)", asFUNCTIONPR([](CKUICallbackStruct *self, int value) { self->NbObjetsLoaded = value; }, (CKUICallbackStruct *, int), void), asCALL_CDECL_OBJFIRST); assert(r >= 0); // NbObjetsLoaded not in our SDK

    r = engine->RegisterObjectMethod("CKUICallbackStruct", "bool get_DoBeep() const", asFUNCTIONPR([](const CKUICallbackStruct *self) -> bool { return self->DoBeep; }, (const CKUICallbackStruct *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKUICallbackStruct", "void set_DoBeep(bool value)", asFUNCTIONPR([](CKUICallbackStruct *self, bool value) { self->DoBeep = value; }, (CKUICallbackStruct *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKUICallbackStruct", "CKDWORD get_Param2() const", asFUNCTIONPR([](const CKUICallbackStruct *self) { return self->Param2; }, (const CKUICallbackStruct *), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKUICallbackStruct", "void set_Param2(CKDWORD value)", asFUNCTIONPR([](CKUICallbackStruct *self, CKDWORD value) { self->Param2 = value; }, (CKUICallbackStruct *, CKDWORD), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKUICallbackStruct", "string get_ConsoleString() const", asFUNCTIONPR([](const CKUICallbackStruct *self) -> std::string { return ScriptStringify(self->ConsoleString); }, (const CKUICallbackStruct *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKUICallbackStruct", "void set_ConsoleString(const string &in value)", asFUNCTIONPR([](CKUICallbackStruct *self, const std::string &value) { CKDeletePointer(self->ConsoleString); self->ConsoleString = CKStrdup(const_cast<CKSTRING>(value.c_str())); }, (CKUICallbackStruct *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKClassDesc

static CK_CLASSID CKClassDescGetToNotify(const CKClassDesc &desc, int index) {
    if (index < 0 || index >= desc.ToNotify.Size()) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return -1;
    }
    return desc.ToNotify[index];
}

static void CKClassDescSetToNotify(CKClassDesc &desc, int index, CK_CLASSID value) {
    if (index < 0 || index >= desc.ToNotify.Size()) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return;
    }
    desc.ToNotify[index] = value;
}

void RegisterCKClassDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKClassDesc", "int Done", asOFFSET(CKClassDesc, Done)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CK_CLASSID Parent", asOFFSET(CKClassDesc, Parent)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKClassDesc", "uintptr_t RegisterFct", asOFFSET(CKClassDesc, RegisterFct)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKClassDesc", "uintptr_t CreationFct", asOFFSET(CKClassDesc, CreationFct)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKClassDesc", "uintptr_t NameFct", asOFFSET(CKClassDesc, NameFct)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKClassDesc", "uintptr_t DependsFct", asOFFSET(CKClassDesc, DependsFct)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKClassDesc", "uintptr_t DependsCountFct", asOFFSET(CKClassDesc, DependsCountFct)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKDWORD DefaultOptions", asOFFSET(CKClassDesc, DefaultOptions)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKDWORD DefaultCopyDependencies", asOFFSET(CKClassDesc, DefaultCopyDependencies)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKDWORD DefaultDeleteDependencies", asOFFSET(CKClassDesc, DefaultDeleteDependencies)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKDWORD DefaultReplaceDependencies", asOFFSET(CKClassDesc, DefaultReplaceDependencies)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKDWORD DefaultSaveDependencies", asOFFSET(CKClassDesc, DefaultSaveDependencies)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "CKGUID Parameter", asOFFSET(CKClassDesc, Parameter)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "int DerivationLevel", asOFFSET(CKClassDesc, DerivationLevel)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "XBitArray Parents", asOFFSET(CKClassDesc, Parents)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "XBitArray Children", asOFFSET(CKClassDesc, Children)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "XBitArray ToBeNotify", asOFFSET(CKClassDesc, ToBeNotify)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "XBitArray CommonToBeNotify", asOFFSET(CKClassDesc, CommonToBeNotify)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKClassDesc", "XClassIDArray ToNotify", asOFFSET(CKClassDesc, ToNotify)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKClassDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKClassDesc *self) { new(self) CKClassDesc(); }, (CKClassDesc*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKClassDesc", asBEHAVE_CONSTRUCT, "void f(const CKClassDesc &in other)", asFUNCTIONPR([](const CKClassDesc &desc, CKClassDesc *self) { new(self) CKClassDesc(desc); }, (const CKClassDesc &, CKClassDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKClassDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKClassDesc *self) { self->~CKClassDesc(); }, (CKClassDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "CKClassDesc &opAssign(const CKClassDesc &in other)", asMETHODPR(CKClassDesc, operator=, (const CKClassDesc &), CKClassDesc &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "CK_CLASSID GetToNotify(int index) const", asFUNCTION(CKClassDescGetToNotify), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void SetToNotify(int index, CK_CLASSID cid)", asFUNCTION(CKClassDescSetToNotify), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "NativePointer get_RegisterFct() const", asFUNCTIONPR([](const CKClassDesc *self) { return NativePointer(self->RegisterFct); }, (const CKClassDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void set_RegisterFct(NativePointer ptr)", asFUNCTIONPR([](CKClassDesc *self, NativePointer ptr) { self->RegisterFct = reinterpret_cast<CKCLASSREGISTERFCT>(ptr.Get()); }, (CKClassDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "NativePointer get_CreationFct() const", asFUNCTIONPR([](const CKClassDesc *self) { return NativePointer(self->CreationFct); }, (const CKClassDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void set_CreationFct(NativePointer ptr)", asFUNCTIONPR([](CKClassDesc *self, NativePointer ptr) { self->CreationFct = reinterpret_cast<CKCLASSCREATIONFCT>(ptr.Get()); }, (CKClassDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "NativePointer get_NameFct() const", asFUNCTIONPR([](const CKClassDesc *self) { return NativePointer(self->NameFct); }, (const CKClassDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void set_NameFct(NativePointer ptr)", asFUNCTIONPR([](CKClassDesc *self, NativePointer ptr) { self->NameFct = reinterpret_cast<CKCLASSNAMEFCT>(ptr.Get()); }, (CKClassDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "NativePointer get_DependsFct() const", asFUNCTIONPR([](const CKClassDesc *self) { return NativePointer(self->DependsFct); }, (const CKClassDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void set_DependsFct(NativePointer ptr)", asFUNCTIONPR([](CKClassDesc *self, NativePointer ptr) { self->DependsFct = reinterpret_cast<CKCLASSDEPENDENCIESFCT>(ptr.Get()); }, (CKClassDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKClassDesc", "NativePointer get_DependsCountFct() const", asFUNCTIONPR([](const CKClassDesc *self) { return NativePointer(self->DependsCountFct); }, (const CKClassDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKClassDesc", "void set_DependsCountFct(NativePointer ptr)", asFUNCTIONPR([](CKClassDesc *self, NativePointer ptr) { self->DependsCountFct = reinterpret_cast<CKCLASSDEPENDENCIESCOUNTFCT>(ptr.Get()); }, (CKClassDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKPluginInfo

void RegisterCKPluginInfo(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginInfo", "CKGUID m_GUID", asOFFSET(CKPluginInfo, m_GUID)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "CKFileExtension m_Extension", asOFFSET(CKPluginInfo, m_Extension)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "XString m_Description", asOFFSET(CKPluginInfo, m_Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "XString m_Author", asOFFSET(CKPluginInfo, m_Author)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "XString m_Summary", asOFFSET(CKPluginInfo, m_Summary)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "CKDWORD m_Version", asOFFSET(CKPluginInfo, m_Version)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "uintptr_t m_InitInstanceFct", asOFFSET(CKPluginInfo, m_InitInstanceFct)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "CK_PLUGIN_TYPE m_Type", asOFFSET(CKPluginInfo, m_Type)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginInfo", "uintptr_t m_ExitInstanceFct", asOFFSET(CKPluginInfo, m_ExitInstanceFct)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginInfo", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginInfo *self) { new(self) CKPluginInfo(); }, (CKPluginInfo*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginInfo", asBEHAVE_CONSTRUCT, "void f(const CKPluginInfo &in other)", asFUNCTIONPR([](const CKPluginInfo &info, CKPluginInfo *self) { new(self) CKPluginInfo(info); }, (const CKPluginInfo &, CKPluginInfo *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginInfo", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginInfo *self) { self->~CKPluginInfo(); }, (CKPluginInfo *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginInfo", "CKPluginInfo &opAssign(const CKPluginInfo &in other)", asMETHODPR(CKPluginInfo, operator=, (const CKPluginInfo &), CKPluginInfo &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginInfo", "NativePointer get_m_InitInstanceFct() const", asFUNCTIONPR([](const CKPluginInfo *self) { return NativePointer(self->m_InitInstanceFct); }, (const CKPluginInfo *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginInfo", "void set_m_InitInstanceFct(NativePointer ptr)", asFUNCTIONPR([](CKPluginInfo *self, NativePointer ptr) { self->m_InitInstanceFct = reinterpret_cast<CK_INITINSTANCEFCT>(ptr.Get()); }, (CKPluginInfo *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginInfo", "NativePointer get_m_ExitInstanceFct() const", asFUNCTIONPR([](const CKPluginInfo *self) { return NativePointer(self->m_ExitInstanceFct); }, (const CKPluginInfo *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginInfo", "void set_m_ExitInstanceFct(NativePointer ptr)", asFUNCTIONPR([](CKPluginInfo *self, NativePointer ptr) { self->m_ExitInstanceFct = reinterpret_cast<CK_EXITINSTANCEFCT>(ptr.Get()); }, (CKPluginInfo *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKEnumStruct

void RegisterCKEnumStruct(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CKEnumStruct", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKEnumStruct *self) { new(self) CKEnumStruct(); }, (CKEnumStruct*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKEnumStruct", asBEHAVE_CONSTRUCT, "void f(const CKEnumStruct &in other)", asFUNCTIONPR([](const CKEnumStruct &info, CKEnumStruct *self) { new(self) CKEnumStruct(info); }, (const CKEnumStruct &, CKEnumStruct *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKEnumStruct", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKEnumStruct *self) { self->~CKEnumStruct(); }, (CKEnumStruct *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKEnumStruct", "CKEnumStruct &opAssign(const CKEnumStruct &in other)", asMETHODPR(CKEnumStruct, operator=, (const CKEnumStruct &), CKEnumStruct &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKEnumStruct", "int GetNumEnums() const", asMETHOD(CKEnumStruct, GetNumEnums), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKEnumStruct", "int GetEnumValue(int index) const", asMETHOD(CKEnumStruct, GetEnumValue), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKEnumStruct", "string GetEnumDescription(int index) const", asFUNCTIONPR([](CKEnumStruct *self, int index) -> std::string { return ScriptStringify(self->GetEnumDescription(index)); }, (CKEnumStruct *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKFlagsStruct
void RegisterCKFlagsStruct(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CKFlagsStruct", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFlagsStruct *self) { new(self) CKFlagsStruct(); }, (CKFlagsStruct*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFlagsStruct", asBEHAVE_CONSTRUCT, "void f(const CKFlagsStruct &in other)", asFUNCTIONPR([](const CKFlagsStruct &info, CKFlagsStruct *self) { new(self) CKFlagsStruct(info); }, (const CKFlagsStruct &, CKFlagsStruct *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFlagsStruct", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFlagsStruct *self) { self->~CKFlagsStruct(); }, (CKFlagsStruct *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFlagsStruct", "CKFlagsStruct &opAssign(const CKFlagsStruct &in other)", asMETHODPR(CKFlagsStruct, operator=, (const CKFlagsStruct &), CKFlagsStruct &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFlagsStruct", "int GetNumFlags() const", asMETHOD(CKFlagsStruct, GetNumFlags), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFlagsStruct", "int GetFlagValue(int index) const", asMETHOD(CKFlagsStruct, GetFlagValue), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFlagsStruct", "string GetFlagDescription(int index) const", asFUNCTIONPR([](CKFlagsStruct *self, int index) -> std::string { return self->GetFlagDescription(index); }, (CKFlagsStruct *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKStructStruct

void RegisterCKStructStruct(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CKStructStruct", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKStructStruct *self) { new(self) CKStructStruct(); }, (CKStructStruct*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKStructStruct", asBEHAVE_CONSTRUCT, "void f(const CKStructStruct &in other)", asFUNCTIONPR([](const CKStructStruct &info, CKStructStruct *self) { new(self) CKStructStruct(info); }, (const CKStructStruct &, CKStructStruct *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKStructStruct", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKStructStruct *self) { self->~CKStructStruct(); }, (CKStructStruct *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStructStruct", "CKStructStruct &opAssign(const CKStructStruct &in other)", asMETHODPR(CKStructStruct, operator=, (const CKStructStruct &), CKStructStruct &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStructStruct", "int GetNumSubParam() const", asMETHOD(CKStructStruct, GetNumSubParam), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStructStruct", "CKGUID &GetSubParamGuid(int index) const", asMETHOD(CKStructStruct, GetSubParamGuid), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStructStruct", "string GetSubParamDescription(int index) const", asFUNCTIONPR([](CKStructStruct *self, int index) -> std::string { return ScriptStringify(self->GetSubParamDescription(index)); }, (CKStructStruct *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKParameterTypeDesc

void RegisterCKParameterTypeDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKParameterType Index", asOFFSET(CKParameterTypeDesc, Index)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKGUID Guid", asOFFSET(CKParameterTypeDesc, Guid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKGUID DerivedFrom", asOFFSET(CKParameterTypeDesc, DerivedFrom)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "XString TypeName", asOFFSET(CKParameterTypeDesc, TypeName)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "int Valid", asOFFSET(CKParameterTypeDesc, Valid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "int DefaultSize", asOFFSET(CKParameterTypeDesc, DefaultSize)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t CreateDefaultFunction", asOFFSET(CKParameterTypeDesc, CreateDefaultFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t DeleteFunction", asOFFSET(CKParameterTypeDesc, DeleteFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t SaveLoadFunction", asOFFSET(CKParameterTypeDesc, SaveLoadFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t CheckFunction", asOFFSET(CKParameterTypeDesc, CheckFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t CopyFunction", asOFFSET(CKParameterTypeDesc, CopyFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t StringFunction", asOFFSET(CKParameterTypeDesc, StringFunction)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterTypeDesc", "uintptr_t UICreatorFunction", asOFFSET(CKParameterTypeDesc, UICreatorFunction)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKPluginEntry &CreatorDll", asOFFSET(CKParameterTypeDesc, CreatorDll)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKDWORD dwParam", asOFFSET(CKParameterTypeDesc, dwParam)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKDWORD dwFlags", asOFFSET(CKParameterTypeDesc, dwFlags)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKDWORD Cid", asOFFSET(CKParameterTypeDesc, Cid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "XBitArray DerivationMask", asOFFSET(CKParameterTypeDesc, DerivationMask)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKParameterTypeDesc", "CKGUID Saver_Manager", asOFFSET(CKParameterTypeDesc, Saver_Manager)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKParameterTypeDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKParameterTypeDesc *self) { new(self) CKParameterTypeDesc(); }, (CKParameterTypeDesc*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKParameterTypeDesc", asBEHAVE_CONSTRUCT, "void f(const CKParameterTypeDesc &in other)", asFUNCTIONPR([](const CKParameterTypeDesc &desc, CKParameterTypeDesc *self) { new(self) CKParameterTypeDesc(desc); }, (const CKParameterTypeDesc &, CKParameterTypeDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKParameterTypeDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKParameterTypeDesc *self) { self->~CKParameterTypeDesc(); }, (CKParameterTypeDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "CKParameterTypeDesc &opAssign(const CKParameterTypeDesc &in other)", asMETHODPR(CKParameterTypeDesc, operator=, (const CKParameterTypeDesc &), CKParameterTypeDesc &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_CreateDefaultFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->CreateDefaultFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_CreateDefaultFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->CreateDefaultFunction = reinterpret_cast<CK_PARAMETERCREATEDEFAULTFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_DeleteFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->DeleteFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_DeleteFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->DeleteFunction = reinterpret_cast<CK_PARAMETERDELETEFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_SaveLoadFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->SaveLoadFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_SaveLoadFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->SaveLoadFunction = reinterpret_cast<CK_PARAMETERSAVELOADFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_CheckFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->CheckFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_CheckFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->CheckFunction = reinterpret_cast<CK_PARAMETERCHECKFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_CopyFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->CopyFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_CopyFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->CopyFunction = reinterpret_cast<CK_PARAMETERCOPYFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_StringFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->StringFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_StringFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->StringFunction = reinterpret_cast<CK_PARAMETERSTRINGFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "NativePointer get_UICreatorFunction() const", asFUNCTIONPR([](const CKParameterTypeDesc *self) { return NativePointer(self->UICreatorFunction); }, (const CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterTypeDesc", "void set_UICreatorFunction(NativePointer ptr)", asFUNCTIONPR([](CKParameterTypeDesc *self, NativePointer ptr) { self->UICreatorFunction = reinterpret_cast<CK_PARAMETERUICREATORFUNCTION>(ptr.Get()); }, (CKParameterTypeDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKBitmapProperties

void RegisterCKBitmapProperties(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKBitmapProperties", "int m_Size", asOFFSET(CKBitmapProperties, m_Size)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapProperties", "CKGUID m_ReaderGuid", asOFFSET(CKBitmapProperties, m_ReaderGuid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapProperties", "CKFileExtension m_Ext", asOFFSET(CKBitmapProperties, m_Ext)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapProperties", "VxImageDescEx m_Format", asOFFSET(CKBitmapProperties, m_Format)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKBitmapProperties", "uintptr_t m_Data", asOFFSET(CKBitmapProperties, m_Data)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapProperties", "NativePointer get_m_Data() const", asFUNCTIONPR([](const CKBitmapProperties *self) { return NativePointer(self->m_Data); }, (const CKBitmapProperties *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapProperties", "void set_m_Data(NativePointer ptr)", asFUNCTIONPR([](CKBitmapProperties *self, NativePointer ptr) { self->m_Data = ptr.Get(); }, (CKBitmapProperties *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKMovieProperties

void RegisterCKMovieProperties(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKMovieProperties", "int m_Size", asOFFSET(CKMovieProperties, m_Size)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMovieProperties", "CKGUID m_ReaderGuid", asOFFSET(CKMovieProperties, m_ReaderGuid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMovieProperties", "CKFileExtension m_Ext", asOFFSET(CKMovieProperties, m_Ext)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMovieProperties", "VxImageDescEx m_Format", asOFFSET(CKMovieProperties, m_Format)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKMovieProperties", "NativePointer m_Data", asOFFSET(CKMovieProperties, m_Data)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMovieProperties", "NativePointer get_m_Data() const", asFUNCTIONPR([](const CKMovieProperties *self) { return NativePointer(self->m_Data); }, (const CKMovieProperties *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMovieProperties", "void set_m_Data(NativePointer ptr)", asFUNCTIONPR([](CKMovieProperties *self, NativePointer ptr) { self->m_Data = ptr.Get(); }, (CKMovieProperties *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKDataReader

template<typename T>
static void RegisterCKDataReaderMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectMethod(name, "void Release()", asMETHODPR(T, Release, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKPluginInfo &GetReaderInfo()", asMETHODPR(T, GetReaderInfo, (), CKPluginInfo*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetOptionsCount()", asMETHODPR(T, GetOptionsCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "string GetOptionDescription(int i)", asFUNCTIONPR([](T *self, int i) -> std::string { return ScriptStringify(self->GetOptionDescription(i)); }, (T *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_DATAREADER_FLAGS GetFlags()", asMETHODPR(T, GetFlags, (), CK_DATAREADER_FLAGS), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKDataReader") != 0) {
        RegisterClassRefCast<T, CKDataReader>(engine, name, "CKDataReader");
    }
}

void RegisterCKDataReader(asIScriptEngine *engine) {
    RegisterCKDataReaderMembers<CKDataReader>(engine, "CKDataReader");
}

// CKModelReader

void RegisterCKModelReader(asIScriptEngine *engine) {
    int r = 0;

    RegisterCKDataReaderMembers<CKModelReader>(engine, "CKModelReader");

    r = engine->RegisterObjectMethod("CKModelReader", "CKERROR Load(CKContext@ context, const string &in filename, CKObjectArray@ objArray, CKDWORD loadFlags, CKCharacter@ carac = null)", asFUNCTIONPR([](CKModelReader *self, CKContext *context, const std::string &filename, CKObjectArray *objArray, CKDWORD loadFlags, CKCharacter *carac) { return self->Load(context, const_cast<CKSTRING>(filename.c_str()), objArray, loadFlags, carac); }, (CKModelReader*, CKContext *, const std::string&, CKObjectArray *, CKDWORD, CKCharacter*), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKModelReader", "CKERROR Save(CKContext@ context, const string &in filename, CKObjectArray@ objArray, CKDWORD saveFlags)", asFUNCTIONPR([](CKModelReader *self, CKContext *context, const std::string &filename, CKObjectArray *objArray, CKDWORD saveFlags) { return self->Save(context, const_cast<CKSTRING>(filename.c_str()), objArray, saveFlags); }, (CKModelReader*, CKContext *, const std::string&, CKObjectArray *, CKDWORD), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKBitmapReader

void RegisterCKBitmapReader(asIScriptEngine *engine) {
    int r = 0;

    RegisterCKDataReaderMembers<CKBitmapReader>(engine, "CKBitmapReader");

    r = engine->RegisterObjectMethod("CKBitmapReader", "bool IsAlphaSaved(CKBitmapProperties@ bp)", asFUNCTIONPR([](CKBitmapReader *self, CKBitmapProperties *bp) -> bool { return self->IsAlphaSaved(bp); }, (CKBitmapReader *, CKBitmapProperties *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "void GetBitmapDefaultProperties(CKBitmapProperties@ &out bp)", asMETHODPR(CKBitmapReader, GetBitmapDefaultProperties, (CKBitmapProperties**), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "void SetBitmapDefaultProperties(CKBitmapProperties@ bp)", asMETHODPR(CKBitmapReader, SetBitmapDefaultProperties, (CKBitmapProperties*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "int ReadFile(const string &in name, CKBitmapProperties@ &out bp)", asFUNCTIONPR([](CKBitmapReader *self, const std::string &name, CKBitmapProperties **bp) -> int { return self->ReadFile(const_cast<char*>(name.c_str()), bp); }, (CKBitmapReader *, const std::string &, CKBitmapProperties **), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "int ReadMemory(NativePointer memory, int size, CKBitmapProperties@ &out bp)", asFUNCTIONPR([](CKBitmapReader* self, NativePointer memory, int size, CKBitmapProperties **bp) { return self->ReadMemory(memory.Get(), size, bp); }, (CKBitmapReader *, NativePointer, int, CKBitmapProperties **), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "int ReadASynchronousFile(const string &in name, CKBitmapProperties@ &out bp)", asFUNCTIONPR([](CKBitmapReader *self, const std::string &name, CKBitmapProperties **bp) -> int { return self->ReadASynchronousFile(const_cast<char*>(name.c_str()), bp); }, (CKBitmapReader *, const std::string &, CKBitmapProperties **), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "int SaveFile(const string &in name, CKBitmapProperties@ bp)", asFUNCTIONPR([](CKBitmapReader *self, const std::string &name, CKBitmapProperties *bp) -> int { return self->SaveFile(const_cast<char*>(name.c_str()), bp); }, (CKBitmapReader *, const std::string &, CKBitmapProperties *), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "int SaveMemory(NativePointer &out memory, CKBitmapProperties@ bp)", asFUNCTIONPR([](CKBitmapReader *self, NativePointer *memory, CKBitmapProperties *bp) { return self->SaveMemory(reinterpret_cast<void **>(memory), bp); }, (CKBitmapReader *, NativePointer *, CKBitmapProperties *), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapReader", "void ReleaseMemory(NativePointer memory)", asFUNCTIONPR([](CKBitmapReader *self, NativePointer memory) { self->ReleaseMemory(memory.Get()); }, (CKBitmapReader *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKSoundReader

void RegisterCKSoundReader(asIScriptEngine *engine) {
    int r = 0;

    RegisterCKDataReaderMembers<CKSoundReader>(engine, "CKSoundReader");

    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR OpenFile(const string &in filename)", asFUNCTIONPR([](CKSoundReader *self, const std::string &filename) -> CKERROR { return self->OpenFile(const_cast<char *>(filename.c_str())); }, (CKSoundReader *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Decode()", asMETHODPR(CKSoundReader, Decode, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR GetDataBuffer(NativePointer &out buf, int &out size)", asFUNCTIONPR([](CKSoundReader *self, NativePointer *buf, int *size) { return self->GetDataBuffer(reinterpret_cast<CKBYTE**>(buf), size); }, (CKSoundReader *, NativePointer *, int *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR GetWaveFormat(CKWaveFormat &out wfe)", asMETHODPR(CKSoundReader, GetWaveFormat, (CKWaveFormat *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "int GetDataSize()", asMETHODPR(CKSoundReader, GetDataSize, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "int GetDuration()", asMETHODPR(CKSoundReader, GetDuration, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Play()", asMETHODPR(CKSoundReader, Play, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Stop()", asMETHODPR(CKSoundReader, Stop, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Pause()", asMETHODPR(CKSoundReader, Pause, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Resume()", asMETHODPR(CKSoundReader, Resume, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR Seek(int pos)", asMETHODPR(CKSoundReader, Seek, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundReader", "CKERROR ReadMemory(NativePointer memory, int size)", asFUNCTIONPR([](CKSoundReader *self, NativePointer memory, int size) { return self->ReadMemory(memory.Get(), size); }, (CKSoundReader *, NativePointer, int), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKMovieReader

void RegisterCKMovieReader(asIScriptEngine *engine) {
    int r = 0;

    RegisterCKDataReaderMembers<CKMovieReader>(engine, "CKMovieReader");

    r = engine->RegisterObjectMethod("CKMovieReader", "int GetMovieFrameCount() const", asMETHODPR(CKMovieReader, GetMovieFrameCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMovieReader", "int GetMovieLength() const", asMETHODPR(CKMovieReader, GetMovieLength, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMovieReader", "CKERROR OpenFile(const string &in name)", asFUNCTIONPR([](CKMovieReader *self, const std::string &name) { return self->OpenFile(const_cast<char*>(name.c_str())); }, (CKMovieReader *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMovieReader", "CKERROR OpenMemory(const string &in name)", asFUNCTIONPR([](CKMovieReader *self, const std::string &name) { return self->OpenMemory(const_cast<char*>(name.c_str())); }, (CKMovieReader *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMovieReader", "CKERROR OpenAsynchronousFile(const string &in name)", asFUNCTIONPR([](CKMovieReader *self, const std::string &name) { return self->OpenAsynchronousFile(const_cast<char*>(name.c_str())); }, (CKMovieReader *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMovieReader", "CKERROR ReadFrame(int f, CKMovieProperties@ &out prop)", asMETHODPR(CKMovieReader, ReadFrame, (int, CKMovieProperties **), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

// CKPluginDll

void RegisterCKPluginDll(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginDll", "XString m_DllFileName", asOFFSET(CKPluginDll, m_DllFileName)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginDll", "INSTANCE_HANDLE m_DllInstance", asOFFSET(CKPluginDll, m_DllInstance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginDll", "int m_PluginInfoCount", asOFFSET(CKPluginDll, m_PluginInfoCount)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginDll", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginDll *self) { new(self) CKPluginDll(); }, (CKPluginDll*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginDll", asBEHAVE_CONSTRUCT, "void f(const CKPluginDll &in other)", asFUNCTIONPR([](const CKPluginDll &dll, CKPluginDll *self) { new(self) CKPluginDll(dll); }, (const CKPluginDll &, CKPluginDll *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginDll", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginDll *self) { self->~CKPluginDll(); }, (CKPluginDll *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginDll", "CKPluginDll &opAssign(const CKPluginDll &in other)", asMETHODPR(CKPluginDll, operator=, (const CKPluginDll &), CKPluginDll &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginDll", "NativePointer GetFunctionPtr(const string &in functionName) const", asFUNCTIONPR([](CKPluginDll *self, const std::string &functionName) { return NativePointer(self->GetFunctionPtr(const_cast<char*>(functionName.c_str()))); }, (CKPluginDll *, const std::string &), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKPluginEntryReadersData

void RegisterCKPluginEntryReadersData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginEntryReadersData", "CKGUID m_SettingsParameterGuid", asOFFSET(CKPluginEntryReadersData, m_SettingsParameterGuid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntryReadersData", "int m_OptionCount", asOFFSET(CKPluginEntryReadersData, m_OptionCount)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntryReadersData", "CK_DATAREADER_FLAGS m_ReaderFlags", asOFFSET(CKPluginEntryReadersData, m_ReaderFlags)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKPluginEntryReadersData", "uintptr_t m_GetReaderFct", asOFFSET(CKPluginEntryReadersData, m_GetReaderFct)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntryReadersData", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntryReadersData *self) { new(self) CKPluginEntryReadersData(); }, (CKPluginEntryReadersData*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginEntryReadersData", asBEHAVE_CONSTRUCT, "void f(const CKPluginEntryReadersData &in other)", asFUNCTIONPR([](const CKPluginEntryReadersData &data, CKPluginEntryReadersData *self) { new(self) CKPluginEntryReadersData(data); }, (const CKPluginEntryReadersData &, CKPluginEntryReadersData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntryReadersData", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntryReadersData *self) { self->~CKPluginEntryReadersData(); }, (CKPluginEntryReadersData *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginEntryReadersData", "CKPluginEntryReadersData &opAssign(const CKPluginEntryReadersData &in other)", asMETHODPR(CKPluginEntryReadersData, operator=, (const CKPluginEntryReadersData &), CKPluginEntryReadersData &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginEntryReadersData", "NativePointer get_m_GetReaderFct() const", asFUNCTIONPR([](const CKPluginEntryReadersData *self) { return NativePointer(self->m_GetReaderFct); }, (const CKPluginEntryReadersData *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginEntryReadersData", "void set_m_GetReaderFct(NativePointer ptr)", asFUNCTIONPR([](CKPluginEntryReadersData *self, NativePointer ptr) { self->m_GetReaderFct = reinterpret_cast<CKReaderGetReaderFunction>(ptr.Get()); }, (CKPluginEntryReadersData *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

}

// CKPluginEntryBehaviorsData

void RegisterCKPluginEntryBehaviorsData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginEntryBehaviorsData", "XGUIDArray m_BehaviorsGUID", asOFFSET(CKPluginEntryBehaviorsData, m_BehaviorsGUID)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntryBehaviorsData", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntryBehaviorsData *self) { new(self) CKPluginEntryBehaviorsData(); }, (CKPluginEntryBehaviorsData*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginEntryBehaviorsData", asBEHAVE_CONSTRUCT, "void f(const CKPluginEntryBehaviorsData &in other)", asFUNCTIONPR([](const CKPluginEntryBehaviorsData &data, CKPluginEntryBehaviorsData *self) { new(self) CKPluginEntryBehaviorsData(data); }, (const CKPluginEntryBehaviorsData &, CKPluginEntryBehaviorsData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntryBehaviorsData", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntryBehaviorsData *self) { self->~CKPluginEntryBehaviorsData(); }, (CKPluginEntryBehaviorsData *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginEntryBehaviorsData", "CKPluginEntryBehaviorsData &opAssign(const CKPluginEntryBehaviorsData &in other)", asMETHODPR(CKPluginEntryBehaviorsData, operator=, (const CKPluginEntryBehaviorsData &), CKPluginEntryBehaviorsData &), asCALL_THISCALL); assert(r >= 0);
}

// CKPluginEntry

void RegisterCKPluginEntry(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginEntry", "int m_PluginDllIndex", asOFFSET(CKPluginEntry, m_PluginDllIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "int m_PositionInDll", asOFFSET(CKPluginEntry, m_PositionInDll)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "CKPluginInfo m_PluginInfo", asOFFSET(CKPluginEntry, m_PluginInfo)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "CKPluginEntryReadersData &m_ReadersInfo", asOFFSET(CKPluginEntry, m_ReadersInfo)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "CKPluginEntryBehaviorsData &m_BehaviorsInfo", asOFFSET(CKPluginEntry, m_BehaviorsInfo)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "int m_Active", asOFFSET(CKPluginEntry, m_Active)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "int m_IndexInCategory", asOFFSET(CKPluginEntry, m_IndexInCategory)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPluginEntry", "int m_NeededByFile", asOFFSET(CKPluginEntry, m_NeededByFile)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntry", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntry *self) { new(self) CKPluginEntry(); }, (CKPluginEntry*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginEntry", asBEHAVE_CONSTRUCT, "void f(const CKPluginEntry &in other)", asFUNCTIONPR([](const CKPluginEntry &entry, CKPluginEntry *self) { new(self) CKPluginEntry(entry); }, (const CKPluginEntry &, CKPluginEntry *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginEntry", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginEntry *self) { self->~CKPluginEntry(); }, (CKPluginEntry *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginEntry", "CKPluginEntry &opAssign(const CKPluginEntry &in other)", asMETHODPR(CKPluginEntry, operator=, (const CKPluginEntry &), CKPluginEntry &), asCALL_THISCALL); assert(r >= 0);
}

// CKPluginCategory

void RegisterCKPluginCategory(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPluginCategory", "XString m_Name", asOFFSET(CKPluginCategory, m_Name)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKPluginCategory", "XArray<CKPluginEntry *> m_Entries", asOFFSET(CKPluginCategory, m_Entries)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginCategory", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPluginCategory *self) { new(self) CKPluginCategory(); }, (CKPluginCategory*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPluginCategory", asBEHAVE_CONSTRUCT, "void f(const CKPluginCategory &in other)", asFUNCTIONPR([](const CKPluginCategory &cate, CKPluginCategory *self) { new(self) CKPluginCategory(cate); }, (const CKPluginCategory &, CKPluginCategory *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPluginCategory", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPluginCategory *self) { self->~CKPluginCategory(); }, (CKPluginCategory *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginCategory", "CKPluginCategory &opAssign(const CKPluginCategory &in other)", asMETHODPR(CKPluginCategory, operator=, (const CKPluginCategory &), CKPluginCategory &), asCALL_THISCALL); assert(r >= 0);
}

// CKOperationDesc

void RegisterCKOperationDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKOperationDesc", "CKGUID OpGuid", asOFFSET(CKOperationDesc, OpGuid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKOperationDesc", "CKGUID P1Guid", asOFFSET(CKOperationDesc, P1Guid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKOperationDesc", "CKGUID P2Guid", asOFFSET(CKOperationDesc, P2Guid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKOperationDesc", "CKGUID ResGuid", asOFFSET(CKOperationDesc, ResGuid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKOperationDesc", "uintptr_t Fct", asOFFSET(CKOperationDesc, Fct)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKOperationDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKOperationDesc *self) { new(self) CKOperationDesc(); }, (CKOperationDesc*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKOperationDesc", asBEHAVE_CONSTRUCT, "void f(const CKOperationDesc &in other)", asFUNCTIONPR([](const CKOperationDesc &desc, CKOperationDesc *self) { new(self) CKOperationDesc(desc); }, (const CKOperationDesc &, CKOperationDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKOperationDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKOperationDesc *self) { self->~CKOperationDesc(); }, (CKOperationDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKOperationDesc", "CKOperationDesc &opAssign(const CKOperationDesc &in other)", asMETHODPR(CKOperationDesc, operator=, (const CKOperationDesc &), CKOperationDesc &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKOperationDesc", "NativePointer get_Fct() const", asFUNCTIONPR([](const CKOperationDesc *self) { return NativePointer(self->Fct); }, (const CKOperationDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKOperationDesc", "void set_Fct(NativePointer ptr)", asFUNCTIONPR([](CKOperationDesc *self, NativePointer ptr) { self->Fct = reinterpret_cast<CK_PARAMETEROPERATION>(ptr.Get()); }, (CKOperationDesc *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKAttributeVal

void RegisterCKAttributeVal(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKAttributeVal", "CKAttributeType AttribType", asOFFSET(CKAttributeVal, AttribType)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKAttributeVal", "CK_ID Parameter", asOFFSET(CKAttributeVal, Parameter)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKAttributeVal", asBEHAVE_CONSTRUCT, "void f(const CKAttributeVal &in other)", asFUNCTIONPR([](const CKAttributeVal &val, CKAttributeVal *self) { new(self) CKAttributeVal(val); }, (const CKAttributeVal &, CKAttributeVal *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeVal", "CKAttributeVal &opAssign(const CKAttributeVal &in other)", asMETHODPR(CKAttributeVal, operator=, (const CKAttributeVal &), CKAttributeVal &), asCALL_THISCALL); assert(r >= 0);
}

// CKTimeProfiler

void RegisterCKTimeProfiler(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CKTimeProfiler", asBEHAVE_CONSTRUCT, "void f(const string &in title, CKContext@ context, int startingCount = 4)", asFUNCTIONPR([](const std::string &iTitle, CKContext *iContext, int iStartingCount, CKTimeProfiler *self) { new(self) CKTimeProfiler(iTitle.c_str(), iContext, iStartingCount); }, (const std::string &, CKContext *, int, CKTimeProfiler *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKTimeProfiler", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKTimeProfiler *self) { self->~CKTimeProfiler(); }, (CKTimeProfiler *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeProfiler", "void opCall(const string &in str)", asFUNCTIONPR([](CKTimeProfiler *self, const std::string &str) { (*self)(str.c_str()); }, (CKTimeProfiler *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeProfiler", "void Reset()", asMETHODPR(CKTimeProfiler, Reset, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeProfiler", "void Dump(string &out buffer, const string &in separator = \" | \")", asFUNCTIONPR([](CKTimeProfiler *self, std::string &buffer, const std::string &separator) { self->Dump(const_cast<char*>(buffer.c_str()), const_cast<char*>(separator.c_str())); }, (CKTimeProfiler *, std::string &, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKMessage

void RegisterCKMessage(asIScriptEngine *engine) {
    int r = 0;

    // Not existing in Virtools 2.1
    // r = engine->RegisterObjectBehaviour("CKMessage", asBEHAVE_ADDREF, "void f()", asMETHOD(CKMessage, AddRef), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectBehaviour("CKMessage", asBEHAVE_RELEASE, "void f()", asMETHOD(CKMessage, Release), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "CKERROR SetBroadcastObjectType(CK_CLASSID type = CKCID_BEOBJECT)", asMETHOD(CKMessage, SetBroadcastObjectType), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "CKERROR AddParameter(CKParameter@ param, bool deleteParameterWithMessage = false)", asFUNCTIONPR([](CKMessage *self, CKParameter *param, bool deleteParam) { return self->AddParameter(param, deleteParam); }, (CKMessage *, CKParameter *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "CKERROR RemoveParameter(CKParameter@ param)", asMETHOD(CKMessage, RemoveParameter), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "int GetParameterCount()", asMETHOD(CKMessage, GetParameterCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "CKParameter@ GetParameter(int pos)", asMETHOD(CKMessage, GetParameter), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "void SetSender(CKBeObject@ obj)", asMETHOD(CKMessage, SetSender), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "CKBeObject@ GetSender()", asMETHOD(CKMessage, GetSender), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "void SetRecipient(CKObject@ recipient)", asMETHOD(CKMessage, SetRecipient), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "CKObject@ GetRecipient()", asMETHOD(CKMessage, GetRecipient), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "CK_MESSAGE_SENDINGTYPE GetSendingType()", asMETHOD(CKMessage, GetSendingType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "void SetSendingType(CK_MESSAGE_SENDINGTYPE type)", asMETHOD(CKMessage, SetSendingType), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessage", "void SetMsgType(CKMessageType type)", asMETHOD(CKMessage, SetMsgType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessage", "CKMessageType GetMsgType()", asMETHOD(CKMessage, GetMsgType), asCALL_THISCALL); assert(r >= 0);
}

// CKWaitingObject

void RegisterCKWaitingObject(asIScriptEngine *engine) {
#if CKVERSION == 0x13022002
    int r = 0;

    r = engine->RegisterObjectProperty("CKWaitingObject", "CKBeObject@ m_BeObject", asOFFSET(CKWaitingObject, m_BeObject)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaitingObject", "CKBehavior@ m_Behavior", asOFFSET(CKWaitingObject, m_Behavior)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaitingObject", "CKBehaviorIO@ m_Output", asOFFSET(CKWaitingObject, m_Output)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaitingObject", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKWaitingObject *self) { new(self) CKWaitingObject(); }, (CKWaitingObject*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKWaitingObject", asBEHAVE_CONSTRUCT, "void f(const CKWaitingObject &in other)", asFUNCTIONPR([](const CKWaitingObject &obj, CKWaitingObject *self) { new(self) CKWaitingObject(obj); }, (const CKWaitingObject &, CKWaitingObject *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaitingObject", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKWaitingObject *self) { self->~CKWaitingObject(); }, (CKWaitingObject *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaitingObject", "CKWaitingObject &opAssign(const CKWaitingObject &in other)", asMETHODPR(CKWaitingObject, operator=, (const CKWaitingObject &), CKWaitingObject &), asCALL_THISCALL); assert(r >= 0);
#endif
}

// CKPATHCATEGORY

void RegisterCKPATHCATEGORY(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPATHCATEGORY", "XString m_Name", asOFFSET(CKPATHCATEGORY, m_Name)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPATHCATEGORY", "CKPATHENTRYVECTOR m_Entries", asOFFSET(CKPATHCATEGORY, m_Entries)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPATHCATEGORY", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPATHCATEGORY *self) { new(self) CKPATHCATEGORY(); }, (CKPATHCATEGORY*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPATHCATEGORY", asBEHAVE_CONSTRUCT, "void f(const CKPATHCATEGORY &in other)", asFUNCTIONPR([](const CKPATHCATEGORY &cate, CKPATHCATEGORY *self) { new(self) CKPATHCATEGORY(cate); }, (const CKPATHCATEGORY &, CKPATHCATEGORY *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPATHCATEGORY", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPATHCATEGORY *self) { self->~CKPATHCATEGORY(); }, (CKPATHCATEGORY *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPATHCATEGORY", "CKPATHCATEGORY &opAssign(const CKPATHCATEGORY &in other)", asMETHODPR(CKPATHCATEGORY, operator=, (const CKPATHCATEGORY &), CKPATHCATEGORY &), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKPARAMETER_DESC(asIScriptEngine *engine) {
    int r = 0;

    // r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "uintptr_t Name", asOFFSET(CKPARAMETER_DESC, Name)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "CKGUID Guid", asOFFSET(CKPARAMETER_DESC, Guid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "int Type", asOFFSET(CKPARAMETER_DESC, Type)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "uintptr_t DefaultValueString", asOFFSET(CKPARAMETER_DESC, DefaultValueString)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "uintptr_t DefaultValue", asOFFSET(CKPARAMETER_DESC, DefaultValue)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "int DefaultValueSize", asOFFSET(CKPARAMETER_DESC, DefaultValueSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPARAMETER_DESC", "int Owner", asOFFSET(CKPARAMETER_DESC, Owner)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPARAMETER_DESC", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPARAMETER_DESC *self) { new(self) CKPARAMETER_DESC(); }, (CKPARAMETER_DESC*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPARAMETER_DESC", asBEHAVE_CONSTRUCT, "void f(const CKPARAMETER_DESC &in other)", asFUNCTIONPR([](const CKPARAMETER_DESC &desc, CKPARAMETER_DESC *self) { new(self) CKPARAMETER_DESC(desc); }, (const CKPARAMETER_DESC &, CKPARAMETER_DESC *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPARAMETER_DESC", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPARAMETER_DESC *self) { self->~CKPARAMETER_DESC(); }, (CKPARAMETER_DESC *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "CKPARAMETER_DESC &opAssign(const CKPARAMETER_DESC &in other)", asMETHODPR(CKPARAMETER_DESC, operator=, (const CKPARAMETER_DESC &), CKPARAMETER_DESC &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "string get_Name() const", asFUNCTIONPR([](const CKPARAMETER_DESC *self) -> std::string { return ScriptStringify(self->Name); }, (const CKPARAMETER_DESC *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "void set_Name(const string &in value)", asFUNCTIONPR([](CKPARAMETER_DESC *self, const std::string &value) { CKDeletePointer(self->Name); self->Name = CKStrdup(const_cast<CKSTRING>(value.c_str())); }, (CKPARAMETER_DESC *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "string get_DefaultValueString() const", asFUNCTIONPR([](const CKPARAMETER_DESC *self) -> std::string { return ScriptStringify(self->DefaultValueString); }, (const CKPARAMETER_DESC *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "void set_DefaultValueString(const string &in value)", asFUNCTIONPR([](CKPARAMETER_DESC *self, const std::string &value) { CKDeletePointer(self->DefaultValueString); self->DefaultValueString = CKStrdup(const_cast<CKSTRING>(value.c_str())); }, (CKPARAMETER_DESC *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "NativePointer get_DefaultValue() const", asFUNCTIONPR([](const CKPARAMETER_DESC *self) { return NativePointer(self->DefaultValue); }, (const CKPARAMETER_DESC *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPARAMETER_DESC", "void set_DefaultValue(NativePointer ptr)", asFUNCTIONPR([](CKPARAMETER_DESC *self, NativePointer ptr) { self->DefaultValue = reinterpret_cast<CKBYTE *>(ptr.Get()); }, (CKPARAMETER_DESC *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKBEHAVIORIO_DESC(asIScriptEngine *engine) {
    int r = 0;

    // r = engine->RegisterObjectProperty("CKBEHAVIORIO_DESC", "uintptr_t Name", asOFFSET(CKBEHAVIORIO_DESC, Name)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBEHAVIORIO_DESC", "CKDWORD Flags", asOFFSET(CKBEHAVIORIO_DESC, Flags)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBEHAVIORIO_DESC", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKBEHAVIORIO_DESC *self) { new(self) CKBEHAVIORIO_DESC(); }, (CKBEHAVIORIO_DESC*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBEHAVIORIO_DESC", asBEHAVE_CONSTRUCT, "void f(const CKBEHAVIORIO_DESC &in other)", asFUNCTIONPR([](const CKBEHAVIORIO_DESC &desc, CKBEHAVIORIO_DESC *self) { new(self) CKBEHAVIORIO_DESC(desc); }, (const CKBEHAVIORIO_DESC &, CKBEHAVIORIO_DESC *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBEHAVIORIO_DESC", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKBEHAVIORIO_DESC *self) { self->~CKBEHAVIORIO_DESC(); }, (CKBEHAVIORIO_DESC *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBEHAVIORIO_DESC", "CKBEHAVIORIO_DESC &opAssign(const CKBEHAVIORIO_DESC &in other)", asMETHODPR(CKBEHAVIORIO_DESC, operator=, (const CKBEHAVIORIO_DESC &), CKBEHAVIORIO_DESC &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBEHAVIORIO_DESC", "string get_Name() const", asFUNCTIONPR([](const CKBEHAVIORIO_DESC *self) -> std::string { return ScriptStringify(self->Name); }, (const CKBEHAVIORIO_DESC *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBEHAVIORIO_DESC", "void set_Name(const string &in value)", asFUNCTIONPR([](CKBEHAVIORIO_DESC *self, const std::string &value) { CKDeletePointer(self->Name); self->Name = CKStrdup(const_cast<CKSTRING>(value.c_str())); }, (CKBEHAVIORIO_DESC *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKBehaviorPrototype

void RegisterCKBehaviorPrototype(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareInput(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->DeclareInput(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareOutput(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->DeclareOutput(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareInParameter(const string &in name, CKGUID guidType, const string &in defaultVal = void)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, const std::string &defaultVal) { return self->DeclareInParameter(const_cast<CKSTRING>(name.c_str()), guidType, const_cast<CKSTRING>(defaultVal.c_str())); }, (CKBehaviorPrototype *, const std::string &, CKGUID, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareInParameter(const string &in name, CKGUID guidType, NativePointer defaultVal, int valSize)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, NativePointer defaultVal, int valSize) { return self->DeclareInParameter(const_cast<CKSTRING>(name.c_str()), guidType, defaultVal.Get(), valSize); }, (CKBehaviorPrototype *, const std::string &, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareOutParameter(const string &in name, CKGUID guidType, const string &in defaultVal = void)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, const std::string &defaultVal) { return self->DeclareOutParameter(const_cast<CKSTRING>(name.c_str()), guidType, const_cast<CKSTRING>(defaultVal.c_str())); }, (CKBehaviorPrototype *, const std::string &, CKGUID, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareOutParameter(const string &in name, CKGUID guidType, NativePointer defaultVal, int valSize)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, NativePointer defaultVal, int valSize) { return self->DeclareOutParameter(const_cast<CKSTRING>(name.c_str()), guidType, defaultVal.Get(), valSize); }, (CKBehaviorPrototype *, const std::string &, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareLocalParameter(const string &in name, CKGUID guidType, const string &in defaultVal = void)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, const std::string &defaultVal) { return self->DeclareLocalParameter(const_cast<CKSTRING>(name.c_str()), guidType, const_cast<CKSTRING>(defaultVal.c_str())); }, (CKBehaviorPrototype *, const std::string &, CKGUID, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareLocalParameter(const string &in name, CKGUID guidType, NativePointer defaultVal, int valSize)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, NativePointer defaultVal, int valSize) { return self->DeclareLocalParameter(const_cast<CKSTRING>(name.c_str()), guidType, defaultVal.Get(), valSize); }, (CKBehaviorPrototype *, const std::string &, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareSetting(const string &in name, CKGUID guidType, const string &in defaultVal = void)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, const std::string &defaultVal) { return self->DeclareSetting(const_cast<CKSTRING>(name.c_str()), guidType, const_cast<CKSTRING>(defaultVal.c_str())); }, (CKBehaviorPrototype *, const std::string &, CKGUID, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int DeclareSetting(const string &in name, CKGUID guidType, NativePointer defaultVal, int valSize)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name, CKGUID guidType, NativePointer defaultVal, int valSize) { return self->DeclareSetting(const_cast<CKSTRING>(name.c_str()), guidType, defaultVal.Get(), valSize); }, (CKBehaviorPrototype *, const std::string &, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetGuid(const CKGUID &in guid)", asMETHODPR(CKBehaviorPrototype, SetGuid, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKGUID GetGuid()", asMETHODPR(CKBehaviorPrototype, GetGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetFlags(CK_BEHAVIORPROTOTYPE_FLAGS flags)", asMETHODPR(CKBehaviorPrototype, SetFlags, (CK_BEHAVIORPROTOTYPE_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CK_BEHAVIORPROTOTYPE_FLAGS GetFlags()", asMETHODPR(CKBehaviorPrototype, GetFlags, (), CK_BEHAVIORPROTOTYPE_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetApplyToClassID(CK_CLASSID cid)", asMETHODPR(CKBehaviorPrototype, SetApplyToClassID, (CK_CLASSID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CK_CLASSID GetApplyToClassID()", asMETHODPR(CKBehaviorPrototype, GetApplyToClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetFunction(NativePointer fct)", asMETHODPR(CKBehaviorPrototype, SetFunction, (CKBEHAVIORFCT), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "NativePointer GetFunction()", asMETHODPR(CKBehaviorPrototype, GetFunction, (), CKBEHAVIORFCT), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetBehaviorCallbackFct(NativePointer fct, CKDWORD callbackMask, NativePointer param)", asMETHODPR(CKBehaviorPrototype, SetBehaviorCallbackFct, (CKBEHAVIORCALLBACKFCT, CKDWORD, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "NativePointer GetBehaviorCallbackFct()", asMETHODPR(CKBehaviorPrototype, GetBehaviorCallbackFct, (), CKBEHAVIORCALLBACKFCT), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetBehaviorFlags(CK_BEHAVIOR_FLAGS flags)", asMETHODPR(CKBehaviorPrototype, SetBehaviorFlags, (CK_BEHAVIOR_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CK_BEHAVIOR_FLAGS GetBehaviorFlags()", asMETHODPR(CKBehaviorPrototype, GetBehaviorFlags, (), CK_BEHAVIOR_FLAGS), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "string GetName()", asFUNCTIONPR([](CKBehaviorPrototype *self) -> std::string { return ScriptStringify(self->GetName()); }, (CKBehaviorPrototype *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetInputCount()", asMETHODPR(CKBehaviorPrototype, GetInputCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetOutputCount()", asMETHODPR(CKBehaviorPrototype, GetOutputCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetInParameterCount()", asMETHODPR(CKBehaviorPrototype, GetInParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetOutParameterCount()", asMETHODPR(CKBehaviorPrototype, GetOutParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetLocalParameterCount()", asMETHODPR(CKBehaviorPrototype, GetLocalParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKBEHAVIORIO_DESC &GetInIOList(int index)", asFUNCTIONPR([](CKBehaviorPrototype *self, int index) -> CKBEHAVIORIO_DESC * {
        if (index >= 0 && index < self->GetInputCount() && self->GetInIOList()) {
            return self->GetInIOList()[index];
        }
        return nullptr;
    }, (CKBehaviorPrototype *, int), CKBEHAVIORIO_DESC *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKBEHAVIORIO_DESC &GetOutIOList(int index)", asFUNCTIONPR([](CKBehaviorPrototype *self, int index) -> CKBEHAVIORIO_DESC * {
        if (index >= 0 && index < self->GetOutputCount() && self->GetOutIOList()) {
            return self->GetOutIOList()[index];
        }
        return nullptr;
    }, (CKBehaviorPrototype *, int), CKBEHAVIORIO_DESC *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKPARAMETER_DESC &GetInParameterList(int index)", asFUNCTIONPR([](CKBehaviorPrototype *self, int index) -> CKPARAMETER_DESC * {
        if (index >= 0 && index < self->GetInParameterCount() && self->GetInParameterList()) {
            return self->GetInParameterList()[index];
        }
        return nullptr;
    }, (CKBehaviorPrototype *, int), CKPARAMETER_DESC *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKPARAMETER_DESC &GetOutParameterList(int index)", asFUNCTIONPR([](CKBehaviorPrototype *self, int index) -> CKPARAMETER_DESC * {
        if (index >= 0 && index < self->GetOutParameterCount() && self->GetOutParameterList()) {
            return self->GetOutParameterList()[index];
        }
        return nullptr;
    }, (CKBehaviorPrototype *, int), CKPARAMETER_DESC *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKPARAMETER_DESC &GetLocalParameterList(int index)", asFUNCTIONPR([](CKBehaviorPrototype *self, int index) -> CKPARAMETER_DESC * {
        if (index >= 0 && index < self->GetLocalParameterCount() && self->GetLocalParameterList()) {
            return self->GetLocalParameterList()[index];
        }
        return nullptr;
    }, (CKBehaviorPrototype *, int), CKPARAMETER_DESC *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "CKObjectDeclaration@ GetSourceObjectDeclaration()", asMETHODPR(CKBehaviorPrototype, GetSoureObjectDeclaration, (), CKObjectDeclaration*), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "void SetSourceObjectDeclaration(CKObjectDeclaration@ decl)", asMETHODPR(CKBehaviorPrototype, SetSourceObjectDeclaration, (CKObjectDeclaration*), void), asCALL_THISCALL); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetInIOIndex(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->GetInIOIndex(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetOutIOIndex(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->GetOutIOIndex(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetInParamIndex(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->GetInParamIndex(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetOutParamIndex(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->GetOutParamIndex(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "int GetLocalParamIndex(const string &in name)", asFUNCTIONPR([](CKBehaviorPrototype *self, const std::string &name) { return self->GetLocalParamIndex(const_cast<CKSTRING>(name.c_str())); }, (CKBehaviorPrototype *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorPrototype", "bool IsRunTime()", asFUNCTIONPR([](CKBehaviorPrototype *self) -> bool { return self->IsRunTime(); }, (CKBehaviorPrototype *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKBitmapSlot

void RegisterCKBitmapSlot(asIScriptEngine *engine) {
    int r = 0;

    // r = engine->RegisterObjectProperty("CKBitmapSlot", "uintptr_t m_DataBuffer", asOFFSET(CKBitmapSlot, m_DataBuffer)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapSlot", "XString m_FileName", asOFFSET(CKBitmapSlot, m_FileName)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBitmapSlot", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKBitmapSlot *self) { new(self) CKBitmapSlot(); }, (CKBitmapSlot*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBitmapSlot", asBEHAVE_CONSTRUCT, "void f(const CKBitmapSlot &in other)", asFUNCTIONPR([](const CKBitmapSlot &slot, CKBitmapSlot *self) { new(self) CKBitmapSlot(slot); }, (const CKBitmapSlot &, CKBitmapSlot *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBitmapSlot", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKBitmapSlot *self) { self->~CKBitmapSlot(); }, (CKBitmapSlot *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapSlot", "CKBitmapSlot &opAssign(const CKBitmapSlot &in other)", asMETHODPR(CKBitmapSlot, operator=, (const CKBitmapSlot &), CKBitmapSlot &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapSlot", "void Allocate(int, int, int)", asMETHOD(CKBitmapSlot, Allocate), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapSlot", "void Free()", asMETHOD(CKBitmapSlot, Free), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapSlot", "void Resize(VxImageDescEx & src, VxImageDescEx & dest)", asMETHOD(CKBitmapSlot, Resize), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapSlot", "void Flush()", asMETHOD(CKBitmapSlot, Flush), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapSlot", "NativePointer get_m_DataBuffer() const", asFUNCTIONPR([](const CKBitmapSlot *self) { return NativePointer(self->m_DataBuffer); }, (const CKBitmapSlot *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapSlot", "void set_m_DataBuffer(NativePointer ptr)", asFUNCTIONPR([](CKBitmapSlot *self, NativePointer ptr) { self->m_DataBuffer = reinterpret_cast<unsigned long *>(ptr.Get()); }, (CKBitmapSlot *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKMovieInfo

void RegisterCKMovieInfo(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKMovieInfo", "XString m_MovieFileName", asOFFSET(CKMovieInfo, m_MovieFileName)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMovieInfo", "CKMovieReader@ m_MovieReader", asOFFSET(CKMovieInfo, m_MovieReader)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMovieInfo", "int m_MovieCurrentSlot", asOFFSET(CKMovieInfo, m_MovieCurrentSlot)); assert(r >= 0);
}

// CKBitmapData

void RegisterCKBitmapData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKBitmapData", "CKMovieInfo@ m_MovieInfo", asOFFSET(CKBitmapData, m_MovieInfo)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKBitmapData", "XArray<CKBitmapSlot *> m_Slots", asOFFSET(CKBitmapData, m_Slots)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "int m_Width", asOFFSET(CKBitmapData, m_Width)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "int m_Height", asOFFSET(CKBitmapData, m_Height)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "int m_CurrentSlot", asOFFSET(CKBitmapData, m_CurrentSlot)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "int m_PickThreshold", asOFFSET(CKBitmapData, m_PickThreshold)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "CKDWORD m_BitmapFlags", asOFFSET(CKBitmapData, m_BitmapFlags)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBitmapData", "CKDWORD m_TransColor", asOFFSET(CKBitmapData, m_TransColor)); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectBehaviour("CKBitmapData", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKBitmapData *self) { new(self) CKBitmapData(); }, (CKBitmapData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
#else
    r = engine->RegisterObjectBehaviour("CKBitmapData", asBEHAVE_CONSTRUCT, "void f(CKContext@ context)", asFUNCTIONPR([](CKBitmapData *self, CKContext *context) { new(self) CKBitmapData(context); }, (CKBitmapData *, CKContext *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
#endif
    r = engine->RegisterObjectBehaviour("CKBitmapData", asBEHAVE_CONSTRUCT, "void f(const CKBitmapData &in other)", asFUNCTIONPR([](const CKBitmapData &data, CKBitmapData *self) { new(self) CKBitmapData(data); }, (const CKBitmapData &, CKBitmapData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBitmapData", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKBitmapData *self) { self->~CKBitmapData(); }, (CKBitmapData *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "CKBitmapData &opAssign(const CKBitmapData &in other)", asMETHODPR(CKBitmapData, operator=, (const CKBitmapData &), CKBitmapData &), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKBitmapData", "bool CreateImage(int width, int height, int bpp = 32, int slot = 0)", asFUNCTIONPR([](CKBitmapData *self, int width, int height, int bpp, int slot) -> bool { return self->CreateImage(width, height, bpp, slot); }, (CKBitmapData *, int, int, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKBitmapData", "bool CreateImage(int width, int height, int bpp = 32, int slot = 0, NativePointer imagePointer = 0)", asFUNCTIONPR([](CKBitmapData *self, int width, int height, int bpp, int slot, NativePointer imagePointer) -> bool { return self->CreateImage(width, height, bpp, slot, imagePointer.Get()); }, (CKBitmapData *, int, int, int, int, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SaveImage(const string &in name, int slot = 0, bool useFormat = false)", asFUNCTIONPR([](CKBitmapData *self, const std::string &name, int slot, bool useFormat) -> bool { return self->SaveImage(const_cast<CKSTRING>(name.c_str()), slot, useFormat); }, (CKBitmapData *, const std::string &, int, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SaveImageAlpha(const string &in name, int slot = 0)", asFUNCTIONPR([](CKBitmapData *self, const std::string &name, int slot) -> bool { return self->SaveImageAlpha(const_cast<CKSTRING>(name.c_str()), slot); }, (CKBitmapData *, const std::string &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "string GetMovieFileName()", asFUNCTIONPR([](CKBitmapData *self) -> std::string { return ScriptStringify(self->GetMovieFileName()); }, (CKBitmapData *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "CKMovieReader@ GetMovieReader()", asMETHODPR(CKBitmapData, GetMovieReader, (), CKMovieReader *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "NativePointer LockSurfacePtr(int slot = -1)", asFUNCTIONPR([](CKBitmapData *self, int slot) { return NativePointer(self->LockSurfacePtr(slot)); }, (CKBitmapData *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool ReleaseSurfacePtr(int slot = -1)", asFUNCTIONPR([](CKBitmapData *self, int slot) -> bool { return self->ReleaseSurfacePtr(slot); }, (CKBitmapData *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "string GetSlotFileName(int slot)", asFUNCTIONPR([](CKBitmapData *self, int slot) -> std::string { return ScriptStringify(self->GetSlotFileName(slot)); }, (CKBitmapData *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SetSlotFileName(int slot, const string &in filename)", asFUNCTIONPR([](CKBitmapData *self, int slot, const std::string &filename) -> bool { return self->SetSlotFileName(slot, const_cast<CKSTRING>(filename.c_str())); }, (CKBitmapData *, int, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "int GetWidth()", asMETHODPR(CKBitmapData, GetWidth, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "int GetHeight()", asMETHODPR(CKBitmapData, GetHeight, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool GetImageDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](CKBitmapData *self, VxImageDescEx &desc) -> bool { return self->GetImageDesc(desc); }, (CKBitmapData *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "int GetSlotCount()", asMETHODPR(CKBitmapData, GetSlotCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SetSlotCount(int count)", asFUNCTIONPR([](CKBitmapData *self, int count) -> bool { return self->SetSlotCount(count); }, (CKBitmapData *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SetCurrentSlot(int slot)", asFUNCTIONPR([](CKBitmapData *self, int slot) -> bool { return self->SetCurrentSlot(slot); }, (CKBitmapData *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "int GetCurrentSlot()", asMETHODPR(CKBitmapData, GetCurrentSlot, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool ReleaseSlot(int slot)", asFUNCTIONPR([](CKBitmapData *self, int slot) -> bool { return self->ReleaseSlot(slot); }, (CKBitmapData *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool ReleaseAllSlots()", asFUNCTIONPR([](CKBitmapData *self) -> bool { return self->ReleaseAllSlots(); }, (CKBitmapData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "bool SetPixel(int x, int y, CKDWORD color, int slot = -1)", asFUNCTIONPR([](CKBitmapData *self, int x, int y, CKDWORD color, int slot) -> bool { return self->SetPixel(x, y, color, slot); }, (CKBitmapData *, int, int, CKDWORD, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "CKDWORD GetPixel(int x, int y, int slot = -1)", asMETHODPR(CKBitmapData, GetPixel, (int, int, int), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "CKDWORD GetTransparentColor()", asMETHODPR(CKBitmapData, GetTransparentColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetTransparentColor(CKDWORD color)", asMETHODPR(CKBitmapData, SetTransparentColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetTransparent(bool transparency)", asFUNCTIONPR([](CKBitmapData *self, bool transparency) { self->SetTransparent(transparency); }, (CKBitmapData *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool IsTransparent()", asFUNCTIONPR([](CKBitmapData *self) -> bool { return self->IsTransparent(); }, (CKBitmapData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "CK_TEXTURE_SAVEOPTIONS GetSaveOptions()", asMETHODPR(CKBitmapData, GetSaveOptions, (), CK_BITMAP_SAVEOPTIONS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetSaveOptions(CK_TEXTURE_SAVEOPTIONS options)", asMETHODPR(CKBitmapData, SetSaveOptions, (CK_BITMAP_SAVEOPTIONS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "CKBitmapProperties@ GetSaveFormat()", asMETHODPR(CKBitmapData, GetSaveFormat, (), CKBitmapProperties *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetSaveFormat(CKBitmapProperties@ format)", asMETHODPR(CKBitmapData, SetSaveFormat, (CKBitmapProperties *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "void SetPickThreshold(int pt)", asMETHODPR(CKBitmapData, SetPickThreshold, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "int GetPickThreshold()", asMETHODPR(CKBitmapData, GetPickThreshold, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "void SetCubeMap(bool cubeMap)", asFUNCTIONPR([](CKBitmapData *self, bool cubeMap) { self->SetCubeMap(cubeMap); }, (CKBitmapData *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool IsCubeMap()", asFUNCTIONPR([](CKBitmapData *self) -> bool { return self->IsCubeMap(); }, (CKBitmapData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "bool ResizeImages(int width, int height)", asFUNCTIONPR([](CKBitmapData *self, int width, int height) -> bool { return self->ResizeImages(width, height); }, (CKBitmapData *, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetDynamicHint(bool dynamic)", asFUNCTIONPR([](CKBitmapData *self, bool dynamic) { self->SetDynamicHint(dynamic); }, (CKBitmapData *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool GetDynamicHint()", asFUNCTIONPR([](CKBitmapData *self) -> bool { return self->GetDynamicHint(); }, (CKBitmapData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "bool ToRestore()", asFUNCTIONPR([](CKBitmapData *self) -> bool { return self->ToRestore(); }, (CKBitmapData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKBitmapData", "bool LoadSlotImage(XString name, int slot = 0)", asFUNCTIONPR([](CKBitmapData *self, XString name, int slot) -> bool { return self->LoadSlotImage(name, slot); }, (CKBitmapData *, XString, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool LoadMovieFile(XString name)", asFUNCTIONPR([](CKBitmapData *self, XString name) -> bool { return self->LoadMovieFile(name); }, (CKBitmapData *, XString), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "CKMovieInfo@ CreateMovieInfo(XString s, CKMovieProperties@ &out mp)", asMETHODPR(CKBitmapData, CreateMovieInfo, (XString, CKMovieProperties**), CKMovieInfo*), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKBitmapData", "bool LoadSlotImage(const XString &name, int slot = 0)", asFUNCTIONPR([](CKBitmapData *self, const XString &name, int slot) -> bool { return self->LoadSlotImage(name, slot); }, (CKBitmapData *, const XString &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool LoadMovieFile(const XString &name)", asFUNCTIONPR([](CKBitmapData *self, const XString &name) -> bool { return self->LoadMovieFile(name); }, (CKBitmapData *, const XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "CKMovieInfo@ CreateMovieInfo(const XString &s, CKMovieProperties@ &out mp)", asMETHODPR(CKBitmapData, CreateMovieInfo, (const XString &, CKMovieProperties **), CKMovieInfo *), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetMovieInfo(CKMovieInfo@ mi)", asMETHODPR(CKBitmapData, SetMovieInfo, (CKMovieInfo *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBitmapData", "void SetAlphaForTransparentColor(const VxImageDescEx &in desc)", asMETHODPR(CKBitmapData, SetAlphaForTransparentColor, (const VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "void SetBorderColorForClamp(const VxImageDescEx &in desc)", asMETHODPR(CKBitmapData, SetBorderColorForClamp, (const VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBitmapData", "bool SetSlotImage(int slot, NativePointer buffer, const VxImageDescEx &in desc)", asFUNCTIONPR([](CKBitmapData *self, int slot, NativePointer buffer, VxImageDescEx &desc) -> bool { return self->SetSlotImage(slot, buffer.Get(), desc); }, (CKBitmapData *, int, NativePointer, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKBitmapData", "bool DumpToChunk(CKStateChunk@ chunk, CKContext@ context, CKFile@ file, NativeBuffer Identifiers)", asFUNCTIONPR([](CKBitmapData *self, CKStateChunk *chunk, CKContext *context, CKFile *file, NativeBuffer identifiers) -> bool { return self->DumpToChunk(chunk, context, file, identifiers); }, (CKBitmapData *, CKStateChunk *, CKContext *, CKFile *, NativeBuffer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBitmapData", "bool ReadFromChunk(CKStateChunk@ chunk, CKContext@ context, CKFile@ file, NativeBuffer Identifiers)", asFUNCTIONPR([](CKBitmapData *self, CKStateChunk *chunk, CKContext *context, CKFile *file, NativeBuffer identifiers) -> bool { return self->ReadFromChunk(chunk, context, file, identifiers); }, (CKBitmapData *, CKStateChunk *, CKContext *, CKFile *, NativeBuffer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKVertexBuffer

void RegisterCKVertexBuffer(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKVertexBuffer", "void Destroy()", asMETHOD(CKVertexBuffer, Destroy), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKVertexBuffer", "CKVB_STATE Check(CKRenderContext@ ctx, uint maxVertexCount, CKRST_DPFLAGS format, bool dynamic = false)", asFUNCTIONPR([](CKVertexBuffer *self, CKRenderContext *ctx, CKDWORD maxVertexCount, CKRST_DPFLAGS format, bool dynamic) { return self->Check(ctx, maxVertexCount, format, dynamic); }, (CKVertexBuffer *, CKRenderContext *, CKDWORD, CKRST_DPFLAGS, bool), CKVB_STATE), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKVertexBuffer", "VxDrawPrimitiveData &Lock(CKRenderContext@ ctx, CKDWORD startVertex, CKDWORD vertexCount, CKLOCKFLAGS lockFlags = CK_LOCK_DEFAULT)", asMETHOD(CKVertexBuffer, Lock), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKVertexBuffer", "void Unlock(CKRenderContext@ ctx)", asMETHOD(CKVertexBuffer, Unlock), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKVertexBuffer", "bool Draw(CKRenderContext@ ctx, VXPRIMITIVETYPE pType, NativePointer indices, int indexCount, CKDWORD startVertex, CKDWORD vertexCount)", asFUNCTIONPR([](CKVertexBuffer *self, CKRenderContext *ctx, VXPRIMITIVETYPE pType, NativePointer indices, int indexCount, CKDWORD startVertex, CKDWORD vertexCount) -> bool { return self->Draw(ctx, pType, reinterpret_cast<CKWORD *>(indices.Get()), indexCount, startVertex, vertexCount); }, (CKVertexBuffer *, CKRenderContext *, VXPRIMITIVETYPE, NativePointer, int, CKDWORD, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKFloorPoint

void RegisterCKFloorPoint(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKFloorPoint", "CK_ID m_UpFloor", offsetof(CKFloorPoint, m_UpFloor)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "int m_UpFaceIndex", offsetof(CKFloorPoint, m_UpFaceIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "VxVector m_UpNormal", offsetof(CKFloorPoint, m_UpNormal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "float m_UpDistance", offsetof(CKFloorPoint, m_UpDistance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "CK_ID m_DownFloor", offsetof(CKFloorPoint, m_DownFloor)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "int m_DownFaceIndex", offsetof(CKFloorPoint, m_DownFaceIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "VxVector m_DownNormal", offsetof(CKFloorPoint, m_DownNormal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFloorPoint", "float m_DownDistance", offsetof(CKFloorPoint, m_DownDistance)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFloorPoint", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFloorPoint *self) { new(self) CKFloorPoint(); }, (CKFloorPoint *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFloorPoint", asBEHAVE_CONSTRUCT, "void f(const CKFloorPoint &in other)", asFUNCTIONPR([](const CKFloorPoint &point, CKFloorPoint *self) { new(self) CKFloorPoint(point); }, (const CKFloorPoint &, CKFloorPoint *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFloorPoint", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFloorPoint *self) { self->~CKFloorPoint(); }, (CKFloorPoint *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorPoint", "CKFloorPoint &opAssign(const CKFloorPoint &in other)", asMETHODPR(CKFloorPoint, operator=, (const CKFloorPoint &), CKFloorPoint &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorPoint", "void Clear()", asMETHOD(CKFloorPoint, Clear), asCALL_THISCALL); assert(r >= 0);
}

// SoundMinion

void RegisterSoundMinion(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("SoundMinion", "CKSOUNDHANDLE m_Source", asOFFSET(SoundMinion, m_Source)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "CKSOUNDHANDLE m_OriginalSource", asOFFSET(SoundMinion, m_OriginalSource)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "CK_ID m_Entity", asOFFSET(SoundMinion, m_Entity)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "CK_ID m_OriginalSound", asOFFSET(SoundMinion, m_OriginalSound)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "VxVector m_Position", asOFFSET(SoundMinion, m_Position)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "VxVector m_Direction", asOFFSET(SoundMinion, m_Direction)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "VxVector m_OldPosition", asOFFSET(SoundMinion, m_OldPosition)); assert(r >= 0);
    r = engine->RegisterObjectProperty("SoundMinion", "float m_TimeStamp", asOFFSET(SoundMinion, m_TimeStamp)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("SoundMinion", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](SoundMinion *self) { new(self) SoundMinion(); }, (SoundMinion *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("SoundMinion", asBEHAVE_CONSTRUCT, "void f(const SoundMinion &in other)", asFUNCTIONPR([](const SoundMinion &sm, SoundMinion *self) { new(self) SoundMinion(sm); }, (const SoundMinion &, SoundMinion *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("SoundMinion", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](SoundMinion *self) { self->~SoundMinion(); }, (SoundMinion *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("SoundMinion", "SoundMinion &opAssign(const SoundMinion &in other)", asMETHODPR(SoundMinion, operator=, (const SoundMinion &), SoundMinion &), asCALL_THISCALL); assert(r >= 0);
}

// CKWaveSoundSettings

void RegisterCKWaveSoundSettings(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKWaveSoundSettings", "float m_Gain", offsetof(CKWaveSoundSettings, m_Gain)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSoundSettings", "float m_Eq", offsetof(CKWaveSoundSettings, m_Eq)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSoundSettings", "float m_Pitch", offsetof(CKWaveSoundSettings, m_Pitch)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSoundSettings", "float m_Priority", offsetof(CKWaveSoundSettings, m_Priority)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSoundSettings", "float m_Pan", offsetof(CKWaveSoundSettings, m_Pan)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveSoundSettings", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKWaveSoundSettings *self) { new(self) CKWaveSoundSettings(); }, (CKWaveSoundSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKWaveSoundSettings", asBEHAVE_CONSTRUCT, "void f(const CKWaveSoundSettings &in other)", asFUNCTIONPR([](const CKWaveSoundSettings &s, CKWaveSoundSettings *self) { new(self) CKWaveSoundSettings(s); }, (const CKWaveSoundSettings &, CKWaveSoundSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveSoundSettings", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKWaveSoundSettings *self) { self->~CKWaveSoundSettings(); }, (CKWaveSoundSettings *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSoundSettings", "CKWaveSoundSettings &opAssign(const CKWaveSoundSettings &in other)", asMETHODPR(CKWaveSoundSettings, operator=, (const CKWaveSoundSettings &), CKWaveSoundSettings &), asCALL_THISCALL); assert(r >= 0);
}

// CKWaveSound3DSettings

void RegisterCKWaveSound3DSettings(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "uint16 m_HeadRelative", offsetof(CKWaveSound3DSettings, m_HeadRelative)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "uint16 m_MuteAfterMax", offsetof(CKWaveSound3DSettings, m_MuteAfterMax)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "float m_InAngle", offsetof(CKWaveSound3DSettings, m_InAngle)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "float m_OutAngle", offsetof(CKWaveSound3DSettings, m_OutAngle)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "float m_OutsideGain", offsetof(CKWaveSound3DSettings, m_OutsideGain)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "float m_MinDistance", offsetof(CKWaveSound3DSettings, m_MinDistance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "float m_MaxDistance", offsetof(CKWaveSound3DSettings, m_MaxDistance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "VxVector m_Position", offsetof(CKWaveSound3DSettings, m_Position)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "VxVector m_Velocity", offsetof(CKWaveSound3DSettings, m_Velocity)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "VxVector m_OrientationDir", offsetof(CKWaveSound3DSettings, m_OrientationDir)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveSound3DSettings", "VxVector m_OrientationUp", offsetof(CKWaveSound3DSettings, m_OrientationUp)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveSound3DSettings", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKWaveSound3DSettings *self) { new(self) CKWaveSound3DSettings(); }, (CKWaveSound3DSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKWaveSound3DSettings", asBEHAVE_CONSTRUCT, "void f(const CKWaveSound3DSettings &in other)", asFUNCTIONPR([](const CKWaveSound3DSettings &s, CKWaveSound3DSettings *self) { new(self) CKWaveSound3DSettings(s); }, (const CKWaveSound3DSettings &, CKWaveSound3DSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveSound3DSettings", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKWaveSound3DSettings *self) { self->~CKWaveSound3DSettings(); }, (CKWaveSound3DSettings *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound3DSettings", "CKWaveSound3DSettings &opAssign(const CKWaveSound3DSettings &in other)", asMETHODPR(CKWaveSound3DSettings, operator=, (const CKWaveSound3DSettings &), CKWaveSound3DSettings &), asCALL_THISCALL); assert(r >= 0);
}

// CKListenerSettings

void RegisterCKListenerSettings(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKListenerSettings", "float m_DistanceFactor", offsetof(CKListenerSettings, m_DistanceFactor)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKListenerSettings", "float m_DopplerFactor", offsetof(CKListenerSettings, m_DopplerFactor)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKListenerSettings", "float m_RollOff", offsetof(CKListenerSettings, m_RollOff)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKListenerSettings", "float m_GlobalGain", offsetof(CKListenerSettings, m_GlobalGain)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKListenerSettings", "float m_PriorityBias", offsetof(CKListenerSettings, m_PriorityBias)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKListenerSettings", "uint m_SoftwareSources", offsetof(CKListenerSettings, m_SoftwareSources)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKListenerSettings", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKListenerSettings *self) { new(self) CKListenerSettings(); }, (CKListenerSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKListenerSettings", asBEHAVE_CONSTRUCT, "void f(const CKListenerSettings &in other)", asFUNCTIONPR([](const CKListenerSettings &s, CKListenerSettings *self) { new(self) CKListenerSettings(s); }, (const CKListenerSettings &, CKListenerSettings *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKListenerSettings", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKListenerSettings *self) { self->~CKListenerSettings(); }, (CKListenerSettings *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKListenerSettings", "CKListenerSettings &opAssign(const CKListenerSettings &in other)", asMETHODPR(CKListenerSettings, operator=, (const CKListenerSettings &), CKListenerSettings &), asCALL_THISCALL); assert(r >= 0);
}

// CKWaveFormat

void RegisterCKWaveFormat(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKWaveFormat", "uint16 wFormatTag", offsetof(CKWaveFormat, wFormatTag)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint16 nChannels", offsetof(CKWaveFormat, nChannels)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint nSamplesPerSec", offsetof(CKWaveFormat, nSamplesPerSec)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint nAvgBytesPerSec", offsetof(CKWaveFormat, nAvgBytesPerSec)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint16 nBlockAlign", offsetof(CKWaveFormat, nBlockAlign)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint16 wBitsPerSample", offsetof(CKWaveFormat, wBitsPerSample)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKWaveFormat", "uint16 cbSize", offsetof(CKWaveFormat, cbSize)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveFormat", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKWaveFormat *self) { new(self) CKWaveFormat(); }, (CKWaveFormat *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKWaveFormat", asBEHAVE_CONSTRUCT, "void f(const CKWaveFormat &in other)", asFUNCTIONPR([](const CKWaveFormat &s, CKWaveFormat *self) { new(self) CKWaveFormat(s); }, (const CKWaveFormat &, CKWaveFormat *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKWaveFormat", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKWaveFormat *self) { self->~CKWaveFormat(); }, (CKWaveFormat *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveFormat", "CKWaveFormat &opAssign(const CKWaveFormat &in other)", asMETHODPR(CKWaveFormat, operator=, (const CKWaveFormat &), CKWaveFormat &), asCALL_THISCALL); assert(r >= 0);
}

// ImpactDesc

void RegisterImpactDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("ImpactDesc", "CK_ID m_OwnerEntity", offsetof(ImpactDesc, m_OwnerEntity)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "CK_ID m_ObstacleTouched", offsetof(ImpactDesc, m_ObstacleTouched)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "CK_ID m_SubObstacleTouched", offsetof(ImpactDesc, m_SubObstacleTouched)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "int m_TouchedVertex", offsetof(ImpactDesc, m_TouchedVertex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "int m_TouchingVertex", offsetof(ImpactDesc, m_TouchingVertex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "int m_TouchedFace", offsetof(ImpactDesc, m_TouchedFace)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "int m_TouchingFace", offsetof(ImpactDesc, m_TouchingFace)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "VxMatrix m_ImpactWorldMatrix", offsetof(ImpactDesc, m_ImpactWorldMatrix)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "VxVector m_ImpactPoint", offsetof(ImpactDesc, m_ImpactPoint)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "VxVector m_ImpactNormal", offsetof(ImpactDesc, m_ImpactNormal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("ImpactDesc", "CK_ID m_Entity", offsetof(ImpactDesc, m_Entity)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("ImpactDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](ImpactDesc *self) { new(self) ImpactDesc(); }, (ImpactDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("ImpactDesc", asBEHAVE_CONSTRUCT, "void f(const ImpactDesc &in other)", asFUNCTIONPR([](const ImpactDesc &desc, ImpactDesc *self) { new(self) ImpactDesc(desc); }, (const ImpactDesc &, ImpactDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("ImpactDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](ImpactDesc *self) { self->~ImpactDesc(); }, (ImpactDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("ImpactDesc", "ImpactDesc &opAssign(const ImpactDesc &in other)", asMETHODPR(ImpactDesc, operator=, (const ImpactDesc &), ImpactDesc &), asCALL_THISCALL); assert(r >= 0);
}

// CKPICKRESULT

void RegisterCKPICKRESULT(asIScriptEngine *engine) {
#if CKVERSION == 0x13022002
    int r = 0;

    r = engine->RegisterObjectProperty("CKPICKRESULT", "VxVector IntersectionPoint", asOFFSET(CKPICKRESULT, IntersectionPoint)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "VxVector IntersectionNormal", asOFFSET(CKPICKRESULT, IntersectionNormal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "float TexU", asOFFSET(CKPICKRESULT, TexU)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "float TexV", asOFFSET(CKPICKRESULT, TexV)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "float Distance", asOFFSET(CKPICKRESULT, Distance)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "int FaceIndex", asOFFSET(CKPICKRESULT, FaceIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPICKRESULT", "CK_ID Sprite", asOFFSET(CKPICKRESULT, Sprite)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPICKRESULT", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPICKRESULT *self) { new(self) CKPICKRESULT(); }, (CKPICKRESULT *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPICKRESULT", asBEHAVE_CONSTRUCT, "void f(const CKPICKRESULT &in other)", asFUNCTIONPR([](const CKPICKRESULT &res, CKPICKRESULT *self) { new(self) CKPICKRESULT(res); }, (const CKPICKRESULT &, CKPICKRESULT *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPICKRESULT", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPICKRESULT *self) { self->~CKPICKRESULT(); }, (CKPICKRESULT *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPICKRESULT", "CKPICKRESULT &opAssign(const CKPICKRESULT &in other)", asMETHODPR(CKPICKRESULT, operator=, (const CKPICKRESULT &), CKPICKRESULT &), asCALL_THISCALL); assert(r >= 0);
#endif
}

// CKSquare

void RegisterCKSquare(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKSquare", "CKDWORD val", asOFFSET(CKSquare, dval)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKSquare", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKSquare *self) { new(self) CKSquare(); }, (CKSquare *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKSquare", asBEHAVE_CONSTRUCT, "void f(const CKSquare &in other)", asFUNCTIONPR([](const CKSquare &s, CKSquare *self) { new(self) CKSquare(s); }, (const CKSquare &, CKSquare *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKSquare", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKSquare *self) { self->~CKSquare(); }, (CKSquare *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSquare", "CKSquare &opAssign(const CKSquare &in other)", asMETHODPR(CKSquare, operator=, (const CKSquare &), CKSquare &), asCALL_THISCALL); assert(r >= 0);
}

// CK2dCurvePoint

void RegisterCK2dCurvePoint(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CK2dCurvePoint", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CK2dCurvePoint *self) { new(self) CK2dCurvePoint(); }, (CK2dCurvePoint *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CK2dCurvePoint", asBEHAVE_CONSTRUCT, "void f(const CK2dCurvePoint &in other)", asFUNCTIONPR([](const CK2dCurvePoint &p, CK2dCurvePoint *self) { new(self) CK2dCurvePoint(p); }, (const CK2dCurvePoint &, CK2dCurvePoint *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CK2dCurvePoint", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CK2dCurvePoint *self) { self->~CK2dCurvePoint(); }, (CK2dCurvePoint *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "CK2dCurvePoint &opAssign(const CK2dCurvePoint &in other)", asMETHODPR(CK2dCurvePoint, operator=, (const CK2dCurvePoint &), CK2dCurvePoint &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "CK2dCurve &GetCurve() const", asMETHOD(CK2dCurvePoint, GetCurve), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "float GetBias() const", asMETHOD(CK2dCurvePoint, GetBias), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetBias(float b)", asMETHOD(CK2dCurvePoint, SetBias), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "float GetTension() const", asMETHOD(CK2dCurvePoint, GetTension), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetTension(float t)", asMETHOD(CK2dCurvePoint, SetTension), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "float GetContinuity() const", asMETHOD(CK2dCurvePoint, GetContinuity), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetContinuity(float c)", asMETHOD(CK2dCurvePoint, SetContinuity), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "bool IsLinear() const", asFUNCTIONPR([](CK2dCurvePoint *self) -> bool { return self->IsLinear(); }, (CK2dCurvePoint *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetLinear(bool linear = false)", asFUNCTIONPR([](CK2dCurvePoint *self, bool linear) { self->SetLinear(linear); }, (CK2dCurvePoint *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "bool IsTCB() const", asFUNCTIONPR([](CK2dCurvePoint *self) -> bool { return self->IsTCB(); }, (CK2dCurvePoint *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void UseTCB(bool use = true)", asFUNCTIONPR([](CK2dCurvePoint *self, bool use) { self->UseTCB(use); }, (CK2dCurvePoint *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "float GetLength() const", asMETHOD(CK2dCurvePoint, GetLength), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "Vx2DVector &GetPosition()", asMETHOD(CK2dCurvePoint, GetPosition), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetPosition(const Vx2DVector &in pos)", asMETHOD(CK2dCurvePoint, SetPosition), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "Vx2DVector &GetInTangent()", asMETHOD(CK2dCurvePoint, GetInTangent), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "Vx2DVector &GetOutTangent()", asMETHOD(CK2dCurvePoint, GetOutTangent), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetInTangent(const Vx2DVector &in input)", asMETHOD(CK2dCurvePoint, SetInTangent), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void SetOutTangent(const Vx2DVector &in output)", asMETHOD(CK2dCurvePoint, SetOutTangent), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurvePoint", "void NotifyUpdate()", asMETHOD(CK2dCurvePoint, NotifyUpdate), asCALL_THISCALL); assert(r >= 0);
}

// CK2dCurve

void RegisterCK2dCurve(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectBehaviour("CK2dCurve", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CK2dCurve *self) { new(self) CK2dCurve(); }, (CK2dCurve *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CK2dCurve", asBEHAVE_CONSTRUCT, "void f(const CK2dCurve &in other)", asFUNCTIONPR([](const CK2dCurve &c, CK2dCurve *self) { new(self) CK2dCurve(c); }, (const CK2dCurve &, CK2dCurve *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CK2dCurve", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CK2dCurve *self) { self->~CK2dCurve(); }, (CK2dCurve *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurve", "CK2dCurve &opAssign(const CK2dCurve &in other)", asMETHODPR(CK2dCurve, operator=, (const CK2dCurve &), CK2dCurve &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurve", "float GetLength()", asMETHOD(CK2dCurve, GetLength), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "CKERROR GetPos(float step, Vx2DVector &out pos)", asMETHOD(CK2dCurve, GetPos), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "float GetY(float x)", asMETHOD(CK2dCurve, GetY), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurve", "void DeleteControlPoint(CK2dCurvePoint &in cpt)", asMETHOD(CK2dCurve, DeleteControlPoint), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "void AddControlPoint(const Vx2DVector &in pos)", asMETHOD(CK2dCurve, AddControlPoint), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "int GetControlPointCount()", asMETHOD(CK2dCurve, GetControlPointCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "CK2dCurvePoint &GetControlPoint(int pos)", asMETHOD(CK2dCurve, GetControlPoint), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurve", "void Update()", asMETHOD(CK2dCurve, Update), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CK2dCurve", "CKStateChunk@ Dump()", asMETHOD(CK2dCurve, Dump), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CK2dCurve", "CKERROR Read(CKStateChunk@ chunk)", asMETHOD(CK2dCurve, Read), asCALL_THISCALL); assert(r >= 0);
}

// CKKeyframeData

template <typename T>
static void RegisterCKKeyMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    XString decl;

    r = engine->RegisterObjectProperty(name, "float TimeStep", asOFFSET(T, TimeStep)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](T *self) { new(self) T(); }, (T *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in other)", name);
    r = engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const T &k, T *self) { new(self) T(k); }, (const T &, T *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name, asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](T *self) { self->~T(); }, (T *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name, name);
    r = engine->RegisterObjectMethod(name, decl.CStr(), asMETHODPR(T, operator=, (const T &), T &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetTime() const", asMETHODPR(T, GetTime, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTime(float t)", asMETHODPR(T, SetTime, (float), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKKey") != 0) {
        RegisterClassValueCast<T, CKKey>(engine, name, "CKKey");
    }
}

template <typename T>
static void RegisterCKRotationKeyMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    XString decl;

    RegisterCKKeyMembers<T>(engine, name);

    r = engine->RegisterObjectProperty(name, "VxQuaternion Rot", asOFFSET(T, Rot)); assert(r >= 0);


    r = engine->RegisterObjectMethod(name, "const VxQuaternion &GetRotation() const", asMETHODPR(T, GetRotation, (), const VxQuaternion &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetRotation(const VxQuaternion &in)", asMETHODPR(T, SetRotation, (const VxQuaternion &), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool Compare(%s &in key, float threshold) const", name);
    r = engine->RegisterObjectMethod(name, decl.CStr(), asFUNCTIONPR([](T *self, T &key, float threshold) -> bool { return self->Compare(key, threshold); }, (T *, T &, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKRotationKey") != 0) {
        RegisterClassValueCast<T, CKRotationKey>(engine, name, "CKRotationKey");
    }
}

template <typename T>
static void RegisterCKPositionKeyMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    XString decl;

    RegisterCKKeyMembers<T>(engine, name);

    r = engine->RegisterObjectProperty(name, "VxVector Pos", asOFFSET(T, Pos)); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "const VxVector &GetPosition() const", asMETHODPR(T, GetPosition, (), const VxVector &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetPosition(const VxVector &in)", asMETHODPR(T, SetPosition, (const VxVector &), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool Compare(%s &in key, float threshold) const", name);
    r = engine->RegisterObjectMethod(name, decl.CStr(), asFUNCTIONPR([](T *self, T &key, float threshold) -> bool { return self->Compare(key, threshold); }, (T *, T &, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKPositionKey") != 0) {
        RegisterClassValueCast<T, CKPositionKey>(engine, name, "CKPositionKey");
    }
}

template<typename T>
void RegisterCKAnimControllerMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectMethod(name, "bool Evaluate(float timeStep, NativePointer res)", asFUNCTIONPR([](T *self, float timeStep, NativePointer res) -> bool { return self->Evaluate(timeStep, res.Get()); }, (T *, float, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int AddKey(CKKey &in key)", asMETHODPR(T, AddKey, (CKKey *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKKey &GetKey(int index)", asMETHODPR(T, GetKey, (int), CKKey *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveKey(int index)", asMETHODPR(T, RemoveKey, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int DumpKeysTo(NativePointer buffer)", asFUNCTIONPR([](T *self, NativePointer buffer) -> int { return self->DumpKeysTo(buffer.Get()); }, (T *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int ReadKeysFrom(NativePointer buffer)", asFUNCTIONPR([](T *self, NativePointer buffer) -> int { return self->ReadKeysFrom(buffer.Get()); }, (T *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool Compare(CKAnimController@ control, float threshold = 0.0)", asFUNCTIONPR([](T *self, CKAnimController *control, float threshold) -> bool { return self->Compare(control, threshold); }, (T *, CKAnimController *, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool Clone(CKAnimController@ control)", asFUNCTIONPR([](T *self, CKAnimController *control) -> bool { return self->Clone(control); }, (T *, CKAnimController *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetKeyCount()", asMETHODPR(T, GetKeyCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetType()", asMETHODPR(T, GetType, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetLength(float l)", asMETHODPR(T, SetLength, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetLength()", asMETHODPR(T, GetLength, (), float), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKAnimController") != 0) {
        RegisterClassRefCast<T, CKAnimController>(engine, name, "CKAnimController");
    }
}

void RegisterCKKeyframeData(asIScriptEngine *engine) {
    int r = 0;

    // CKKey
    RegisterCKKeyMembers<CKKey>(engine, "CKKey");

    // CKRotationKey
    RegisterCKRotationKeyMembers<CKRotationKey>(engine, "CKRotationKey");

    r = engine->RegisterObjectBehaviour("CKRotationKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxQuaternion &in rot)", asFUNCTIONPR([](CKRotationKey *self, float time, VxQuaternion &rot) { new (self) CKRotationKey(time, rot); }, (CKRotationKey *, float, VxQuaternion &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // CKPositionKey
    RegisterCKPositionKeyMembers<CKPositionKey>(engine, "CKPositionKey");

    r = engine->RegisterObjectBehaviour("CKPositionKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxVector &in pos)", asFUNCTIONPR([](CKPositionKey *self, float time, VxVector &pos) { new (self) CKPositionKey(time, pos); }, (CKPositionKey *, float, VxVector &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // CKTCBPositionKey
    RegisterCKPositionKeyMembers<CKTCBPositionKey>(engine, "CKTCBPositionKey");

    r = engine->RegisterObjectProperty("CKTCBPositionKey", "float tension", asOFFSET(CKTCBPositionKey, tension)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBPositionKey", "float continuity", asOFFSET(CKTCBPositionKey, continuity)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBPositionKey", "float bias", asOFFSET(CKTCBPositionKey, bias)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBPositionKey", "float easeto", asOFFSET(CKTCBPositionKey, easeto)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBPositionKey", "float easefrom", asOFFSET(CKTCBPositionKey, easefrom)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKTCBPositionKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxVector &in pos)", asFUNCTIONPR([](CKTCBPositionKey *self, float time, VxVector &pos) { new (self) CKTCBPositionKey(time, pos); }, (CKTCBPositionKey *, float, VxVector &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // CKRotationKey
    RegisterCKRotationKeyMembers<CKTCBRotationKey>(engine, "CKTCBRotationKey");

    r = engine->RegisterObjectProperty("CKTCBRotationKey", "float tension", asOFFSET(CKTCBRotationKey, tension)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBRotationKey", "float continuity", asOFFSET(CKTCBRotationKey, continuity)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBRotationKey", "float bias", asOFFSET(CKTCBRotationKey, bias)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBRotationKey", "float easeto", asOFFSET(CKTCBRotationKey, easeto)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKTCBRotationKey", "float easefrom", asOFFSET(CKTCBRotationKey, easefrom)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKTCBRotationKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxQuaternion &in rot)", asFUNCTIONPR([](CKTCBRotationKey *self, float time, VxQuaternion &rot) { new (self) CKTCBRotationKey(time, rot); }, (CKTCBRotationKey *, float, VxQuaternion &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // CKBezierKeyFlags
    r = engine->RegisterObjectBehaviour("CKBezierKeyFlags", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKBezierKeyFlags *self) { new(self) CKBezierKeyFlags(); }, (CKBezierKeyFlags *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBezierKeyFlags", asBEHAVE_CONSTRUCT, "void f(const CKBezierKeyFlags &in other)", asFUNCTIONPR([](const CKBezierKeyFlags &f, CKBezierKeyFlags *self) { new(self) CKBezierKeyFlags(f); }, (const CKBezierKeyFlags &, CKBezierKeyFlags *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBezierKeyFlags", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKBezierKeyFlags *self) { self->~CKBezierKeyFlags(); }, (CKBezierKeyFlags *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "CKBezierKeyFlags &opAssign(const CKBezierKeyFlags &in other)", asMETHODPR(CKBezierKeyFlags, operator=, (const CKBezierKeyFlags &), CKBezierKeyFlags &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "bool opIneq(const CKBezierKeyFlags &in) const", asFUNCTIONPR([](const CKBezierKeyFlags &lhs, const CKBezierKeyFlags &rhs) -> bool { return lhs != rhs; }, (const CKBezierKeyFlags &, const CKBezierKeyFlags &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "CKBEZIERKEY_FLAGS GetInTangentMode()", asMETHODPR(CKBezierKeyFlags, GetInTangentMode, (), CKBEZIERKEY_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "CKBEZIERKEY_FLAGS GetOutTangentMode()", asMETHODPR(CKBezierKeyFlags, GetOutTangentMode, (), CKBEZIERKEY_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "void SetInTangentMode(CKBEZIERKEY_FLAGS f)", asMETHODPR(CKBezierKeyFlags, SetInTangentMode, (CKBEZIERKEY_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBezierKeyFlags", "void SetOutTangentMode(CKBEZIERKEY_FLAGS f)", asMETHODPR(CKBezierKeyFlags, SetOutTangentMode, (CKBEZIERKEY_FLAGS), void), asCALL_THISCALL); assert(r >= 0);

    // CKBezierPositionKey
    RegisterCKPositionKeyMembers<CKBezierPositionKey>(engine, "CKBezierPositionKey");

    r = engine->RegisterObjectProperty("CKBezierPositionKey", "CKBezierKeyFlags Flags", asOFFSET(CKBezierPositionKey, Flags)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBezierPositionKey", "VxVector In", asOFFSET(CKBezierPositionKey, In)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKBezierPositionKey", "VxVector Out", asOFFSET(CKBezierPositionKey, Out)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKBezierPositionKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxVector &in pos, const CKBezierKeyFlags &in flags, const VxVector &in input, const VxVector &in output)", asFUNCTIONPR([](CKBezierPositionKey *self, float time, VxVector &pos, const CKBezierKeyFlags &flags, VxVector &in, VxVector &out) { new (self) CKBezierPositionKey(time, pos, flags, in, out); }, (CKBezierPositionKey *, float, VxVector &, const CKBezierKeyFlags &, VxVector &, VxVector &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBezierPositionKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxVector &in pos, const CKBezierKeyFlags &in flags)", asFUNCTIONPR([](CKBezierPositionKey *self, float time, VxVector &pos, const CKBezierKeyFlags &flags) { new (self) CKBezierPositionKey(time, pos, flags); }, (CKBezierPositionKey *, float, VxVector &, const CKBezierKeyFlags &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKBezierPositionKey", asBEHAVE_CONSTRUCT, "void f(float time, const VxVector &in pos, const VxVector &in input, const VxVector &in output)", asFUNCTIONPR([](CKBezierPositionKey *self, float time, VxVector &pos, VxVector &in, VxVector &out) { new (self) CKBezierPositionKey(time, pos, in, out); }, (CKBezierPositionKey *, float, VxVector &, VxVector &, VxVector &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBezierPositionKey", "CKBezierKeyFlags &GetFlags()", asMETHODPR(CKBezierPositionKey, GetFlags, (), CKBezierKeyFlags &), asCALL_THISCALL); assert(r >= 0);

    // CKMorphKey
    RegisterCKKeyMembers<CKMorphKey>(engine, "CKMorphKey");

    r = engine->RegisterObjectProperty("CKMorphKey", "VxVector &PosArray", asOFFSET(CKMorphKey, PosArray)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKMorphKey", "VxCompressedVector &NormArray", asOFFSET(CKMorphKey, NormArray)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMorphKey", "bool Compare(CKMorphKey &in key, int nbVertex, float threshold) const", asFUNCTIONPR([](CKMorphKey *self, CKMorphKey &key, int nbVertex, float threshold) -> bool { return self->Compare(key, nbVertex, threshold); }, (CKMorphKey *, CKMorphKey &, int, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // CKAnimController
    RegisterCKAnimControllerMembers<CKAnimController>(engine, "CKAnimController");

    // CKMorphController
    RegisterCKAnimControllerMembers<CKMorphController>(engine, "CKMorphController");

    r = engine->RegisterObjectMethod("CKMorphController", "int AddKey(float timeStep, bool allocateNormals)", asFUNCTIONPR([](CKMorphController *self, float timeStep, bool allocateNormals) { return self->AddKey(timeStep, allocateNormals); }, (CKMorphController *, float, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMorphController", "int AddKey(CKKey &in key, bool allocateNormals)", asFUNCTIONPR([](CKMorphController *self, CKKey &key, bool allocateNormals) { return self->AddKey(&key, allocateNormals); }, (CKMorphController *, CKKey &, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMorphController", "bool Evaluate(float timeStep, int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride, NativePointer normalPtr)", asFUNCTIONPR([](CKMorphController *self, float timeStep, int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride, NativePointer normalPtr) -> bool { return self->Evaluate(timeStep, vertexCount, vertexPtr.Get(), vertexStride, reinterpret_cast<VxCompressedVector *>(normalPtr.Get())); }, (CKMorphController *, float, int, NativePointer, CKDWORD, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMorphController", "void SetMorphVertexCount(int count)", asMETHODPR(CKMorphController, SetMorphVertexCount, (int), void), asCALL_THISCALL); assert(r >= 0);
}

// CKAnimKey

void RegisterCKAnimKey(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKAnimKey", "VxVector m_Pos", asOFFSET(CKAnimKey, m_Pos)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKAnimKey", "VxVector m_Scl", asOFFSET(CKAnimKey, m_Scl)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKAnimKey", "VxQuaternion m_Rot", asOFFSET(CKAnimKey, m_Rot)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKAnimKey", "VxQuaternion m_SclRot", asOFFSET(CKAnimKey, m_SclRot)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKAnimKey", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKAnimKey *self) { new(self) CKAnimKey(); }, (CKAnimKey *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKAnimKey", asBEHAVE_CONSTRUCT, "void f(const CKAnimKey &in other)", asFUNCTIONPR([](const CKAnimKey &k, CKAnimKey *self) { new(self) CKAnimKey(k); }, (const CKAnimKey &, CKAnimKey *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKAnimKey", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKAnimKey *self) { self->~CKAnimKey(); }, (CKAnimKey *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAnimKey", "CKAnimKey &opAssign(const CKAnimKey &in other)", asMETHODPR(CKAnimKey, operator=, (const CKAnimKey &), CKAnimKey &), asCALL_THISCALL); assert(r >= 0);
}

// CKSceneObjectDesc

void RegisterCKSceneObjectDesc(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKSceneObjectDesc", "CK_ID m_Object", asOFFSET(CKSceneObjectDesc, m_Object)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKSceneObjectDesc", "CKStateChunk@ m_InitialValue", asOFFSET(CKSceneObjectDesc, m_InitialValue)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKSceneObjectDesc", "CKDWORD m_Flags", asOFFSET(CKSceneObjectDesc, m_Flags)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKSceneObjectDesc", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKSceneObjectDesc *self) { new(self) CKSceneObjectDesc(); }, (CKSceneObjectDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKSceneObjectDesc", asBEHAVE_CONSTRUCT, "void f(const CKSceneObjectDesc &in other)", asFUNCTIONPR([](const CKSceneObjectDesc &k, CKSceneObjectDesc *self) { new(self) CKSceneObjectDesc(k); }, (const CKSceneObjectDesc &, CKSceneObjectDesc *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKSceneObjectDesc", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKSceneObjectDesc *self) { self->~CKSceneObjectDesc(); }, (CKSceneObjectDesc *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKSceneObjectDesc &opAssign(const CKSceneObjectDesc &in other)", asMETHODPR(CKSceneObjectDesc, operator=, (const CKSceneObjectDesc &), CKSceneObjectDesc &), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKERROR ReadState(CKStateChunk@ chunk)", asMETHODPR(CKSceneObjectDesc, ReadState, (CKStateChunk*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKSceneObjectDesc", "int GetSize()", asMETHODPR(CKSceneObjectDesc, GetSize, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKSceneObjectDesc", "void Clear()", asMETHODPR(CKSceneObjectDesc, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKSceneObjectDesc", "void Init(CKObject@ obj = null)", asMETHODPR(CKSceneObjectDesc, Init, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKDWORD ActiveAtStart()", asMETHODPR(CKSceneObjectDesc, ActiveAtStart, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKDWORD DeActiveAtStart()", asMETHODPR(CKSceneObjectDesc, DeActiveAtStart, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKDWORD NothingAtStart()", asMETHODPR(CKSceneObjectDesc, NothingAtStart, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKDWORD ResetAtStart()", asMETHODPR(CKSceneObjectDesc, ResetAtStart, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSceneObjectDesc", "CKDWORD IsActive()", asMETHODPR(CKSceneObjectDesc, IsActive, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
#endif
}

// CKPatch

static short int CKPatchGetV(CKPatch &patch, int index) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'v'");
        return 0;
    }
    return patch.v[index];
}

static void CKPatchSetV(CKPatch &patch, int index, short int value) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'v'");
        return;
    }
    patch.v[index] = value;
}

static short int CKPatchGetVec(CKPatch &patch, int index) {
    if (index < 0 || index >= 8) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'vec'");
        return 0;
    }
    return patch.vec[index];
}

static void CKPatchSetVec(CKPatch &patch, int index, short int value) {
    if (index < 0 || index >= 8) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'vec'");
        return;
    }
    patch.vec[index] = value;
}

static short int CKPatchGetInterior(CKPatch &patch, int index) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'interior'");
        return 0;
    }
    return patch.interior[index];
}

static void CKPatchSetInterior(CKPatch &patch, int index, short int value) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'interior'");
        return;
    }
    patch.interior[index] = value;
}

static short int CKPatchGetEdge(CKPatch &patch, int index) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'edge'");
        return 0;
    }
    return patch.edge[index];
}

static void CKPatchSetEdge(CKPatch &patch, int index, short int value) {
    if (index < 0 || index >= 4) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of range for 'edge'");
        return;
    }
    patch.edge[index] = value;
}

void RegisterCKPatch(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPatch", "CKDWORD type", asOFFSET(CKPatch, type)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatch", "CKDWORD SmoothingGroup", asOFFSET(CKPatch, SmoothingGroup)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatch", "CK_ID Material", asOFFSET(CKPatch, Material)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatch", "VxVector &auxs", asOFFSET(CKPatch, auxs)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPatch", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKPatch *self) { new(self) CKPatch(); }, (CKPatch *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKPatch", asBEHAVE_CONSTRUCT, "void f(const CKPatch &in other)", asFUNCTIONPR([](const CKPatch &p, CKPatch *self) { new(self) CKPatch(p); }, (const CKPatch &, CKPatch *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKPatch", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKPatch *self) { self->~CKPatch(); }, (CKPatch *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPatch", "CKPatch &opAssign(const CKPatch &in other)", asMETHODPR(CKPatch, operator=, (const CKPatch &), CKPatch &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPatch", "int16 GetV(int index)", asFUNCTION(CKPatchGetV), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatch", "void SetV(int index, int16 value)", asFUNCTION(CKPatchSetV), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPatch", "int16 GetVec(int index)", asFUNCTION(CKPatchGetVec), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatch", "void SetVec(int index, int16 value)", asFUNCTION(CKPatchSetVec), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPatch", "int16 GetInterior(int index)", asFUNCTION(CKPatchGetInterior), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatch", "void SetInterior(int index, int16 value)", asFUNCTION(CKPatchSetInterior), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPatch", "int16 GetEdge(int index)", asFUNCTION(CKPatchGetEdge), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatch", "void SetEdge(int index, int16 value)", asFUNCTION(CKPatchSetEdge), asCALL_CDECL_OBJLAST); assert(r >= 0);
}

void RegisterCKPatchEdge(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 v1", asOFFSET(CKPatchEdge, v1)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 vec12", asOFFSET(CKPatchEdge, vec12)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 vec21", asOFFSET(CKPatchEdge, vec21)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 v2", asOFFSET(CKPatchEdge, v2)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 patch1", asOFFSET(CKPatchEdge, patch1)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKPatchEdge", "int16 patch2", asOFFSET(CKPatchEdge, patch2)); assert(r >= 0);
}

static short int CKTVPatchGetTV(CKTVPatch &patch, int index) {
    if (index < 0 || index >= 4) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return 0;
    }
    return patch.tv[index];
}

static void CKTVPatchSetTV(CKTVPatch &patch, int index, short int tv) {
    if (index < 0 || index >= 4) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return;
    }
    patch.tv[index] = tv;
}

void RegisterCKTVPatch(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKTVPatch", "int16 GetTV(int index)", asFUNCTION(CKTVPatchGetTV), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTVPatch", "void SetTV(int index, int16 value)", asFUNCTION(CKTVPatchSetTV), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKSkinBoneData

void RegisterCKSkinBoneData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKSkinBoneData", "void SetBone(CK3dEntity@ ent)", asMETHOD(CKSkinBoneData, SetBone), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinBoneData", "CK3dEntity@ GetBone() const", asMETHOD(CKSkinBoneData, GetBone), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinBoneData", "void SetBoneInitialInverseMatrix(const VxMatrix &in mat)", asMETHOD(CKSkinBoneData, SetBoneInitialInverseMatrix), asCALL_THISCALL); assert(r >= 0);
}

// CKSkinVertexData

void RegisterCKSkinVertexData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKSkinVertexData", "void SetBoneCount(int boneCount)", asMETHOD(CKSkinVertexData, SetBoneCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "int GetBoneCount() const", asMETHOD(CKSkinVertexData, GetBoneCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "int GetBone(int n) const", asMETHOD(CKSkinVertexData, GetBone), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "void SetBone(int n, int boneIdx)", asMETHOD(CKSkinVertexData, SetBone), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "float GetWeight(int n) const", asMETHOD(CKSkinVertexData, GetWeight), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "void SetWeight(int n, float weight)", asMETHOD(CKSkinVertexData, SetWeight), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "VxVector &GetInitialPos()", asMETHOD(CKSkinVertexData, GetInitialPos), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkinVertexData", "void SetInitialPos(const VxVector &in pos)", asMETHOD(CKSkinVertexData, SetInitialPos), asCALL_THISCALL); assert(r >= 0);
}

// CKSkin

void RegisterCKSkin(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKSkin", "void SetObjectInitMatrix(const VxMatrix &in mat)", asMETHOD(CKSkin, SetObjectInitMatrix), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void SetBoneCount(int boneCount)", asMETHOD(CKSkin, SetBoneCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void SetVertexCount(int count)", asMETHOD(CKSkin, SetVertexCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "int GetBoneCount() const", asMETHOD(CKSkin, GetBoneCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "int GetVertexCount() const", asMETHOD(CKSkin, GetVertexCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void ConstructBoneTransfoMatrices(CKContext@ context)", asMETHOD(CKSkin, ConstructBoneTransfoMatrices), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "bool CalcPoints(int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride)", asFUNCTIONPR([](CKSkin *self, int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride) -> bool { return self->CalcPoints(vertexCount, reinterpret_cast<CKBYTE *>(vertexPtr.Get()), vertexStride); }, (CKSkin *, int, NativePointer, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "CKSkinBoneData@ GetBoneData(int boneIdx) const", asMETHOD(CKSkin, GetBoneData), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "CKSkinVertexData@ GetVertexData(int vertexIdx) const", asMETHOD(CKSkin, GetVertexData), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void RemapVertices(const XIntArray &in permutation)", asMETHOD(CKSkin, RemapVertices), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void SetNormalCount(int count)", asMETHOD(CKSkin, SetNormalCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "int GetNormalCount() const", asMETHOD(CKSkin, GetNormalCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "void SetNormal(int index, const VxVector &in norm)", asMETHOD(CKSkin, SetNormal), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSkin", "VxVector &GetNormal(int index)", asMETHOD(CKSkin, GetNormal), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKSkin", "bool CalcPoints(int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride, NativePointer normalPtr, CKDWORD normalStride)", asFUNCTIONPR([](CKSkin *self, int vertexCount, NativePointer vertexPtr, CKDWORD vertexStride, NativePointer normalPtr, CKDWORD normalStride) -> bool { return self->CalcPoints(vertexCount, reinterpret_cast<CKBYTE *>(vertexPtr.Get()), vertexStride, reinterpret_cast<CKBYTE *>(normalPtr.Get()), normalStride); }, (CKSkin *, int, NativePointer, CKDWORD, NativePointer, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0); // CalcPoints signature differs in our SDK
}

// CKIkJoint

void RegisterCKIkJoint(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKIkJoint", "CKDWORD m_Flags", offsetof(CKIkJoint, m_Flags)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKIkJoint", "VxVector m_Min", offsetof(CKIkJoint, m_Min)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKIkJoint", "VxVector m_Max", offsetof(CKIkJoint, m_Max)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKIkJoint", "VxVector m_Damping", offsetof(CKIkJoint, m_Damping)); assert(r >= 0);
}

// CKFileManagerData

void RegisterCKFileManagerData(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKFileManagerData", "CKStateChunk@ data", asOFFSET(CKFileManagerData, data)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileManagerData", "CKGUID Manager", asOFFSET(CKFileManagerData, Manager)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileManagerData", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFileManagerData *self) { new(self) CKFileManagerData(); }, (CKFileManagerData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFileManagerData", asBEHAVE_CONSTRUCT, "void f(const CKFileManagerData &in other)", asFUNCTIONPR([](const CKFileManagerData &data, CKFileManagerData *self) { new(self) CKFileManagerData(data); }, (const CKFileManagerData &, CKFileManagerData *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileManagerData", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFileManagerData *self) { self->~CKFileManagerData(); }, (CKFileManagerData *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFileManagerData", "CKFileManagerData &opAssign(const CKFileManagerData &in other)", asMETHODPR(CKFileManagerData, operator=, (const CKFileManagerData &), CKFileManagerData &), asCALL_THISCALL); assert(r >= 0);
}

// CKFilePluginDependencies

void RegisterCKFilePluginDependencies(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKFilePluginDependencies", "int m_PluginCategory", asOFFSET(CKFilePluginDependencies, m_PluginCategory)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKFilePluginDependencies", "XArray<CKGUID> m_Guids", asOFFSET(CKFilePluginDependencies, m_Guids)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFilePluginDependencies", "XBitArray ValidGuids", asOFFSET(CKFilePluginDependencies, ValidGuids)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFilePluginDependencies", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFilePluginDependencies *self) { new(self) CKFilePluginDependencies(); }, (CKFilePluginDependencies *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFilePluginDependencies", asBEHAVE_CONSTRUCT, "void f(const CKFilePluginDependencies &in other)", asFUNCTIONPR([](const CKFilePluginDependencies &deps, CKFilePluginDependencies *self) { new(self) CKFilePluginDependencies(deps); }, (const CKFilePluginDependencies &, CKFilePluginDependencies *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFilePluginDependencies", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFilePluginDependencies *self) { self->~CKFilePluginDependencies(); }, (CKFilePluginDependencies *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFilePluginDependencies", "CKFilePluginDependencies &opAssign(const CKFilePluginDependencies &in other)", asMETHODPR(CKFilePluginDependencies, operator=, (const CKFilePluginDependencies &), CKFilePluginDependencies &), asCALL_THISCALL); assert(r >= 0);
}

// CKFileObject

void RegisterCKFileObject(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKFileObject", "CK_ID Object", asOFFSET(CKFileObject, Object)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CK_ID CreatedObject", asOFFSET(CKFileObject, CreatedObject)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CK_CLASSID ObjectCid", asOFFSET(CKFileObject, ObjectCid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CKObject@ ObjPtr", asOFFSET(CKFileObject, ObjPtr)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKFileObject", "uintptr_t Name", asOFFSET(CKFileObject, Name)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CKStateChunk@ Data", asOFFSET(CKFileObject, Data)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "int PostPackSize", asOFFSET(CKFileObject, PostPackSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "int PrePackSize", asOFFSET(CKFileObject, PrePackSize)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CK_FO_OPTIONS Options", asOFFSET(CKFileObject, Options)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "int FileIndex", asOFFSET(CKFileObject, FileIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKFileObject", "CKDWORD SaveFlags", asOFFSET(CKFileObject, SaveFlags)); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileObject", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](CKFileObject *self) { new(self) CKFileObject(); }, (CKFileObject *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("CKFileObject", asBEHAVE_CONSTRUCT, "void f(const CKFileObject &in other)", asFUNCTIONPR([](const CKFileObject &obj, CKFileObject *self) { new(self) CKFileObject(obj); }, (const CKFileObject &, CKFileObject *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("CKFileObject", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](CKFileObject *self) { self->~CKFileObject(); }, (CKFileObject *self), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFileObject", "CKFileObject &opAssign(const CKFileObject &in other)", asMETHODPR(CKFileObject, operator=, (const CKFileObject &), CKFileObject &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFileObject", "bool CanBeLoad()", asFUNCTIONPR([](CKFileObject *self) -> bool { return self->CanBeLoad(); }, (CKFileObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFileObject", "void CleanData()", asMETHOD(CKFileObject, CleanData), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFileObject", "string get_Name() const", asFUNCTIONPR([](const CKFileObject *self) -> std::string { return ScriptStringify(self->Name); }, (const CKFileObject *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFileObject", "void set_Name(const string &in value)", asFUNCTIONPR([](CKFileObject *self, const std::string &value) { CKDeletePointer(self->Name); self->Name = CKStrdup(const_cast<CKSTRING>(value.c_str())); }, (CKFileObject *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

// CKStateChunk

void RegisterCKStateChunk(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKStateChunk", "void StartRead()", asMETHODPR(CKStateChunk, StartRead, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void StartWrite()", asMETHODPR(CKStateChunk, StartWrite, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void CloseChunk()", asMETHODPR(CKStateChunk, CloseChunk, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void Clear()", asMETHODPR(CKStateChunk, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void UpdateDataSize()", asMETHODPR(CKStateChunk, UpdateDataSize, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "CK_CLASSID GetChunkClassID()", asMETHODPR(CKStateChunk, GetChunkClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void Clone(CKStateChunk@ chunk)", asMETHODPR(CKStateChunk, Clone, (CKStateChunk*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "int16 GetDataVersion()", asMETHODPR(CKStateChunk, GetDataVersion, (), short), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void SetDataVersion(int16 version)", asMETHODPR(CKStateChunk, SetDataVersion, (short), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int16 GetChunkVersion()", asMETHODPR(CKStateChunk, GetChunkVersion, (), short), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteIdentifier(CKDWORD id)", asMETHODPR(CKStateChunk, WriteIdentifier, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "CKDWORD ReadIdentifier()", asMETHODPR(CKStateChunk, ReadIdentifier, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "bool SeekIdentifier(CKDWORD identifier)", asFUNCTIONPR([](CKStateChunk *self, CKDWORD identifier) -> bool { return self->SeekIdentifier(identifier); }, (CKStateChunk *, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int SeekIdentifierAndReturnSize(CKDWORD identifier)", asMETHODPR(CKStateChunk, SeekIdentifierAndReturnSize, (CKDWORD), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "int GetCurrentPos()", asMETHODPR(CKStateChunk, GetCurrentPos, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void Skip(int dwordCount)", asMETHODPR(CKStateChunk, Skip, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void Goto(int dwordCount)", asMETHODPR(CKStateChunk, Goto, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int GetDataSize()", asMETHODPR(CKStateChunk, GetDataSize, (), int), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKStateChunk", "CKDWORD ComputeCRC(CKDWORD adler)", asMETHODPR(CKStateChunk, ComputeCRC, (CKDWORD), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void Pack(int compressionLevel)", asMETHODPR(CKStateChunk, Pack, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "bool UnPack(int destSize)", asFUNCTIONPR([](CKStateChunk *self, int destSize) -> bool { return self->UnPack(destSize); }, (CKStateChunk *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteByte(uint8 byte)", asMETHODPR(CKStateChunk, WriteByte, (CKCHAR), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteWord(uint16 data)", asMETHODPR(CKStateChunk, WriteWord, (CKWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteDword(uint32 data)", asMETHODPR(CKStateChunk, WriteDword, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteDwordAsWords(uint32 data)", asMETHODPR(CKStateChunk, WriteDwordAsWords, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteInt(int data)", asMETHODPR(CKStateChunk, WriteInt, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteFloat(float data)", asMETHODPR(CKStateChunk, WriteFloat, (float), void), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteString(const string &in str)", asFUNCTIONPR([](CKStateChunk *self, const std::string &str) { self->WriteString(const_cast<CKSTRING>(str.c_str())); }, (CKStateChunk *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteString(const string &in str)", asFUNCTIONPR([](CKStateChunk *self, const std::string &str) { self->WriteString(str.c_str()); }, (CKStateChunk *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteObjectID(CK_ID id)", asMETHODPR(CKStateChunk, WriteObjectID, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteObject(CKObject@ obj)", asMETHODPR(CKStateChunk, WriteObject, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteGuid(CKGUID data)", asMETHODPR(CKStateChunk, WriteGuid, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteVector(const VxVector &in v)", asMETHODPR(CKStateChunk, WriteVector, (const VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteMatrix(const VxMatrix &in mat)", asMETHODPR(CKStateChunk, WriteMatrix, (const VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteObjectArray(CKObjectArray@ objArray, CKContext@ context = null)", asMETHODPR(CKStateChunk, WriteObjectArray, (CKObjectArray *, CKContext *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteSubChunk(CKStateChunk@ sub)", asMETHODPR(CKStateChunk, WriteSubChunk, (CKStateChunk*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBitmap(BITMAP_HANDLE bitmap, const string &in ext = void)", asFUNCTIONPR([](CKStateChunk *self, BITMAP_HANDLE bitmap, const std::string &ext) { self->WriteBitmap(bitmap, ext.empty() ? nullptr : const_cast<CKSTRING>(ext.c_str())); }, (CKStateChunk *, BITMAP_HANDLE, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteReaderBitmap(const VxImageDescEx &in desc, CKBitmapReader@ reader, CKBitmapProperties@ bp)", asMETHODPR(CKStateChunk, WriteReaderBitmap, (const VxImageDescEx&, CKBitmapReader *, CKBitmapProperties *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteManagerInt(CKGUID manager, int val)", asMETHODPR(CKStateChunk, WriteManagerInt, (CKGUID, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBuffer(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBuffer(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBufferNoSize(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBufferNoSize(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBuffer_LEndian(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBuffer_LEndian(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBuffer_LEndian16(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBuffer_LEndian16(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBufferNoSize_LEndian(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBufferNoSize_LEndian(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteBufferNoSize_LEndian16(int size, NativePointer buf)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buf) { self->WriteBufferNoSize_LEndian16(size, buf.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void StartObjectIDSequence(int count)", asMETHODPR(CKStateChunk, StartObjectIDSequence, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteObjectIDSequence(CK_ID id)", asMETHODPR(CKStateChunk, WriteObjectIDSequence, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteObjectSequence(CKObject@ obj)", asMETHODPR(CKStateChunk, WriteObjectSequence, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void StartSubChunkSequence(int count)", asMETHODPR(CKStateChunk, StartSubChunkSequence, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteSubChunkSequence(CKStateChunk@ sub)", asMETHODPR(CKStateChunk, WriteSubChunkSequence, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void StartManagerSequence(CKGUID man, int count)", asMETHODPR(CKStateChunk, StartManagerSequence, (CKGUID, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteManagerSequence(int val)", asMETHODPR(CKStateChunk, WriteManagerSequence, (int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "int StartReadSequence()", asMETHODPR(CKStateChunk, StartReadSequence, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "int StartManagerReadSequence(CKGUID &out guid)", asMETHODPR(CKStateChunk, StartManagerReadSequence, (CKGUID *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int ReadManagerIntSequence()", asMETHODPR(CKStateChunk, ReadManagerIntSequence, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "CK_ID ReadObjectID()", asMETHODPR(CKStateChunk, ReadObjectID, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "CKObject@ ReadObject(CKContext@ context)", asMETHODPR(CKStateChunk, ReadObject, (CKContext *), CKObject *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "uint8 ReadByte()", asMETHODPR(CKStateChunk, ReadByte, (), CKBYTE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "uint16 ReadWord()", asMETHODPR(CKStateChunk, ReadWord, (), CKWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "CKGUID ReadGuid()", asMETHODPR(CKStateChunk, ReadGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "uint32 ReadDword()", asMETHODPR(CKStateChunk, ReadDword, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "uint32 ReadDwordAsWords()", asMETHODPR(CKStateChunk, ReadDwordAsWords, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int ReadInt()", asMETHODPR(CKStateChunk, ReadInt, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "float ReadFloat()", asMETHODPR(CKStateChunk, ReadFloat, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadVector(VxVector &out v)", asMETHODPR(CKStateChunk, ReadVector, (VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadMatrix(VxMatrix &out mat)", asMETHODPR(CKStateChunk, ReadMatrix, (VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int ReadManagerInt(CKGUID &out guid)", asMETHODPR(CKStateChunk, ReadManagerInt, (CKGUID *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "const XObjectArray &ReadXObjectArray()", asMETHODPR(CKStateChunk, ReadXObjectArray, (), const XObjectArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "const XObjectPointerArray &ReadXObjectArray(CKContext@ context)", asMETHODPR(CKStateChunk, ReadXObjectArray, (CKContext *), const XObjectPointerArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadObjectArray(CKObjectArray@ objArray)", asMETHODPR(CKStateChunk, ReadObjectArray, (CKObjectArray *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "CKObjectArray@ ReadObjectArray()", asMETHODPR(CKStateChunk, ReadObjectArray, (), CKObjectArray *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer(NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer buffer) { self->ReadAndFillBuffer(buffer.Get()); }, (CKStateChunk *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer(int size, NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buffer) { self->ReadAndFillBuffer(size, buffer.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer_LEndian(NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer buffer) { self->ReadAndFillBuffer_LEndian(buffer.Get()); }, (CKStateChunk *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer_LEndian(int size, NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buffer) { self->ReadAndFillBuffer_LEndian(size, buffer.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer_LEndian16(NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer buffer) { self->ReadAndFillBuffer_LEndian16(buffer.Get()); }, (CKStateChunk *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void ReadAndFillBuffer_LEndian16(int size, NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, int size, NativePointer buffer) { self->ReadAndFillBuffer_LEndian16(size, buffer.Get()); }, (CKStateChunk *, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKStateChunk", "CKStateChunk@ ReadSubChunk()", asMETHODPR(CKStateChunk, ReadSubChunk, (), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKStateChunk", "CKStateChunk@ ReadSubChunk(CK_READSUBCHUNK_FLAGS flags = CK_RSC_DEFAULT)", asMETHODPR(CKStateChunk, ReadSubChunk, (CK_READSUBCHUNK_FLAGS), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKStateChunk", "int ReadBuffer(NativePointer &out buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer *buffer) { return self->ReadBuffer(reinterpret_cast<void **>(buffer)); }, (CKStateChunk *, NativePointer *), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int ReadString(string &out str)", asFUNCTIONPR([](CKStateChunk *self, std::string &str) { CKSTRING buffer = nullptr; int result = self->ReadString(&buffer); if (buffer) { str = buffer; } return result; }, (CKStateChunk *, std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "BITMAP_HANDLE ReadBitmap()", asMETHODPR(CKStateChunk, ReadBitmap, (), BITMAP_HANDLE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "bool ReadReaderBitmap(const VxImageDescEx &in desc)", asFUNCTIONPR([](CKStateChunk *self, const VxImageDescEx &desc) -> bool { return self->ReadReaderBitmap(desc); }, (CKStateChunk *, const VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int RemapObject(CK_ID oldID, CK_ID newID)", asMETHODPR(CKStateChunk, RemapObject, (CK_ID, CK_ID), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int RemapObjects(CKContext@ context, CKDependenciesContext@ depContext = null)", asMETHODPR(CKStateChunk, RemapObjects, (CKContext *, CKDependenciesContext *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int RemapManagerInt(CKGUID manager, NativePointer conversionTable, int nbEntries)", asFUNCTIONPR([](CKStateChunk *self, CKGUID manager, NativePointer conversionTable, int nbEntries) { return self->RemapManagerInt(manager, reinterpret_cast<int*>(conversionTable.Get()), nbEntries); }, (CKStateChunk *, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "int RemapParameterInt(CKGUID parameterType, NativePointer conversionTable, int nbEntries)", asFUNCTIONPR([](CKStateChunk *self, CKGUID parameterType, NativePointer conversionTable, int nbEntries) { return self->RemapParameterInt(parameterType, reinterpret_cast<int*>(conversionTable.Get()), nbEntries); }, (CKStateChunk *, CKGUID, NativePointer, int), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "void AddChunk(CKStateChunk@ chunk)", asMETHODPR(CKStateChunk, AddChunk, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void AddChunkAndDelete(CKStateChunk@ chunk)", asMETHODPR(CKStateChunk, AddChunkAndDelete, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "int ConvertToBuffer(NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer buffer) { return self->ConvertToBuffer(buffer.Get()); }, (CKStateChunk *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "bool ConvertFromBuffer(NativePointer buffer)", asFUNCTIONPR([](CKStateChunk *self, NativePointer buffer) -> bool { return self->ConvertFromBuffer(buffer.Get()); }, (CKStateChunk *, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "NativePointer LockWriteBuffer(int dwordCount)", asFUNCTIONPR([](CKStateChunk *self, int dwordCount) { return NativePointer(self->LockWriteBuffer(dwordCount)); }, (CKStateChunk *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "NativePointer LockReadBuffer()", asFUNCTIONPR([](CKStateChunk *self) { return NativePointer(self->LockReadBuffer()); }, (CKStateChunk *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKStateChunk", "NativePointer ReadRawBitmap(VxImageDescEx &out desc)", asFUNCTIONPR([](CKStateChunk *self, VxImageDescEx& desc) { return NativePointer(self->ReadRawBitmap(desc)); }, (CKStateChunk *, VxImageDescEx &), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateChunk", "void WriteRawBitmap(const VxImageDescEx &in desc)", asMETHODPR(CKStateChunk, WriteRawBitmap, (const VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
}

// CKFile

void RegisterCKFile(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKFile", "CKERROR OpenFile(const string &in filename, CK_LOAD_FLAGS flags = CK_LOAD_DEFAULT)", asFUNCTIONPR([](CKFile *self, const std::string &filename, CK_LOAD_FLAGS flags) { return self->OpenFile(const_cast<CKSTRING>(filename.c_str()), flags); }, (CKFile *, const std::string &, CK_LOAD_FLAGS), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "CKERROR OpenMemory(NativePointer buffer, int bufferSize, CK_LOAD_FLAGS flags = CK_LOAD_DEFAULT)", asFUNCTIONPR([](CKFile *self, NativePointer buffer, int bufferSize, CK_LOAD_FLAGS flags) { return self->OpenMemory(buffer.Get(), bufferSize, flags); }, (CKFile *, NativePointer, int, CK_LOAD_FLAGS), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFile", "CKERROR LoadFileData(CKObjectArray@ objArray)", asMETHODPR(CKFile, LoadFileData, (CKObjectArray *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFile", "CKERROR Load(const string &in filename, CKObjectArray@ objArray, CK_LOAD_FLAGS flags = CK_LOAD_DEFAULT)", asFUNCTIONPR([](CKFile *self, const std::string &filename, CKObjectArray *objArray, CK_LOAD_FLAGS flags) { return self->Load(const_cast<CKSTRING>(filename.c_str()), objArray, flags); }, (CKFile *, const std::string &, CKObjectArray *, CK_LOAD_FLAGS), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "CKERROR Load(NativePointer buffer, int bufferSize, CKObjectArray@ objArray, CK_LOAD_FLAGS flags = CK_LOAD_DEFAULT)", asFUNCTIONPR([](CKFile *self, NativePointer buffer, int bufferSize, CKObjectArray * objArray, CK_LOAD_FLAGS flags) { return self->Load(buffer.Get(), bufferSize, objArray, flags); }, (CKFile *, NativePointer, int, CKObjectArray *, CK_LOAD_FLAGS), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFile", "void UpdateAndApplyAnimationsTo(CKCharacter@ character)", asMETHODPR(CKFile, UpdateAndApplyAnimationsTo, (CKCharacter*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFile", "CKERROR StartSave(const string &in filename, CKDWORD flags = CK_STATESAVE_ALL)", asFUNCTIONPR([](CKFile *self, const std::string &filename, CKDWORD flags) { return self->StartSave(const_cast<CKSTRING>(filename.c_str()), flags); }, (CKFile *, const std::string &, CKDWORD), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void SaveObject(CKObject@ obj, CKDWORD flags = CK_STATESAVE_ALL)", asMETHODPR(CKFile, SaveObject, (CKObject *, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void SaveObjects(CKObjectArray@ objArray, CKDWORD flags = CK_STATESAVE_ALL)", asMETHODPR(CKFile, SaveObjects, (CKObjectArray *, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void SaveObjects(const XObjectArray &in ids, CKDWORD flags = CK_STATESAVE_ALL)", asFUNCTIONPR([](CKFile *file, const XObjectArray &objects, CKDWORD flags) { file->SaveObjects(objects.Begin(), objects.Size(), flags); }, (CKFile *, const XObjectArray &, CKDWORD), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void SaveObjectAsReference(CKObject@ obj)", asMETHODPR(CKFile, SaveObjectAsReference, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "CKERROR EndSave()", asMETHODPR(CKFile, EndSave, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFile", "bool IncludeFile(const string &in file, int searchPathCategory = -1)", asFUNCTIONPR([](CKFile *self, const std::string &file, int searchPathCategory) -> bool { return self->IncludeFile(const_cast<CKSTRING>(file.c_str()), searchPathCategory); }, (CKFile *, const std::string &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "bool IsObjectToBeSaved(CK_ID id)", asFUNCTIONPR([](CKFile *self, CK_ID id) -> bool { return self->IsObjectToBeSaved(id); }, (CKFile *, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void LoadAndSave(const string &in filename, const string &in newFileName)", asFUNCTIONPR([](CKFile *self, const std::string &filename, const std::string &newFileName) { self->LoadAndSave(const_cast<CKSTRING>(filename.c_str()), const_cast<CKSTRING>(newFileName.c_str())); }, (CKFile *, const std::string &, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "void RemapManagerInt(CKGUID manager, NativePointer conversionTable, int tableSize)", asFUNCTIONPR([](CKFile *self, CKGUID manager, NativePointer conversionTable, int tableSize) { self->RemapManagerInt(manager, reinterpret_cast<int *>(conversionTable.Get()), tableSize); }, (CKFile *, CKGUID, NativePointer, int), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFile", "XFilePluginDependenciesArray &GetMissingPlugins()", asMETHODPR(CKFile, GetMissingPlugins, (), XClassArray<CKFilePluginDependencies>*), asCALL_THISCALL); assert(r >= 0);
}

// CKDependencies

void RegisterCKDependencies(asIScriptEngine *engine) {
    int r = 0;

    RegisterXSArray<CKDependencies, CKDWORD>(engine, "CKDependencies", "CKDWORD");

    r = engine->RegisterObjectProperty("CKDependencies", "CK_DEPENDENCIES_FLAGS m_Flags", asOFFSET(CKDependencies, m_Flags)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKDependencies", "void ModifyOptions(CK_CLASSID cid, CKDWORD add, CKDWORD remove)", asMETHOD(CKDependencies, ModifyOptions), asCALL_THISCALL); assert(r >= 0);
}

// CKDependenciesContext

void RegisterCKDependenciesContext(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKDependenciesContext", "void AddObjects(NativePointer ids, int count)", asFUNCTIONPR([](CKDependenciesContext *self, NativePointer ids, int count) { self->AddObjects(reinterpret_cast<CK_ID *>(ids.Get()), count); }, (CKDependenciesContext *, NativePointer, int), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKDependenciesContext", "int GetObjectsCount()", asMETHODPR(CKDependenciesContext, GetObjectsCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "CKObject@ GetObjects(int i)", asMETHODPR(CKDependenciesContext, GetObjects, (int), CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "CK_ID RemapID(CK_ID &in id)", asMETHODPR(CKDependenciesContext, RemapID, (CK_ID &), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "CKObject@ Remap(CKObject@ obj)", asMETHODPR(CKDependenciesContext, Remap, (const CKObject *), CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "bool IsDependenciesHere(CK_ID id)", asFUNCTIONPR([](CKDependenciesContext *self, CK_ID id) -> bool { return self->IsDependenciesHere(id); }, (CKDependenciesContext *, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "XObjectArray FillDependencies()", asMETHODPR(CKDependenciesContext, FillDependencies, (), XObjectArray), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "XObjectArray FillRemappedDependencies()", asMETHODPR(CKDependenciesContext, FillRemappedDependencies, (), XObjectArray), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "void StartDependencies(CKDependencies &in d)", asMETHODPR(CKDependenciesContext, StartDependencies, (CKDependencies*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "void StopDependencies()", asMETHODPR(CKDependenciesContext, StopDependencies, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "CKDWORD GetClassDependencies(int c)", asMETHODPR(CKDependenciesContext, GetClassDependencies, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "void Copy(const string &in appendString = void)", asMETHODPR(CKDependenciesContext, Copy, (CKSTRING), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "void SetOperationMode(CK_DEPENDENCIES_OPMODE m)", asMETHODPR(CKDependenciesContext, SetOperationMode, (CK_DEPENDENCIES_OPMODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "bool IsInMode(CK_DEPENDENCIES_OPMODE m)", asFUNCTIONPR([](CKDependenciesContext *self, CK_DEPENDENCIES_OPMODE m) -> bool { return self->IsInMode(m); }, (CKDependenciesContext *, CK_DEPENDENCIES_OPMODE), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "void SetCreationMode(CK_OBJECTCREATION_OPTIONS m)", asMETHODPR(CKDependenciesContext, SetCreationMode, (CK_OBJECTCREATION_OPTIONS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDependenciesContext", "bool ContainClassID(CK_CLASSID cid)", asFUNCTIONPR([](CKDependenciesContext *self, CK_CLASSID cid) -> bool { return self->ContainClassID(cid); }, (CKDependenciesContext *, CK_CLASSID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKDependenciesContext", "CKERROR FinishPrepareDependencies(CKObject@ self, CK_CLASSID cid)", asMETHODPR(CKDependenciesContext, FinishPrepareDependencies, (CKObject *, CK_CLASSID), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKDependenciesContext", "const XHashID &GetDependenciesMap() const", asMETHODPR(CKDependenciesContext, GetDependenciesMap, () const, const XHashID&), asCALL_THISCALL); assert(r >= 0);
}

// CKDebugContext

void RegisterCKDebugContext(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKDebugContext", "float delta", offsetof(CKDebugContext, delta)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKBeObject@ CurrentObject", offsetof(CKDebugContext, CurrentObject)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKBehavior@ CurrentScript", offsetof(CKDebugContext, CurrentScript)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKBehavior@ CurrentBehavior", offsetof(CKDebugContext, CurrentBehavior)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKBehavior@ SubBehavior", offsetof(CKDebugContext, SubBehavior)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKObjectArray ObjectsToExecute", offsetof(CKDebugContext, ObjectsToExecute)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKObjectArray ScriptsToExecute", offsetof(CKDebugContext, ScriptsToExecute)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKObjectArray BehaviorStack", offsetof(CKDebugContext, BehaviorStack)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKContext@ m_Context", offsetof(CKDebugContext, m_Context)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "CKDEBUG_STATE CurrentBehaviorAction", offsetof(CKDebugContext, CurrentBehaviorAction)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKDebugContext", "int InDebug", offsetof(CKDebugContext, InDebug)); assert(r >= 0);

    // Not implemented
    // r = engine->RegisterObjectMethod("CKDebugContext", "void Init(XObjectPointerArray &in array, float delta)", asMETHODPR(CKDebugContext, Init, (XObjectPointerArray &, float), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDebugContext", "void StepInto(CKBehavior@ beh)", asMETHODPR(CKDebugContext, StepInto, (CKBehavior *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDebugContext", "void StepBehavior()", asMETHODPR(CKDebugContext, StepBehavior, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDebugContext", "bool DebugStep()", asMETHODPR(CKDebugContext, DebugStep, (), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDebugContext", "void Clear()", asMETHODPR(CKDebugContext, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
}

// CKObjectArray

void RegisterCKObjectArray(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectMethod("CKObjectArray", "int GetCount()", asMETHODPR(CKObjectArray, GetCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "int GetCurrentPos()", asMETHODPR(CKObjectArray, GetCurrentPos, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "CKObject@ GetData(CKContext@ context)", asMETHODPR(CKObjectArray, GetData, (CKContext *), CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID GetDataId()", asMETHODPR(CKObjectArray, GetDataId, (), CK_ID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID SetDataId(CK_ID id)", asMETHODPR(CKObjectArray, SetDataId, (CK_ID), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID SetData(CKObject@ obj)", asMETHODPR(CKObjectArray, SetData, (CKObject *), CK_ID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void Reset()", asMETHODPR(CKObjectArray, Reset, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "bool PtrSeek(CKObject@ obj)", asFUNCTIONPR([](CKObjectArray *self, CKObject *obj) -> bool { return self->PtrSeek(obj); }, (CKObjectArray *, CKObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool IDSeek(CK_ID id)", asFUNCTIONPR([](CKObjectArray *self, CK_ID id) -> bool { return self->IDSeek(id); }, (CKObjectArray *, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool PositionSeek(int pos)", asFUNCTIONPR([](CKObjectArray *self, int pos) -> bool { return self->PositionSeek(pos); }, (CKObjectArray *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID Seek(int pos)", asMETHODPR(CKObjectArray, Seek, (int), CK_ID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void Next()", asMETHODPR(CKObjectArray, Next, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void Previous()", asMETHODPR(CKObjectArray, Previous, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "int GetPosition(CKObject@ obj)", asMETHODPR(CKObjectArray, GetPosition, (CKObject *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "int GetPosition(CK_ID id)", asMETHODPR(CKObjectArray, GetPosition, (CK_ID), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID PtrFind(CKObject@ obj)", asMETHODPR(CKObjectArray, PtrFind, (CKObject *), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID IDFind(CK_ID id)", asMETHODPR(CKObjectArray, IDFind, (CK_ID), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID PositionFind(int pos)", asMETHODPR(CKObjectArray, PositionFind, (int), CK_ID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertFront(CKObject@ obj)", asMETHODPR(CKObjectArray, InsertFront, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertRear(CKObject@ obj)", asMETHODPR(CKObjectArray, InsertRear, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertAt(CKObject@ obj)", asMETHODPR(CKObjectArray, InsertAt, (CKObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool AddIfNotHere(CKObject@ obj)", asFUNCTIONPR([](CKObjectArray *self, CKObject *obj) -> bool { return self->AddIfNotHere(obj); }, (CKObjectArray *, CKObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKObjectArray", "bool AddIfNotHereSorted(CKObject@ obj, OBJECTARRAYCMPFCT cmpFct, CKContext@ context)", asFUNCTIONPR([](CKObjectArray *self, CKObject *obj, OBJECTARRAYCMPFCT cmpFct, CKContext *context) -> bool { return self->AddIfNotHereSorted(obj, cmpFct, context); }, (CKObjectArray *, CKObject *, OBJECTARRAYCMPFCT, CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertFront(CK_ID id)", asMETHODPR(CKObjectArray, InsertFront, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertRear(CK_ID id)", asMETHODPR(CKObjectArray, InsertRear, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void InsertAt(CK_ID id)", asMETHODPR(CKObjectArray, InsertAt, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool AddIfNotHere(CK_ID id)", asFUNCTIONPR([](CKObjectArray *self, CK_ID id) -> bool { return self->AddIfNotHere(id); }, (CKObjectArray *, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKObjectArray", "bool AddIfNotHereSorted(CK_ID id, OBJECTARRAYCMPFCT cmpFc, CKContext@ context)", asFUNCTIONPR([](CKObjectArray *self, CK_ID id, OBJECTARRAYCMPFCT cmpFc, CKContext *context) -> bool { return self->AddIfNotHereSorted(id, cmpFc, context); }, (CKObjectArray *, CK_ID, OBJECTARRAYCMPFCT, CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "CKERROR Append(CKObjectArray@ objArray)", asMETHODPR(CKObjectArray, Append, (CKObjectArray *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID RemoveFront()", asMETHODPR(CKObjectArray, RemoveFront, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID RemoveRear()", asMETHODPR(CKObjectArray, RemoveRear, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "CK_ID RemoveAt()", asMETHODPR(CKObjectArray, RemoveAt, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool Remove(CKObject@ obj)", asFUNCTIONPR([](CKObjectArray *self, CKObject *obj) -> bool { return self->Remove(obj); }, (CKObjectArray *, CKObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool Remove(CK_ID id)", asFUNCTIONPR([](CKObjectArray *self, CK_ID id) -> bool { return self->Remove(id); }, (CKObjectArray *, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void Clear()", asMETHODPR(CKObjectArray, Clear, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "bool EndOfList()", asFUNCTIONPR([](CKObjectArray *self) -> bool { return self->EndOfList(); }, (CKObjectArray *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "bool ListEmpty()", asFUNCTIONPR([](CKObjectArray *self) -> bool { return self->ListEmpty(); }, (CKObjectArray *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "void SwapCurrentWithNext()", asMETHODPR(CKObjectArray, SwapCurrentWithNext, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectArray", "void SwapCurrentWithPrevious()", asMETHODPR(CKObjectArray, SwapCurrentWithPrevious, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectArray", "bool Check(CKContext@ context)", asFUNCTIONPR([](CKObjectArray *self, CKContext *context) -> bool { return self->Check(context); }, (CKObjectArray *, CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKObjectArray", "void Sort(OBJECTARRAYCMPFCT cmpFct, CKContext@ context)", asMETHODPR(CKObjectArray, Sort, (OBJECTARRAYCMPFCT, CKContext *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKObjectArray", "void InsertSorted(CKObject@ obj, OBJECTARRAYCMPFCT cmpFct, CKContext@ context)", asMETHODPR(CKObjectArray, InsertSorted, (CKObject *, OBJECTARRAYCMPFCT, CKContext *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKObjectArray", "void InsertSorted(CK_ID id, OBJECTARRAYCMPFCT cmpFct, CKContext@ context)", asMETHODPR(CKObjectArray, InsertSorted, (CK_ID, OBJECTARRAYCMPFCT, CKContext *), void), asCALL_THISCALL); assert(r >= 0);
}

// CKObjectDeclaration

void RegisterCKObjectDeclaration(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectProperty("CKObjectDeclaration", "CKGUID m_Guid", offsetof(CKObjectDeclaration, m_Guid)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "CK_CLASSID m_CompatibleClassID", offsetof(CKObjectDeclaration, m_CompatibleClassID)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKObjectDeclaration", "uintptr_t m_CreationFunction", offsetof(CKObjectDeclaration, m_CreationFunction)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "CKDWORD m_Version", offsetof(CKObjectDeclaration, m_Version)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKObjectDeclaration", "uintptr_t m_Description", offsetof(CKObjectDeclaration, m_Description)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "CKBehaviorPrototype@ m_Proto", offsetof(CKObjectDeclaration, m_Proto)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "int m_Type", offsetof(CKObjectDeclaration, m_Type)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "CKGUID m_AuthorGuid", offsetof(CKObjectDeclaration, m_AuthorGuid)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKObjectDeclaration", "uintptr_t m_AuthorName", offsetof(CKObjectDeclaration, m_AuthorName)); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKObjectDeclaration", "uintptr_t m_Category", offsetof(CKObjectDeclaration, m_Category)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "XString m_Name", offsetof(CKObjectDeclaration, m_Name)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "int m_PluginIndex", offsetof(CKObjectDeclaration, m_PluginIndex)); assert(r >= 0);
    r = engine->RegisterObjectProperty("CKObjectDeclaration", "XGUIDArray m_ManagersGuid", offsetof(CKObjectDeclaration, m_ManagersGuid)); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetDescription(const string &in description)", asFUNCTIONPR([](CKObjectDeclaration *self, const std::string &description) { self->SetDescription(const_cast<CKSTRING>(description.c_str())); }, (CKObjectDeclaration *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "string GetDescription()", asFUNCTIONPR([](CKObjectDeclaration *self) -> std::string { return ScriptStringify(self->GetDescription()); }, (CKObjectDeclaration *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetGuid(CKGUID guid)", asMETHODPR(CKObjectDeclaration, SetGuid, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CKGUID GetGuid()", asMETHODPR(CKObjectDeclaration, GetGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetType(int type)", asMETHODPR(CKObjectDeclaration, SetType, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "int GetType()", asMETHODPR(CKObjectDeclaration, GetType, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void NeedManager(CKGUID manager)", asMETHODPR(CKObjectDeclaration, NeedManager, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetCreationFunction(NativePointer f)", asFUNCTIONPR([](CKObjectDeclaration *self, NativePointer f) { self->SetCreationFunction(reinterpret_cast<CKDLL_CREATEPROTOFUNCTION>(f.Get())); }, (CKObjectDeclaration *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "NativePointer GetCreationFunction()", asFUNCTIONPR([](CKObjectDeclaration *self) { return NativePointer(self->GetCreationFunction()); }, (CKObjectDeclaration *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetAuthorGuid(CKGUID guid)", asMETHODPR(CKObjectDeclaration, SetAuthorGuid, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CKGUID GetAuthorGuid()", asMETHODPR(CKObjectDeclaration, GetAuthorGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetAuthorName(const string &in name)", asFUNCTIONPR([](CKObjectDeclaration *self, const std::string &name) { self->SetAuthorName(const_cast<CKSTRING>(name.c_str())); }, (CKObjectDeclaration *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "string GetAuthorName()", asFUNCTIONPR([](CKObjectDeclaration *self) -> std::string { return ScriptStringify(self->GetAuthorName()); }, (CKObjectDeclaration *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetVersion(CKDWORD verion)", asMETHODPR(CKObjectDeclaration, SetVersion, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CKDWORD GetVersion()", asMETHODPR(CKObjectDeclaration, GetVersion, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetCompatibleClassId(CK_CLASSID cid)", asMETHODPR(CKObjectDeclaration, SetCompatibleClassId, (CK_CLASSID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CK_CLASSID GetCompatibleClassId()", asMETHODPR(CKObjectDeclaration, GetCompatibleClassId, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetCategory(const string &in category)", asFUNCTIONPR([](CKObjectDeclaration *self, const std::string &category) { self->SetCategory(const_cast<CKSTRING>(category.c_str())); }, (CKObjectDeclaration *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "string GetCategory()", asFUNCTIONPR([](CKObjectDeclaration *self) -> std::string { return ScriptStringify(self->GetCategory()); }, (CKObjectDeclaration *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "string GetName()", asFUNCTIONPR([](CKObjectDeclaration *self) -> std::string { return ScriptStringify(self->GetName()); }, (CKObjectDeclaration *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "int GetPluginIndex()", asMETHODPR(CKObjectDeclaration, GetPluginIndex, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetPluginIndex(int)", asMETHODPR(CKObjectDeclaration, SetPluginIndex, (int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CKBehaviorPrototype@ GetProto()", asMETHODPR(CKObjectDeclaration, GetProto, (), CKBehaviorPrototype*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "void SetProto(CKBehaviorPrototype@ proto)", asMETHODPR(CKObjectDeclaration, SetProto, (CKBehaviorPrototype*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKObjectDeclaration", "int GetManagerNeededCount()", asMETHODPR(CKObjectDeclaration, GetManagerNeededCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectDeclaration", "CKGUID GetManagerNeeded(int index)", asMETHODPR(CKObjectDeclaration, GetManagerNeeded, (int), CKGUID), asCALL_THISCALL); assert(r >= 0);
}