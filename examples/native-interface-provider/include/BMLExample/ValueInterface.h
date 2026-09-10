#ifndef BML_EXAMPLE_VALUE_INTERFACE_H
#define BML_EXAMPLE_VALUE_INTERFACE_H

#include <BML/Interface.h>

#define BML_EXAMPLE_VALUE_PROVIDER_ID "BMLExampleValueProvider"
#define BML_EXAMPLE_VALUE_PROVIDER_VERSION_MAJOR 1
#define BML_EXAMPLE_VALUE_PROVIDER_VERSION_MINOR 0
#define BML_EXAMPLE_VALUE_PROVIDER_VERSION_PATCH 0

#define BML_EXAMPLE_VALUE_INTERFACE_ID "example.value"
#define BML_EXAMPLE_VALUE_INTERFACE_MAJOR 1u
#define BML_EXAMPLE_VALUE_INTERFACE_MINOR 0u

typedef struct BML_ExampleValueInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *ReadValue)(int input, int *outValue);
} BML_ExampleValueInterface;

#ifdef __cplusplus
#include <BML/Interface.hpp>

BML_DECLARE_INTERFACE_TRAITS(BML_ExampleValueTraits, BML_ExampleValueInterface,
                             BML_EXAMPLE_VALUE_INTERFACE_ID, BML_EXAMPLE_VALUE_INTERFACE_MAJOR, ReadValue);
#endif

#endif // BML_EXAMPLE_VALUE_INTERFACE_H
