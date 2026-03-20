// BML AngelScript API Stubs (for IDE autocompletion)
// This file is NOT compiled - it provides type hints only.

// ============================================================================
// Logging
// ============================================================================
void bmlLog(const string &in message);
void bmlLogWarning(const string &in message);
void bmlLogError(const string &in message);

// ============================================================================
// Console
// ============================================================================
void bmlPrint(const string &in message);

// ============================================================================
// Events (IMC)
// ============================================================================
void bmlSubscribe(const string &in topic, const string &in callbackName);
void bmlUnsubscribe(const string &in topic, const string &in callbackName);
void bmlPublish(const string &in topic);
void bmlPublish(const string &in topic, const string &in data);
void bmlPublishInt(const string &in topic, int value);

// ============================================================================
// Object Lookup
// ============================================================================
CKContext@ GetCKContext();
CK3dObject@ Find3dObject(const string &in name);
CKGroup@ FindGroup(const string &in name);
CKMaterial@ FindMaterial(const string &in name);
CKTexture@ FindTexture(const string &in name);
CKMesh@ FindMesh(const string &in name);
CK2dEntity@ Find2dEntity(const string &in name);
CKDataArray@ FindArray(const string &in name);
CKSound@ FindSound(const string &in name);

// ============================================================================
// Configuration
// ============================================================================
int bmlConfigGetInt(const string &in category, const string &in key, int defaultVal = 0);
float bmlConfigGetFloat(const string &in category, const string &in key, float defaultVal = 0.0f);
bool bmlConfigGetBool(const string &in category, const string &in key, bool defaultVal = false);
string bmlConfigGetString(const string &in category, const string &in key, const string &in defaultVal = "");

void bmlConfigSetInt(const string &in category, const string &in key, int value);
void bmlConfigSetFloat(const string &in category, const string &in key, float value);
void bmlConfigSetBool(const string &in category, const string &in key, bool value);
void bmlConfigSetString(const string &in category, const string &in key, const string &in value);

// ============================================================================
// Input
// ============================================================================
bool bmlIsKeyDown(int key);
bool bmlIsKeyToggled(int key);
bool bmlIsMouseButtonDown(int button);
void bmlGetMousePosition(int &out x, int &out y);

enum CKKEY {
    // Row 1: Escape + number row
    CKKEY_ESCAPE = 0x01,
    CKKEY_1 = 0x02, CKKEY_2 = 0x03, CKKEY_3 = 0x04, CKKEY_4 = 0x05,
    CKKEY_5 = 0x06, CKKEY_6 = 0x07, CKKEY_7 = 0x08, CKKEY_8 = 0x09,
    CKKEY_9 = 0x0A, CKKEY_0 = 0x0B,
    CKKEY_MINUS = 0x0C, CKKEY_EQUALS = 0x0D, CKKEY_BACKSPACE = 0x0E,

    // Row 2: Tab + QWERTY
    CKKEY_TAB = 0x0F,
    CKKEY_Q = 0x10, CKKEY_W = 0x11, CKKEY_E = 0x12, CKKEY_R = 0x13, CKKEY_T = 0x14,
    CKKEY_Y = 0x15, CKKEY_U = 0x16, CKKEY_I = 0x17, CKKEY_O = 0x18, CKKEY_P = 0x19,
    CKKEY_LBRACKET = 0x1A, CKKEY_RBRACKET = 0x1B, CKKEY_RETURN = 0x1C,

    // Row 3: Ctrl + home row
    CKKEY_LCONTROL = 0x1D,
    CKKEY_A = 0x1E, CKKEY_S = 0x1F, CKKEY_D = 0x20, CKKEY_F = 0x21,
    CKKEY_G = 0x22, CKKEY_H = 0x23, CKKEY_J = 0x24, CKKEY_K = 0x25, CKKEY_L = 0x26,
    CKKEY_SEMICOLON = 0x27, CKKEY_APOSTROPHE = 0x28, CKKEY_GRAVE = 0x29,

