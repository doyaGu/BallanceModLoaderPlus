#ifndef BML_INTEROPSIGNATURE_H
#define BML_INTEROPSIGNATURE_H

#include <string>
#include <vector>

namespace BML {

enum class InteropValueType {
    Unsupported,
    Void,
    Bool,
    Int,
    Float,
    String,
    BoolArray,
    IntArray,
    FloatArray,
    StringArray,
    Buffer,
    ObjectId,
};

struct InteropTypeDescriptor {
    InteropValueType Type = InteropValueType::Unsupported;
    InteropValueType ElementType = InteropValueType::Unsupported;
    std::string CanonicalName;

    bool IsSupported() const { return Type != InteropValueType::Unsupported; }
    bool IsArrayLike() const {
        return Type == InteropValueType::BoolArray ||
               Type == InteropValueType::IntArray ||
               Type == InteropValueType::FloatArray ||
               Type == InteropValueType::StringArray ||
               Type == InteropValueType::Buffer;
    }

    bool operator==(const InteropTypeDescriptor &other) const {
        return Type == other.Type && ElementType == other.ElementType;
    }
    bool operator!=(const InteropTypeDescriptor &other) const {
        return !(*this == other);
    }
};

struct InteropSignatureInfo {
    InteropValueType ReturnType = InteropValueType::Unsupported;
    InteropTypeDescriptor ReturnDescriptor;
    std::string Name;
    std::vector<InteropValueType> ParameterTypes;
    std::vector<InteropTypeDescriptor> ParameterDescriptors;
    std::string CanonicalSignature;
};

class InteropSignature {
public:
    static std::string NormalizeType(std::string type);
    static std::string ExtractReturnType(const std::string &signature);
    static std::string ExtractFunctionName(const std::string &signature);
    static std::vector<std::string> ExtractParameterTypes(const std::string &signature);
    static InteropValueType TypeFromName(const std::string &type);
    static InteropTypeDescriptor DescriptorFromName(const std::string &type);
    static InteropSignatureInfo Compile(const std::string &signature);
    static bool IsSupported(const InteropSignatureInfo &info);
    static bool Equivalent(const InteropSignatureInfo &left, const InteropSignatureInfo &right);
    static bool EquivalentSignatures(const std::string &left,
                                     const std::string &right,
                                     const std::string &expectedName = std::string());
    static int Validate(const std::string &signature,
                        const std::string &expectedName,
                        InteropSignatureInfo *outInfo = nullptr);
};

} // namespace BML

#endif
