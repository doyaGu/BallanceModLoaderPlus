#ifndef BML_TESTS_PLAYER_INTERFACE_PROVIDER_TEST_API_H
#define BML_TESTS_PLAYER_INTERFACE_PROVIDER_TEST_API_H

#include <BML/Interface.h>

#define BML_PLAYER_PROVIDER_INTERFACE_ID "test.player.provider"
#define BML_PLAYER_PROVIDER_CLEANUP_INTERFACE_ID "test.player.provider.cleanup"
#define BML_PLAYER_PROVIDER_EXPLICIT_INTERFACE_ID "test.player.provider.explicit"
#define BML_PLAYER_PROVIDER_INTERFACE_MAJOR 1u
#define BML_PLAYER_PROVIDER_INTERFACE_MINOR 0u

typedef struct BML_PlayerProviderInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *ReadValue)(int input, int *outValue);
} BML_PlayerProviderInterface;

#ifdef __cplusplus
#include <BML/Interface.hpp>

BML_DECLARE_INTERFACE_TRAITS(BML_PlayerProviderTraits, BML_PlayerProviderInterface,
                             BML_PLAYER_PROVIDER_INTERFACE_ID, BML_PLAYER_PROVIDER_INTERFACE_MAJOR, ReadValue);
BML_DECLARE_INTERFACE_TRAITS(BML_PlayerProviderCleanupTraits, BML_PlayerProviderInterface,
                             BML_PLAYER_PROVIDER_CLEANUP_INTERFACE_ID, BML_PLAYER_PROVIDER_INTERFACE_MAJOR,
                             ReadValue);
BML_DECLARE_INTERFACE_TRAITS(BML_PlayerProviderExplicitTraits, BML_PlayerProviderInterface,
                             BML_PLAYER_PROVIDER_EXPLICIT_INTERFACE_ID, BML_PLAYER_PROVIDER_INTERFACE_MAJOR,
                             ReadValue);
#endif

#endif // BML_TESTS_PLAYER_INTERFACE_PROVIDER_TEST_API_H
