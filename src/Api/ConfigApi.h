#ifndef BML_API_CONFIGAPI_H
#define BML_API_CONFIGAPI_H

#include "BML/Config.h"
#include "Config/ConfigValue.h"
#include <memory>
#include <vector>
#include <unordered_map>

class ModContext;
class IMod;
class Config;
class Property;

namespace BML::Api {
class ConfigApi {
public:
    int Declare(ModContext &context, IMod &owner, const BML_ConfigPropertyDefinition *definition,
                BML_ConfigPropertyHandle *outProperty);
    int Find(ModContext &context, IMod &owner, const char *category, const char *key,
             BML_ConfigPropertyHandle *outProperty);
    int Read(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property,
             BML_ConfigPropertyVisitor visitor, void *userData);
    int Set(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property,
            const BML_ConfigValue *value, bool isDefault);
    int SetComment(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property,
                   const char *comment);
    int SetEditor(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property,
                  uint32_t editor);
    int SetChoices(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property,
                   const char *const *choices, size_t count);
    BML_ConfigPropertyHandle HandleFor(IMod &owner, Property *property) const noexcept;
    void CleanupOwner(IMod &owner) noexcept;

private:
    struct Record {
        IMod *Owner = nullptr;
        Property *Target = nullptr;
        std::string Category;
        std::string Key;
        uint64_t TargetGeneration = 0;
        uint32_t Type = BML_CONFIG_TYPE_NONE;
        uint32_t Editor = 0;
        std::string Comment;
        std::vector<std::string> ChoiceValues;
        ConfigValue Default = 0;
        std::vector<const char *> Choices;

        void BindChoices() {
            Choices.resize(ChoiceValues.size());
            for (size_t i = 0; i < Choices.size(); ++i)
                Choices[i] = ChoiceValues[i].c_str();
        }
    };

    Record *Resolve(ModContext &context, IMod &owner, BML_ConfigPropertyHandle property);
    std::unordered_map<BML_ConfigPropertyHandle, std::shared_ptr<Record>> m_Records;
};

const BML_ConfigInterface &ConfigInterface();
} // namespace BML::Api
#endif
