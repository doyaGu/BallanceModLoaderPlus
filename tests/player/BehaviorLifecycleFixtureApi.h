#ifndef BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H
#define BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H

#include <cstdint>

#include "CKTypes.h"

class CKBehavior;

inline const CKGUID BML_LIFECYCLE_FIXTURE_GUID(
    0x6f736d3a, 0x741ec901);

enum class BMLLifecycleFixtureMode : std::uint32_t {
    Normal = 0,
    CloseOnEdited = 1,
    // Runs the close hook from the RESET, DETACH and DELETE callbacks, that
    // is from inside the native teardown the Loader drives for the block.
    CloseOnTeardown = 2,
};

struct BMLLifecycleFixtureEvent {
    std::uint32_t Message = 0;
    std::uint32_t BehaviorId = 0;
    std::int32_t SettingValue = 0;
    std::uint32_t OwnerVisible = 0;
    std::uint32_t ParentVisible = 0;
    std::uint32_t LinkVisible = 0;
    std::uint32_t SourceVisible = 0;
};

struct BMLLifecycleFixtureTrace {
    std::uint32_t Size = sizeof(BMLLifecycleFixtureTrace);
    std::uint32_t Version = 1;
    std::uint32_t EventCount = 0;
    BMLLifecycleFixtureEvent Events[32]{};
    std::uint32_t CreateCount = 0;
    std::uint32_t AttachCount = 0;
    std::uint32_t SettingsEditedCount = 0;
    std::uint32_t EditedCount = 0;
    std::uint32_t ResetCount = 0;
    std::uint32_t DetachCount = 0;
    std::uint32_t DeleteCount = 0;
    std::int32_t SettingsEditedObserved = 0;
    std::int32_t FinalNormalizedValue = 0;
    std::uint32_t CloseHookCalls = 0;
    std::uint32_t CloseHookAccepted = 0;
};

using BMLLifecycleFixtureCloseHook = int (*)(CKBehavior *, void *);
using BMLLifecycleFixtureResetTraceFn = void (*)();
using BMLLifecycleFixtureSetModeFn = void (*)(BMLLifecycleFixtureMode);
using BMLLifecycleFixtureSetCloseHookFn = void (*)(
    BMLLifecycleFixtureCloseHook, void *);
using BMLLifecycleFixtureReadTraceFn = int (*)(
    BMLLifecycleFixtureTrace *);

#endif // BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H
