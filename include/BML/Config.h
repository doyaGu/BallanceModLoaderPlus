#ifndef BML_CONFIG_H
#define BML_CONFIG_H

#include "BML/Mod.h"

BML_BEGIN_CDECLS
#define BML_CONFIG_VALUE_1_0_SIZE                                                                  \
    (offsetof(BML_ConfigValue, Data) + sizeof(((BML_ConfigValue *)0)->Data))
#define BML_CONFIG_VALUE_INIT {.StructSize = sizeof(BML_ConfigValue)}
#define BML_CONFIG_PROPERTY_DEFINITION_1_0_SIZE                                                    \
    (offsetof(BML_ConfigPropertyDefinition, ChoiceCount) +                                         \
     sizeof(((BML_ConfigPropertyDefinition *)0)->ChoiceCount))
#define BML_CONFIG_PROPERTY_DEFINITION_INIT {.StructSize = sizeof(BML_ConfigPropertyDefinition)}
#define BML_CONFIG_PROPERTY_INFO_1_0_SIZE                                                          \
    (offsetof(BML_ConfigPropertyInfo, Value) + sizeof(((BML_ConfigPropertyInfo *)0)->Value))
#define BML_CONFIG_PROPERTY_INFO_INIT {.StructSize = sizeof(BML_ConfigPropertyInfo)}
#define BML_CONFIG_INTERFACE_1_0_SIZE                                                              \
    (offsetof(BML_ConfigInterface, HasKey) + sizeof(((BML_ConfigInterface *)0)->HasKey))
#pragma pack(push, 8)
#define BML_CONFIG_INTERFACE_ID "bml.config"
#define BML_CONFIG_INTERFACE_MAJOR 1u
#define BML_CONFIG_TYPE_STRING 0u
#define BML_CONFIG_TYPE_BOOLEAN 1u
#define BML_CONFIG_TYPE_INTEGER 2u
#define BML_CONFIG_TYPE_KEY 3u
#define BML_CONFIG_TYPE_FLOAT 4u
#define BML_CONFIG_TYPE_NONE 5u
#ifndef BML_CONFIG_EDITOR_DEFINED
#define BML_CONFIG_EDITOR_DEFINED

typedef enum BML_ConfigPropertyEditor {
    BML_CONFIG_EDITOR_DEFAULT = 0,
    BML_CONFIG_EDITOR_COLOR = 1,
    BML_CONFIG_EDITOR_CHOICE = 2
} BML_ConfigPropertyEditor;
#endif
typedef struct BML_ConfigValue {
    size_t StructSize;
    uint32_t Type;

    union {
        const char *String;
        uint32_t Boolean;
        int32_t Integer;
        float Float;
        uint32_t Key;
    } Data;
} BML_ConfigValue;

typedef struct BML_ConfigPropertyDefinition {
    size_t StructSize;
    const char *Category;
    const char *Key;
    const BML_ConfigValue *DefaultValue;
    const char *Comment;
    uint32_t Editor;
    const char *const *Choices;
    size_t ChoiceCount;
} BML_ConfigPropertyDefinition;

typedef struct BML_ConfigPropertyInfo {
    size_t StructSize;
    BML_ConfigPropertyHandle Handle;
    const BML_ConfigPropertyDefinition *Definition;
    const BML_ConfigValue *Value;
} BML_ConfigPropertyInfo;

typedef int(BML_CDECL *BML_ConfigPropertyVisitor)(void *userData,
                                                  const BML_ConfigPropertyInfo *info);

typedef struct BML_ConfigInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *DeclareProperty)(BML_ModContextHandle context,
                                    const BML_ConfigPropertyDefinition *definition,
                                    BML_ConfigPropertyHandle *outProperty);
    int(BML_CDECL *FindProperty)(BML_ModContextHandle context, const char *category,
                                 const char *key, BML_ConfigPropertyHandle *outProperty);
    int(BML_CDECL *ReadProperty)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                                 BML_ConfigPropertyVisitor visitor, void *userData);
    int(BML_CDECL *SetValue)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                             const BML_ConfigValue *value);
    int(BML_CDECL *SetDefaultValue)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                                    const BML_ConfigValue *value);
    int(BML_CDECL *SetComment)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                               const char *comment);
    int(BML_CDECL *SetEditor)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                              uint32_t editor);
    int(BML_CDECL *SetChoices)(BML_ModContextHandle context, BML_ConfigPropertyHandle property,
                               const char *const *choices, size_t count);
    int(BML_CDECL *SetCategoryComment)(BML_ModContextHandle context, const char *category,
                                       const char *comment);
    int(BML_CDECL *HasCategory)(BML_ModContextHandle context, const char *category,
                                uint32_t *outExists);
    int(BML_CDECL *HasKey)(BML_ModContextHandle context, const char *category, const char *key,
                           uint32_t *outExists);
} BML_ConfigInterface;

#pragma pack(pop)
BML_END_CDECLS

#endif
