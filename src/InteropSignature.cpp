#include "InteropSignature.h"

#include <cctype>
#include <mutex>
#include <unordered_map>

#include "BML/Defines.h"
#include "Utils/StringUtils.h"

namespace BML {

static std::mutex g_SignatureCacheMutex;
static std::unordered_map<std::string, InteropSignatureInfo> g_SignatureCache;

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

static std::string PrepareType(std::string type) {
    type = utils::TrimStringCopy(type);
    const size_t assignment = type.find('=');
    if (assignment != std::string::npos)
        type = utils::TrimStringCopy(type.substr(0, assignment));
    return RemoveTrailingParameterName(type);
}

static std::string RemoveAsciiWhitespace(const std::string &value) {
    std::string compact;
    compact.reserve(value.size());
    for (char ch : value) {
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
            compact.push_back(ch);
    }
    return compact;
}

static bool StartsWith(const std::string &value, const char *prefix) {
    const std::string prefixValue(prefix);
    return value.size() >= prefixValue.size() &&
           value.compare(0, prefixValue.size(), prefixValue) == 0;
}

static bool EndsWith(const std::string &value, const char *suffix) {
    const std::string suffixValue(suffix);
    return value.size() >= suffixValue.size() &&
           value.compare(value.size() - suffixValue.size(), suffixValue.size(), suffixValue) == 0;
}

static bool ExtractWrapped(const std::string &value,
                           const char *prefix,
                           const char *suffix,
                           std::string &inner) {
    if (!StartsWith(value, prefix) || !EndsWith(value, suffix))
        return false;
    const size_t prefixSize = std::string(prefix).size();
    const size_t suffixSize = std::string(suffix).size();
    if (value.size() < prefixSize + suffixSize)
        return false;
    inner = value.substr(prefixSize, value.size() - prefixSize - suffixSize);
    return !inner.empty();
}

static bool ExtractArrayReturnType(const std::string &compact, std::string &inner) {
    if (ExtractWrapped(compact, "array<", ">@", inner))
        return true;
    if (EndsWith(compact, "[]@")) {
        inner = compact.substr(0, compact.size() - 3);
        return !inner.empty();
    }
    return false;
}

static bool ExtractArrayParameterType(const std::string &compact, std::string &inner) {
    if (ExtractWrapped(compact, "constarray<", ">&in", inner))
        return true;
    if (StartsWith(compact, "const") && EndsWith(compact, "[]&in")) {
        inner = compact.substr(5, compact.size() - 5 - 5);
        return !inner.empty();
    }
    return false;
}

static bool SplitParametersNestedAware(const std::string &parameters,
                                       std::vector<std::string> &outTypes) {
    outTypes.clear();
    if (utils::TrimStringCopy(parameters).empty())
        return true;

    int genericDepth = 0;
    size_t start = 0;
    for (size_t i = 0; i <= parameters.size(); ++i) {
        const bool atEnd = i == parameters.size();
        const char ch = atEnd ? ',' : parameters[i];

        if (!atEnd) {
            if (ch == '<') {
                ++genericDepth;
            } else if (ch == '>') {
                --genericDepth;
                if (genericDepth < 0)
                    return false;
            }
        }

        if ((atEnd || ch == ',') && genericDepth == 0) {
            std::string parameter = utils::TrimStringCopy(parameters.substr(start, i - start));
            if (parameter.empty())
                return false;
            outTypes.push_back(InteropSignature::NormalizeType(parameter));
            start = i + 1;
        }
    }

    return genericDepth == 0;
}

static InteropTypeDescriptor MakeDescriptor(InteropValueType type,
                                            const std::string &canonicalName,
                                            InteropValueType elementType = InteropValueType::Unsupported) {
    InteropTypeDescriptor descriptor;
    descriptor.Type = type;
    descriptor.ElementType = elementType;
    descriptor.CanonicalName = canonicalName;
    return descriptor;
}

struct InteropTypeFacts {
    InteropValueType Type;
    BML_CALL_VALUE_TYPE CallValueType;
    const char *ScriptArrayDecl;
    bool V2;
};

static const InteropTypeFacts kTypeFacts[] = {
    {InteropValueType::Bool, BML_CALL_VALUE_BOOL, nullptr, false},
    {InteropValueType::Int, BML_CALL_VALUE_INT, nullptr, false},
    {InteropValueType::Float, BML_CALL_VALUE_FLOAT, nullptr, false},
    {InteropValueType::String, BML_CALL_VALUE_STRING, nullptr, false},
    {InteropValueType::BoolArray, BML_CALL_VALUE_BOOL_ARRAY, "array<bool>", true},
    {InteropValueType::IntArray, BML_CALL_VALUE_INT_ARRAY, "array<int>", true},
    {InteropValueType::FloatArray, BML_CALL_VALUE_FLOAT_ARRAY, "array<float>", true},
    {InteropValueType::StringArray, BML_CALL_VALUE_STRING_ARRAY, "array<string>", true},
    {InteropValueType::Buffer, BML_CALL_VALUE_BUFFER, "array<uint8>", true},
    {InteropValueType::ObjectId, BML_CALL_VALUE_OBJECT_ID, nullptr, true},
};

static const InteropTypeFacts *FindTypeFacts(InteropValueType type) {
    for (const InteropTypeFacts &facts : kTypeFacts) {
        if (facts.Type == type)
            return &facts;
    }
    return nullptr;
}

static bool MapArrayElement(const std::string &inner,
                            InteropValueType &arrayType,
                            InteropValueType &elementType,
                            std::string &canonicalElement) {
    if (inner == "bool") {
        arrayType = InteropValueType::BoolArray;
        elementType = InteropValueType::Bool;
        canonicalElement = "bool";
        return true;
    }
    if (inner == "int") {
        arrayType = InteropValueType::IntArray;
        elementType = InteropValueType::Int;
        canonicalElement = "int";
        return true;
    }
    if (inner == "float") {
        arrayType = InteropValueType::FloatArray;
        elementType = InteropValueType::Float;
        canonicalElement = "float";
        return true;
    }
    if (inner == "string") {
        arrayType = InteropValueType::StringArray;
        elementType = InteropValueType::String;
        canonicalElement = "string";
        return true;
    }
    if (inner == "uint8") {
        arrayType = InteropValueType::Buffer;
        elementType = InteropValueType::Int;
        canonicalElement = "uint8";
        return true;
    }
    return false;
}

static InteropTypeDescriptor DescriptorFromPreparedType(const std::string &prepared) {
    const std::string compact = RemoveAsciiWhitespace(prepared);
    if (compact == "void")
        return MakeDescriptor(InteropValueType::Void, "void");
    if (compact == "bool")
        return MakeDescriptor(InteropValueType::Bool, "bool");
    if (compact == "int")
        return MakeDescriptor(InteropValueType::Int, "int");
    if (compact == "float")
        return MakeDescriptor(InteropValueType::Float, "float");
    if (compact == "string" || compact == "conststring&in" || compact == "string&in")
        return MakeDescriptor(InteropValueType::String, "string");
    if (compact == "CKObject@")
        return MakeDescriptor(InteropValueType::ObjectId, "CKObject@");

    std::string inner;
    if (ExtractArrayParameterType(compact, inner) ||
        ExtractArrayReturnType(compact, inner)) {
        InteropValueType arrayType = InteropValueType::Unsupported;
        InteropValueType elementType = InteropValueType::Unsupported;
        std::string canonicalElement;
        if (MapArrayElement(inner, arrayType, elementType, canonicalElement)) {
            return MakeDescriptor(arrayType,
                                  std::string("array<") + canonicalElement + ">",
                                  elementType);
        }
    }

    return InteropTypeDescriptor();
}

static std::string BuildCanonicalSignature(const InteropSignatureInfo &info) {
    if (info.Name.empty() || !info.ReturnDescriptor.IsSupported())
        return {};

    std::string signature = info.ReturnDescriptor.CanonicalName;
    signature += " ";
    signature += info.Name;
    signature += "(";
    for (size_t i = 0; i < info.ParameterDescriptors.size(); ++i) {
        if (i > 0)
            signature += ", ";
        signature += info.ParameterDescriptors[i].CanonicalName;
    }
    signature += ")";
    return signature;
}

std::string InteropSignature::NormalizeType(std::string type) {
    const std::string prepared = PrepareType(std::move(type));
    const std::string compact = RemoveAsciiWhitespace(prepared);

    if (compact == "void" ||
        compact == "bool" ||
        compact == "int" ||
        compact == "float" ||
        compact == "string")
        return compact;
    if (compact == "conststring&in")
        return "const string &in";
    if (compact == "string&in")
        return "string &in";
    if (compact == "CKObject@")
        return "CKObject@";

    std::string inner;
    if (ExtractArrayParameterType(compact, inner) ||
        ExtractArrayReturnType(compact, inner)) {
        InteropValueType arrayType = InteropValueType::Unsupported;
        InteropValueType elementType = InteropValueType::Unsupported;
        std::string canonicalElement;
        if (MapArrayElement(inner, arrayType, elementType, canonicalElement)) {
            if (ExtractArrayParameterType(compact, inner))
                return std::string("const array<") + canonicalElement + "> &in";
            return std::string("array<") + canonicalElement + ">@";
        }
    }

    return prepared;
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
    if (!SplitParametersNestedAware(parameters, types))
        return std::vector<std::string>();
    return types;
}

InteropValueType InteropSignature::TypeFromName(const std::string &type) {
    return DescriptorFromName(type).Type;
}

InteropTypeDescriptor InteropSignature::DescriptorFromName(const std::string &type) {
    return DescriptorFromPreparedType(PrepareType(type));
}

bool InteropSignature::IsArrayLike(InteropValueType type) {
    const InteropTypeFacts *facts = FindTypeFacts(type);
    return facts && facts->ScriptArrayDecl;
}

bool InteropSignature::IsV2Type(InteropValueType type) {
    const InteropTypeFacts *facts = FindTypeFacts(type);
    return facts && facts->V2;
}

const char *InteropSignature::ScriptArrayDecl(InteropValueType type) {
    const InteropTypeFacts *facts = FindTypeFacts(type);
    return facts ? facts->ScriptArrayDecl : nullptr;
}

BML_CALL_VALUE_TYPE InteropSignature::ToCallValueType(InteropValueType type) {
    const InteropTypeFacts *facts = FindTypeFacts(type);
    return facts ? facts->CallValueType : BML_CALL_VALUE_EMPTY;
}

InteropSignatureInfo InteropSignature::Compile(const std::string &signature) {
    {
        std::lock_guard<std::mutex> lock(g_SignatureCacheMutex);
        auto it = g_SignatureCache.find(signature);
        if (it != g_SignatureCache.end())
            return it->second;
    }

    InteropSignatureInfo info;
    info.ReturnDescriptor = DescriptorFromName(ExtractReturnType(signature));
    info.ReturnType = info.ReturnDescriptor.Type;
    info.Name = ExtractFunctionName(signature);

    const std::vector<std::string> parameterTypes = ExtractParameterTypes(signature);
    info.ParameterTypes.reserve(parameterTypes.size());
    info.ParameterDescriptors.reserve(parameterTypes.size());
    for (const std::string &parameterType : parameterTypes) {
        InteropTypeDescriptor descriptor = DescriptorFromName(parameterType);
        info.ParameterDescriptors.push_back(descriptor);
        info.ParameterTypes.push_back(descriptor.Type);
    }

    info.CanonicalSignature = BuildCanonicalSignature(info);
    {
        std::lock_guard<std::mutex> lock(g_SignatureCacheMutex);
        g_SignatureCache.emplace(signature, info);
    }
    return info;
}

bool InteropSignature::IsSupported(const InteropSignatureInfo &info) {
    if (info.Name.empty() || !info.ReturnDescriptor.IsSupported())
        return false;
    if (info.ParameterDescriptors.size() != info.ParameterTypes.size())
        return false;
    for (const InteropTypeDescriptor &parameterType : info.ParameterDescriptors) {
        if (!parameterType.IsSupported() || parameterType.Type == InteropValueType::Void)
            return false;
    }
    return true;
}

bool InteropSignature::Equivalent(const InteropSignatureInfo &left, const InteropSignatureInfo &right) {
    if (left.Name != right.Name)
        return false;
    if (left.ReturnDescriptor != right.ReturnDescriptor)
        return false;
    if (left.ParameterDescriptors.size() != right.ParameterDescriptors.size())
        return false;
    for (size_t i = 0; i < left.ParameterDescriptors.size(); ++i) {
        if (left.ParameterDescriptors[i] != right.ParameterDescriptors[i])
            return false;
    }
    return true;
}

bool InteropSignature::EquivalentSignatures(const std::string &left,
                                            const std::string &right,
                                            const std::string &expectedName) {
    InteropSignatureInfo leftInfo;
    InteropSignatureInfo rightInfo;
    if (Validate(left, expectedName, &leftInfo) != BML_OK)
        return false;
    if (Validate(right, expectedName, &rightInfo) != BML_OK)
        return false;
    return Equivalent(leftInfo, rightInfo);
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

    std::vector<std::string> parameters;
    if (!SplitParametersNestedAware(signature.substr(open + 1, close - open - 1), parameters))
        return BML_ERROR_INTEROP_BAD_SIGNATURE;

    InteropSignatureInfo info = Compile(signature);
    if (!IsSupported(info))
        return BML_ERROR_INTEROP_BAD_SIGNATURE;
    if (info.ParameterDescriptors.size() != parameters.size())
        return BML_ERROR_INTEROP_BAD_SIGNATURE;

    const std::string normalizedReturn = NormalizeType(ExtractReturnType(signature));
    const std::string compactReturn = RemoveAsciiWhitespace(normalizedReturn);
    if (info.ReturnDescriptor.IsArrayLike() && !EndsWith(compactReturn, ">@"))
        return BML_ERROR_INTEROP_BAD_SIGNATURE;

    for (size_t i = 0; i < parameters.size(); ++i) {
        const std::string compactParameter = RemoveAsciiWhitespace(parameters[i]);
        if (info.ParameterDescriptors[i].IsArrayLike() &&
            !ExtractWrapped(compactParameter, "constarray<", ">&in", parameters[i])) {
            return BML_ERROR_INTEROP_BAD_SIGNATURE;
        }
    }

    if (!expectedName.empty() && info.Name != expectedName)
        return BML_ERROR_INTEROP_SIGNATURE_MISMATCH;
    if (outInfo)
        *outInfo = info;
    return BML_OK;
}

} // namespace BML
