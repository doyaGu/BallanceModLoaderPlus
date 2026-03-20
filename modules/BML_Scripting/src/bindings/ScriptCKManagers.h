#ifndef CK_SCRIPTCKMANAGERS_H
#define CK_SCRIPTCKMANAGERS_H

#include <angelscript.h>

void RegisterCKPluginManager(asIScriptEngine *engine);
void RegisterCKBaseManager(asIScriptEngine *engine);
void RegisterCKParameterManager(asIScriptEngine *engine);
void RegisterCKAttributeManager(asIScriptEngine *engine);
void RegisterCKTimeManager(asIScriptEngine *engine);
void RegisterCKMessageManager(asIScriptEngine *engine);
void RegisterCKBehaviorManager(asIScriptEngine *engine);
void RegisterCKPathManager(asIScriptEngine *engine);
void RegisterCKRenderManager(asIScriptEngine *engine);
void RegisterCKFloorManager(asIScriptEngine *engine);
void RegisterCKGridManager(asIScriptEngine *engine);
void RegisterCKInterfaceManager(asIScriptEngine *engine);
void RegisterCKSoundManager(asIScriptEngine *engine);
void RegisterCKMidiManager(asIScriptEngine *engine);
void RegisterCKInputManager(asIScriptEngine *engine);
void RegisterCKCollisionManager(asIScriptEngine *engine);

#endif // CK_SCRIPTCKMANAGERS_H
