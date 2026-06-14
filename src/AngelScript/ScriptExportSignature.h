#ifndef BML_SCRIPTEXPORTSIGNATURE_H
#define BML_SCRIPTEXPORTSIGNATURE_H

#include <string>
#include <vector>

#include "InteropSignature.h"
#include "ScriptModDefinition.h"

namespace BML {

using ScriptExportValueType = InteropValueType;
using ScriptExportSignatureInfo = InteropSignatureInfo;

class ScriptExportSignature {
public:
    static std::string Canonicalize(const std::string &declaration, const std::string &publicName);
    static std::string BuildMethodDecl(const ScriptModExportDefinition &exportInfo);
    static ScriptExportSignatureInfo Compile(const std::string &signature);
    static ScriptExportValueType TypeFromName(const std::string &type);
    static std::vector<std::string> ExtractParameterTypes(const std::string &signature);
    static std::string ExtractReturnType(const std::string &signature);
    static std::string NormalizeType(std::string type);
};

} // namespace BML

#endif
