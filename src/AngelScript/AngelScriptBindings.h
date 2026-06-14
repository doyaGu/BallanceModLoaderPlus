#ifndef BML_ANGELSCRIPT_BINDINGS_H
#define BML_ANGELSCRIPT_BINDINGS_H

class ModContext;

bool BML_TryRegisterAngelScriptBindings(ModContext *context);
void BML_UnregisterAngelScriptBindings(ModContext *context);

#endif // BML_ANGELSCRIPT_BINDINGS_H
