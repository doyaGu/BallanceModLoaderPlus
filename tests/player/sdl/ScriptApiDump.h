#ifndef BML_TEST_SCRIPT_API_DUMP_H
#define BML_TEST_SCRIPT_API_DUMP_H

#include <CKAngelScript.h>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Reflect the loaded runtime, rather than teaching a source parser another
// registration spelling. The plugin and borrowed engine remain host-owned.
inline std::string DumpBmlScriptApi(const BML::Mod::Context &context) {
    std::string file = context.GetDirectory(BML_DIR_GAME).Take() + "/BuildingBlocks/";
#if defined(_WIN32)
    file += "AngelScript.dll";
#elif defined(__APPLE__)
    file += "libAngelScript.dylib";
#else
    file += "libAngelScript.so";
#endif
    auto *module = SDL_LoadObject(file.c_str());
    if (!module)
        throw std::runtime_error(SDL_GetError());
    struct ReleaseModule {
        SDL_SharedObject *Module;
        ~ReleaseModule() {
            SDL_UnloadObject(Module);
        }
    } release{module};
    const auto get =
        reinterpret_cast<decltype(&CKGetAngelScript)>(SDL_LoadFunction(module, "CKGetAngelScript"));
    const auto borrow = reinterpret_cast<decltype(&CKAngelScriptBorrowEngine)>(
        SDL_LoadFunction(module, "CKAngelScriptBorrowEngine"));
    if (!get || !borrow)
        throw std::runtime_error("CKAngelScript reflection API unavailable");
    asIScriptEngine *engine = nullptr;
    CKAngelScriptResult result{};
    result.Size = sizeof(result);
    if (borrow(get(BML::Engine::BorrowContext(context).Take()), &engine, &result) != CKAS_OK ||
        !engine)
        throw std::runtime_error("Cannot borrow script engine");

    std::map<std::string, std::vector<std::string>> namespaces;
    const auto owned = [](const char *ns) {
        return ns && (std::string_view(ns) == "BML" || std::string_view(ns).starts_with("BML::"));
    };
    for (asUINT i = 0; i < engine->GetEnumCount(); ++i) {
        const auto *type = engine->GetEnumByIndex(i);
        if (!owned(type->GetNamespace()))
            continue;
        std::ostringstream text;
        text << "enum " << type->GetName() << " {\n";
        for (asUINT n = 0; n < type->GetEnumValueCount(); ++n) {
#if ANGELSCRIPT_VERSION >= 23900
            asINT64 value = 0;
#else
            int value = 0;
#endif
            const auto *name = type->GetEnumValueByIndex(n, &value);
            text << "    " << name << " = " << value << ",\n";
        }
        text << "}\n";
        namespaces[type->GetNamespace()].push_back(text.str());
    }
    for (asUINT i = 0; i < engine->GetFuncdefCount(); ++i) {
        const auto *type = engine->GetFuncdefByIndex(i);
        if (owned(type->GetNamespace()))
            namespaces[type->GetNamespace()].push_back(
                std::string("funcdef ") +
                type->GetFuncdefSignature()->GetDeclaration(false, false, true) + ";\n");
    }
    for (asUINT i = 0; i < engine->GetObjectTypeCount(); ++i) {
        const auto *type = engine->GetObjectTypeByIndex(i);
        if (!owned(type->GetNamespace()))
            continue;
        std::ostringstream text;
        text << ((type->GetFlags() & asOBJ_SCRIPT_OBJECT) ? "interface " : "class ")
             << type->GetName() << " {\n";
        std::set<std::string> constructors;
        for (asUINT n = 0; n < type->GetBehaviourCount(); ++n) {
            asEBehaviours behavior;
            auto *function = type->GetBehaviourByIndex(n, &behavior);
            if (behavior != asBEHAVE_CONSTRUCT && behavior != asBEHAVE_FACTORY)
                continue;
            const std::string declaration = function->GetDeclaration(false, false, true);
            auto start = declaration.find('(');
            if (start != std::string::npos)
                constructors.insert(std::string(type->GetName()) + declaration.substr(start));
        }
        for (asUINT n = 0; n < type->GetFactoryCount(); ++n) {
            const std::string declaration =
                type->GetFactoryByIndex(n)->GetDeclaration(false, false, true);
            const auto start = declaration.find('(');
            if (start != std::string::npos)
                constructors.insert(std::string(type->GetName()) + declaration.substr(start));
        }
        for (const auto &constructor : constructors)
            text << "    " << constructor << ";\n";
        for (asUINT n = 0; n < type->GetPropertyCount(); ++n)
            text << "    " << type->GetPropertyDeclaration(n) << ";\n";
        for (asUINT n = 0; n < type->GetMethodCount(); ++n) {
            auto *method = type->GetMethodByIndex(n);
            text << "    " << method->GetDeclaration(false, false, true);
            if (method->IsProperty())
                text << " property";
            text << ";\n";
        }
        text << "}\n";
        namespaces[type->GetNamespace()].push_back(text.str());
    }
    for (asUINT i = 0; i < engine->GetGlobalFunctionCount(); ++i) {
        auto *function = engine->GetGlobalFunctionByIndex(i);
        if (owned(function->GetNamespace()))
            namespaces[function->GetNamespace()].push_back(
                std::string(function->GetDeclaration(false, false, true)) + ";\n");
    }
    for (asUINT i = 0; i < engine->GetGlobalPropertyCount(); ++i) {
        const char *name = nullptr, *ns = nullptr;
        int type = 0;
        bool constant = false;
        engine->GetGlobalPropertyByIndex(i, &name, &ns, &type, &constant);
        if (owned(ns))
            namespaces[ns].push_back(std::string(constant ? "const " : "") +
                                     engine->GetTypeDeclaration(type, true) + " " + name + ";\n");
    }
    std::ostringstream output;
    output << "// Reflected from BML+ runtime registrations. Editor reference; do not include in "
              "Mods.\n\n";
    for (auto &[ns, declarations] : namespaces) {
        std::size_t start = 0, count = 0;
        while (start < ns.size()) {
            auto end = ns.find("::", start);
            output << "namespace " << ns.substr(start, end - start) << " {\n";
            ++count;
            if (end == std::string::npos)
                break;
            start = end + 2;
        }
        for (const auto &declaration : declarations)
            output << declaration << '\n';
        for (std::size_t n = 0; n < count; ++n)
            output << "}\n";
        output << '\n';
    }
    return output.str();
}
#endif
