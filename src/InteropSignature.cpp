#include "InteropSignature.h"

#include <cctype>

#include "BML/Defines.h"
#include "Utils/StringUtils.h"

namespace BML {

static bool IsIdentifierToken(const std::string &value) {
    if (value.empty())
        return false;
    const auto first = static_cast<unsigned char>(value.front());
    if (!std::isalpha(first) && value.front() != '_')
        return false;
    for (char ch : value) {
        const auto current = static_cast<unsigned char>(ch);
        if (!std::isalnum(current) && ch != '_')
            return false;
    }
    return true;
}

static std::string RemoveTrailingParameterName(std::string type) {
    type = utils::TrimStringCopy(type);
    const size_t nameStart = type.find_last_of(" \t");
    if (nameStart == std::string::npos)
        return type;

    const std::string maybeName = utils::TrimStringCopy(type.substr(nameStart + 1));
    if (!IsIdentifierToken(maybeName))
        return type;

    const std::string maybeType = utils::TrimStringCopy(type.substr(0, nameStart));
    return maybeType.empty() ? type : maybeType;
}

std::string InteropSignature::NormalizeType(std::string type) {
    type = utils::TrimStringCopy(type);
    const size_t assignment = type.find('=');
    if (assignment != std::string::npos)
        type = utils::TrimStringCopy(type.substr(0, assignment));
    type = RemoveTrailingParameterName(type);

    std::string compact;
    compact.reserve(type.size());
    for (char ch : type) {
        if (ch != ' ' && ch != '\t')
            compact.push_back(ch);
    }

    if (compact == "void")
        return "void";
    if (compact == "bool")
        return "bool";
    if (compact == "int")
        return "int";
    if (compact == "float")
        return "float";
    if (compact == "string")
        return "string";
    if (compact == "conststring&in")
        return "const string &in";
    if (compact == "string&in")
        return "string &in";
    return type;
}

std::string InteropSignature::ExtractReturnType(const std::string &signature) {
    const size_t paren = signature.find('(');
    if (paren == std::string::npos)
        return {};
    const std::string prefix = utils::TrimStringCopy(signature.substr(0, paren));
    const size_t nameStart = prefix.find_last_of(" \t");
    if (nameStart == std::string::npos)
        return {};
    return utils::TrimStringCopy(prefix.substr(0, nameStart));
}

std::string InteropSignature::ExtractFunctionName(const std::string &signature) {
    const size_t paren = signature.find('(');
    if (paren == std::string::npos)
        return {};
    const std::string prefix = utils::TrimStringCopy(signature.substr(0, paren));
    const size_t nameStart = prefix.find_last_of(" \t");
    if (nameStart == std::string::npos || nameStart + 1 >= prefix.size())
        return {};
    return utils::TrimStringCopy(prefix.substr(nameStart + 1));
}

std::vector<std::string> InteropSignature::ExtractParameterTypes(const std::string &signature) {
    const size_t open = signature.find('(');
    const size_t close = signature.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open + 1)
        return std::vector<std::string>();

    std::vector<std::string> types;
    const std::string parameters = signature.substr(open + 1, close - open - 1);
    size_t start = 0;
    while (start < parameters.size()) {
        const size_t comma = parameters.find(',', start);
        const size_t end = comma == std::string::npos ? parameters.size() : comma;
        const std::string parameter = utils::TrimStringCopy(parameters.substr(start, end - start));
        if (!parameter.empty())
            types.push_back(NormalizeType(parameter));
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return types;
}

InteropValueType InteropSignature::TypeFromName(const std::string &type) {
    const std::string normalized = NormalizeType(type);
    if (normalized == "void")
        return InteropValueType::Void;
    if (normalized == "bool")
        return InteropValueType::Bool;
    if (normalized == "int")
        return InteropValueType::Int;
    if (normalized == "float")
        return InteropValueType::Float;
    if (normalized == "string" || normalized == "const string &in" || normalized == "string &in")
        return InteropValueType::String;
    return InteropValueType::Unsupported;
}

InteropSignatureInfo InteropSignature::Compile(const std::string &signature) {
    InteropSignatureInfo info;
    info.ReturnType = TypeFromName(ExtractReturnType(signature));
    info.Name = ExtractFunctionName(signature);
    const std::vector<std::string> parameterTypes = ExtractParameterTypes(signature);
    info.ParameterTypes.reserve(parameterTypes.size());
    for (const std::string &parameterType : parameterTypes)
        info.ParameterTypes.push_back(TypeFromName(parameterType));
    return info;
}

bool InteropSignature::IsSupported(const InteropSignatureInfo &info) {
    if (info.Name.empty() || info.ReturnType == InteropValueType::Unsupported)
        return false;
    for (const InteropValueType parameterType : info.ParameterTypes) {
        if (parameterType == InteropValueType::Unsupported || parameterType == InteropValueType::Void)
            return false;
    }
    return true;
}

int InteropSignature::Validate(const std::string &signature,
                               const std::string &expectedName,
                               InteropSignatureInfo *outInfo) {
    const size_t open = signature.find('(');
    const size_t close = signature.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        return BML_ERROR_INTEROP_BAD_SIGNATURE;
    }
    for (size_t i = close + 1; i < signature.size(); ++i) {
        if (signature[i] != ' ' && signature[i] != '\t')
            return BML_ERROR_INTEROP_BAD_SIGNATURE;
    }
    const std::string parameters = utils::TrimStringCopy(signature.substr(open + 1, close - open - 1));
    if (!parameters.empty()) {
        size_t start = 0;
        while (start <= parameters.size()) {
            const size_t comma = parameters.find(',', start);
            const size_t end = comma == std::string::npos ? parameters.size() : comma;
            if (utils::TrimStringCopy(parameters.substr(start, end - start)).empty())
                return BML_ERROR_INTEROP_BAD_SIGNATURE;
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }
    }

    InteropSignatureInfo info = Compile(signature);
    if (!IsSupported(info))
        return BML_ERROR_INTEROP_BAD_SIGNATURE;
    if (!expectedName.empty() && info.Name != expectedName)
        return BML_ERROR_INTEROP_SIGNATURE_MISMATCH;
    if (outInfo)
        *outInfo = info;
    return BML_OK;
}

} // namespace BML
