#ifndef BML_SCRIPTEXPORTDISPATCHER_H
#define BML_SCRIPTEXPORTDISPATCHER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "BML/Interop.h"
#include "ScriptDiagnostic.h"
#include "ScriptExportSignature.h"
#include "ScriptModDefinition.h"
#include "ScriptModRuntime.h"

namespace BML {

struct ScriptExportBinding {
    std::string Name;
    std::string Signature;
    std::string MethodDecl;
    ScriptExportSignatureInfo SignatureInfo;
    CKAngelScriptMethod *Method = nullptr;
};

class ScriptExportTable {
public:
    bool Cache(CKContext *context,
               ScriptModRuntime &runtime,
               const std::vector<ScriptModExportDefinition> &definitions,
               ScriptDiagnostic &diagnostic);
    bool Release(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic *diagnostic = nullptr);
    bool HasExport(const std::string &name, const std::string &signature = std::string()) const;
    const ScriptExportBinding *Resolve(const std::string &name, const std::string &signature = std::string()) const;
    std::string GetSignature(const std::string &name) const;
    void GetSignatures(const std::string &name, std::vector<std::string> &out) const;
    int GetCount() const;
    bool GetInfo(int index, std::string &name, std::string &signature) const;
    void Clear();

private:
    void RebuildIndex();

    std::vector<ScriptExportBinding> m_Exports;
    std::unordered_map<std::string, std::vector<size_t>> m_ExportsByName;
    std::unordered_map<std::string, size_t> m_ExportsByCanonicalSignature;
};

class ScriptExportDispatcher {
public:
    static int CallVoid(CKContext *context,
                        ScriptModRuntime &runtime,
                        const ScriptExportBinding &binding,
                        ScriptDiagnostic &diagnostic);
    static int CallString(CKContext *context,
                          ScriptModRuntime &runtime,
                          const ScriptExportBinding &binding,
                          const std::string &argument,
                          bool hasArgument,
                          std::string &out,
                          ScriptDiagnostic &diagnostic);
    static int CallFrame(CKContext *context,
                         ScriptModRuntime &runtime,
                         const ScriptExportBinding &binding,
                         BML_CallFrame *frame,
                         ScriptDiagnostic &diagnostic);
};

} // namespace BML

#endif