    // Row 4: Shift + bottom row
    CKKEY_LSHIFT = 0x2A, CKKEY_BACKSLASH = 0x2B,
    CKKEY_Z = 0x2C, CKKEY_X = 0x2D, CKKEY_C = 0x2E, CKKEY_V = 0x2F,
    CKKEY_B = 0x30, CKKEY_N = 0x31, CKKEY_M = 0x32,
    CKKEY_COMMA = 0x33, CKKEY_PERIOD = 0x34, CKKEY_SLASH = 0x35, CKKEY_RSHIFT = 0x36,

    // Numpad multiply, alt, space, caps
    CKKEY_MULTIPLY = 0x37, CKKEY_LALT = 0x38, CKKEY_SPACE = 0x39, CKKEY_CAPSLOCK = 0x3A,

    // Function keys
    CKKEY_F1 = 0x3B, CKKEY_F2 = 0x3C, CKKEY_F3 = 0x3D, CKKEY_F4 = 0x3E,
    CKKEY_F5 = 0x3F, CKKEY_F6 = 0x40, CKKEY_F7 = 0x41, CKKEY_F8 = 0x42,
    CKKEY_F9 = 0x43, CKKEY_F10 = 0x44,

    // Locks
    CKKEY_NUMLOCK = 0x45, CKKEY_SCROLLLOCK = 0x46,

    // Numpad
    CKKEY_NUMPAD7 = 0x47, CKKEY_NUMPAD8 = 0x48, CKKEY_NUMPAD9 = 0x49, CKKEY_SUBTRACT = 0x4A,
    CKKEY_NUMPAD4 = 0x4B, CKKEY_NUMPAD5 = 0x4C, CKKEY_NUMPAD6 = 0x4D, CKKEY_ADD = 0x4E,
    CKKEY_NUMPAD1 = 0x4F, CKKEY_NUMPAD2 = 0x50, CKKEY_NUMPAD3 = 0x51,
    CKKEY_NUMPAD0 = 0x52, CKKEY_DECIMAL = 0x53,

    // F11-F12
    CKKEY_F11 = 0x57, CKKEY_F12 = 0x58,

    // Extended keys
    CKKEY_NUMPADENTER = 0x9C, CKKEY_RCONTROL = 0x9D,
    CKKEY_DIVIDE = 0xB5, CKKEY_RALT = 0xB8,

    // Navigation cluster
    CKKEY_HOME = 0xC7, CKKEY_UP = 0xC8, CKKEY_PAGEUP = 0xC9,
    CKKEY_LEFT = 0xCB, CKKEY_RIGHT = 0xCD,
    CKKEY_END = 0xCF, CKKEY_DOWN = 0xD0, CKKEY_PAGEDOWN = 0xD1,
    CKKEY_INSERT = 0xD2, CKKEY_DELETE = 0xD3,

    // Windows keys
    CKKEY_LWIN = 0xDB, CKKEY_RWIN = 0xDC
};

// ============================================================================
// Timers
// ============================================================================
int bmlSetTimer(int delayMs, const string &in callbackName);
int bmlSetInterval(int intervalMs, const string &in callbackName);
void bmlCancelTimer(int timerId);

// ============================================================================
// State Persistence (survives hot-reload, hidden from ModMenu)
// ============================================================================
void bmlSaveState(const string &in key, const string &in value);
string bmlLoadState(const string &in key, const string &in defaultVal = "");
void bmlSaveStateInt(const string &in key, int value);
int bmlLoadStateInt(const string &in key, int defaultVal = 0);
void bmlSaveStateFloat(const string &in key, float value);
float bmlLoadStateFloat(const string &in key, float defaultVal = 0.0f);

// ============================================================================
// Coroutines
// ============================================================================
void bmlYield();
void bmlWaitFrames(int frames);
void bmlWaitSeconds(float seconds);
int bmlGetFrame();

// ============================================================================
// Lifecycle Callbacks (implement these in your script)
// ============================================================================
// void OnAttach();           // Called when mod is loaded
// void OnDetach();           // Called when mod is unloaded
// void OnInit();             // Called when Virtools engine is ready
// void OnPrepareReload();    // Called before hot reload
