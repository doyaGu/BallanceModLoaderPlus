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
};

struct InteropSignatureInfo {
    InteropValueType ReturnType = InteropValueType::Unsupported;
    std::string Name;
    std::vector<InteropValueType> ParameterTypes;
};

class InteropSignature {
public:
    static std::string NormalizeType(std::string type);
    static std::string ExtractReturnType(const std::string &signature);
    static std::string ExtractFunctionName(const std::string &signature);
    static std::vector<std::string> ExtractParameterTypes(const std::string &signature);
    static InteropValueType TypeFromName(const std::string &type);
    static InteropSignatureInfo Compile(const std::string &signature);
    static bool IsSupported(const InteropSignatureInfo &info);
    static int Validate(const std::string &signature,
                        const std::string &expectedName,
                        InteropSignatureInfo *outInfo = nullptr);
};

} // namespace BML

#endif
