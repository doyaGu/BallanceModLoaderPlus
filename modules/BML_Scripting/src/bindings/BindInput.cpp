#include "BindInput.h"

#include <cassert>

#include "CKInputManager.h"

#include "bml_logging.h"
#include "bml_virtools.h"
#include "../ModuleScope.h"

namespace BML::Scripting {

namespace {

static bool g_InputReady = false;
static bool g_InputWarnedOnce = false;

CKInputManager *GetIM() {
    if (!g_InputReady) {
        if (!g_InputWarnedOnce) {
            g_InputWarnedOnce = true;
            const BML_Mod owner = CurrentScriptOwner();
            if (g_Services && g_Services->Logging && g_Services->Logging->Log && owner) {
                g_Services->Logging->Log(owner, BML_LOG_WARN, "script",
                    "Input bindings called before engine init - returning nullptr");
            }
        }
        return nullptr;
    }
    if (!g_Services || !g_Services->Context || !g_Services->Module) return nullptr;
    BML_Context ctx = g_Services->Context->Context;
    if (!ctx) return nullptr;
    void *ptr = nullptr;
    if (g_Services->Context->GetUserData(ctx, BML_VIRTOOLS_KEY_INPUTMANAGER, &ptr) != BML_RESULT_OK)
        return nullptr;
    return static_cast<CKInputManager *>(ptr);
}

static bool Script_IsKeyDown(int key) {
    auto *im = GetIM();
    return im ? (im->IsKeyDown(static_cast<CKDWORD>(key)) != 0) : false;
}

static bool Script_IsKeyToggled(int key) {
    auto *im = GetIM();
    return im ? (im->IsKeyToggled(static_cast<CKDWORD>(key)) != 0) : false;
}

static bool Script_IsMouseButtonDown(int button) {
    auto *im = GetIM();
    return im ? (im->IsMouseButtonDown(static_cast<CK_MOUSEBUTTON>(button)) != 0) : false;
}

static void Script_GetMousePosition(int &x, int &y) {
    auto *im = GetIM();
    if (!im) { x = y = 0; return; }
    Vx2DVector pos;
    im->GetMousePosition(pos, FALSE);
    x = static_cast<int>(pos.x);
    y = static_cast<int>(pos.y);
}

} // namespace

void RegisterInputBindings(asIScriptEngine *engine) {
    int r;
    r = engine->RegisterGlobalFunction("bool bmlIsKeyDown(int key)",
        asFUNCTION(Script_IsKeyDown), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool bmlIsKeyToggled(int key)",
        asFUNCTION(Script_IsKeyToggled), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool bmlIsMouseButtonDown(int button)",
        asFUNCTION(Script_IsMouseButtonDown), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlGetMousePosition(int &out x, int &out y)",
        asFUNCTION(Script_GetMousePosition), asCALL_CDECL); assert(r >= 0);

    // Key constants - complete DirectInput scan code enum
    r = engine->RegisterEnum("CKKEY"); assert(r >= 0);

    // Row 1: Escape + number row
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_ESCAPE", 0x01); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_1", 0x02); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_2", 0x03); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_3", 0x04); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_4", 0x05); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_5", 0x06); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_6", 0x07); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_7", 0x08); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_8", 0x09); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_9", 0x0A); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_0", 0x0B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_MINUS", 0x0C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_EQUALS", 0x0D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_BACKSPACE", 0x0E); assert(r >= 0);

    // Row 2: Tab + QWERTY row
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_TAB", 0x0F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_Q", 0x10); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_W", 0x11); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_E", 0x12); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_R", 0x13); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_T", 0x14); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_Y", 0x15); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_U", 0x16); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_I", 0x17); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_O", 0x18); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_P", 0x19); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LBRACKET", 0x1A); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RBRACKET", 0x1B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RETURN", 0x1C); assert(r >= 0);

    // Row 3: Ctrl + home row
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LCONTROL", 0x1D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_A", 0x1E); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_S", 0x1F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_D", 0x20); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F", 0x21); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_G", 0x22); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_H", 0x23); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_J", 0x24); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_K", 0x25); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_L", 0x26); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_SEMICOLON", 0x27); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_APOSTROPHE", 0x28); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_GRAVE", 0x29); assert(r >= 0);

    // Row 4: Shift + bottom row
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LSHIFT", 0x2A); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_BACKSLASH", 0x2B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_Z", 0x2C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_X", 0x2D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_C", 0x2E); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_V", 0x2F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_B", 0x30); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_N", 0x31); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_M", 0x32); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_COMMA", 0x33); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_PERIOD", 0x34); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_SLASH", 0x35); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RSHIFT", 0x36); assert(r >= 0);

    // Numpad multiply + space + caps
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_MULTIPLY", 0x37); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LALT", 0x38); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_SPACE", 0x39); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_CAPSLOCK", 0x3A); assert(r >= 0);

    // Function keys
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F1", 0x3B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F2", 0x3C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F3", 0x3D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F4", 0x3E); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F5", 0x3F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F6", 0x40); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F7", 0x41); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F8", 0x42); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F9", 0x43); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F10", 0x44); assert(r >= 0);

    // Locks
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMLOCK", 0x45); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_SCROLLLOCK", 0x46); assert(r >= 0);

    // Numpad
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD7", 0x47); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD8", 0x48); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD9", 0x49); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_SUBTRACT", 0x4A); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD4", 0x4B); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD5", 0x4C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD6", 0x4D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_ADD", 0x4E); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD1", 0x4F); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD2", 0x50); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD3", 0x51); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPAD0", 0x52); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_DECIMAL", 0x53); assert(r >= 0);

    // F11-F12
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F11", 0x57); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_F12", 0x58); assert(r >= 0);

    // Extended keys
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_NUMPADENTER", 0x9C); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RCONTROL", 0x9D); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_DIVIDE", 0xB5); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RALT", 0xB8); assert(r >= 0);

    // Navigation cluster
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_HOME", 0xC7); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_UP", 0xC8); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_PAGEUP", 0xC9); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LEFT", 0xCB); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RIGHT", 0xCD); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_END", 0xCF); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_DOWN", 0xD0); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_PAGEDOWN", 0xD1); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_INSERT", 0xD2); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_DELETE", 0xD3); assert(r >= 0);

    // Windows keys
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_LWIN", 0xDB); assert(r >= 0);
    r = engine->RegisterEnumValue("CKKEY", "CKKEY_RWIN", 0xDC); assert(r >= 0);
}

void SetInputBindingsReady(bool ready) {
    g_InputReady = ready;
    if (ready) g_InputWarnedOnce = false;
}

} // namespace BML::Scripting
