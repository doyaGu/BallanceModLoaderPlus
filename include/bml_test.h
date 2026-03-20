/**
 * @file bml_test.h
 * @brief BML Test Support - C API for mod unit testing without running the game
 *
 * Provides a minimal BML test context that bootstraps Core APIs (config,
 * logging, IMC, timer) without requiring CK2/Virtools. Use in test fixture
 * SetUp/TearDown to exercise mod logic in isolation.
 *
 * Link against `bml_test` (static library) which links BML.dll.
 */

#ifndef BML_TEST_H
#define BML_TEST_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_logging.h"
#include "bml_imc.h"

BML_BEGIN_CDECLS

/**
 * @brief Initialize a minimal BML test context.
 *
 * Bootstraps Core APIs (config, logging, IMC, timer) but no CK2/Virtools.
 * Call once per test fixture SetUp. Paired with bmlTestShutdown().
 *
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TestInit)(void);

/**
 * @brief Shutdown the test context.
 *
 * Clears log sink override, shuts down the microkernel, and resets state.
 * Call in TearDown.
 */
typedef void (*PFN_BML_TestShutdown)(void);

/**
 * @brief Create a mock module handle for testing.
 *
 * Creates a BML_Mod handle backed by a synthetic manifest with the given ID.
 * The handle is valid until bmlTestShutdown() is called.
 *
 * @param[in]  id      Module ID string (e.g. "com.test.mymod")
 * @param[out] out_mod Receives the mock module handle
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if id or out_mod is NULL
 */
typedef BML_Result (*PFN_BML_TestCreateMockMod)(const char *id, BML_Mod *out_mod);

/**
 * @brief Advance the test clock: pump IMC, tick timers.
 *
 * Processes all pending IMC messages and advances the timer system.
 */
typedef void (*PFN_BML_TestTick)(void);

/**
 * @brief Publish to a topic by name (convenience for tests).
 *
 * Resolves the topic name to an ID, then publishes the data.
 *
 * @param[in] topic_name Topic name string
 * @param[in] data       Payload data (may be NULL if size is 0)
 * @param[in] size       Payload size in bytes
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_TestPublish)(const char *topic_name,
                                          const void *data, size_t size);

BML_END_CDECLS

#endif /* BML_TEST_H */
