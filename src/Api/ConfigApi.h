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
    int Declare(ModContext &, IMod &, const BML_ConfigPropertyDefinition *,
                BML_ConfigPropertyHandle *);
    int Find(ModContext &, IMod &, const char *, const char *, BML_ConfigPropertyHandle *);
    int Read(ModContext &, IMod &, BML_ConfigPropertyHandle, BML_ConfigPropertyVisitor, void *);
    int Set(ModContext &, IMod &, BML_ConfigPropertyHandle, const BML_ConfigValue *,
            bool isDefault);
    int Comment(ModContext &, IMod &, BML_ConfigPropertyHandle, const char *);
    int Editor(ModContext &, IMod &, BML_ConfigPropertyHandle, uint32_t);
    int Choices(ModContext &, IMod &, BML_ConfigPropertyHandle, const char *const *, size_t);
    BML_ConfigPropertyHandle HandleFor(IMod &, Property *) const noexcept;
    void CleanupOwner(IMod &) noexcept;

  private:
    struct Record {
        IMod *Owner = nullptr;
        Property *Target = nullptr;
        std::string Category, Key;
        uint64_t TargetGeneration = 0;
        uint32_t Type = BML_CONFIG_TYPE_NONE, Editor = 0;
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
    Record *Resolve(ModContext &, IMod &, BML_ConfigPropertyHandle);
    std::unordered_map<BML_ConfigPropertyHandle, std::shared_ptr<Record>> m_Records;
};
const BML_ConfigInterface &ConfigInterface();
} // namespace BML::Api
#endif
