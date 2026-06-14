#include "ScriptExportSignature.h"

#include <utility>

#include "Utils/StringUtils.h"

namespace BML {

std::string ScriptExportSignature::NormalizeType(std::string type) {
    return InteropSignature::NormalizeType(std::move(type));
}

std::string ScriptExportSignature::Canonicalize(const std::string &declaration, const std::string &publicName) {
    const std::string returnType = NormalizeType(ExtractReturnType(declaration));
    if (returnType.empty() || publicName.empty())
        return declaration;

    const std::vector<std::string> parameterTypes = ExtractParameterTypes(declaration);
    std::string signature = returnType + " " + publicName + "(";
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        if (i != 0)
            signature += ", ";
        signature += parameterTypes[i];
    }
    signature += ")";
    return signature;
}

std::string ScriptExportSignature::BuildMethodDecl(const ScriptModExportDefinition &exportInfo) {
    if (exportInfo.Method.empty() || exportInfo.Method == exportInfo.Name)
        return exportInfo.Signature;

    const size_t paren = exportInfo.Signature.find('(');
    if (paren == std::string::npos)
        return exportInfo.Signature;

    const std::string prefix = utils::TrimStringCopy(exportInfo.Signature.substr(0, paren));
    const size_t nameStart = prefix.find_last_of(" \t");
    if (nameStart == std::string::npos)
        return exportInfo.Signature;

    return prefix.substr(0, nameStart + 1) + exportInfo.Method + exportInfo.Signature.substr(paren);
}

ScriptExportSignatureInfo ScriptExportSignature::Compile(const std::string &signature) {
    return InteropSignature::Compile(signature);
}

ScriptExportValueType ScriptExportSignature::TypeFromName(const std::string &type) {
    return InteropSignature::TypeFromName(type);
}

std::string ScriptExportSignature::ExtractReturnType(const std::string &signature) {
    return InteropSignature::ExtractReturnType(signature);
}

std::vector<std::string> ScriptExportSignature::ExtractParameterTypes(const std::string &signature) {
    return InteropSignature::ExtractParameterTypes(signature);
}

} // namespace BML
