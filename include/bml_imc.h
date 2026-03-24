#ifndef BML_IMC_H
#define BML_IMC_H

/**
 * @file bml_imc.h
 * @brief Inter-Module Communication (IMC) umbrella header
 *
 * Includes all IMC subsystem headers: shared types, pub/sub bus, and RPC.
 * For finer-grained includes, use bml_imc_types.h, bml_imc_bus.h, or
 * bml_imc_rpc.h directly.
 */

#include "bml_imc_types.h"
#include "bml_imc_bus.h"
#include "bml_imc_rpc.h"

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

/* Verify struct_size is at offset 0 for all versioned structs */
static_assert(offsetof(BML_ImcMessage, struct_size) == 0,
    "BML_ImcMessage.struct_size must be at offset 0");
static_assert(offsetof(BML_ImcBuffer, struct_size) == 0,
    "BML_ImcBuffer.struct_size must be at offset 0");
static_assert(offsetof(BML_RpcInfo, struct_size) == 0,
    "BML_RpcInfo.struct_size must be at offset 0");
static_assert(offsetof(BML_RpcCallOptions, struct_size) == 0,
    "BML_RpcCallOptions.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
static_assert(sizeof(BML_FutureState) == sizeof(int32_t),
    "BML_FutureState must be 32-bit");
static_assert(sizeof(BML_EventResult) == sizeof(int32_t),
    "BML_EventResult must be 32-bit");

#endif /* BML_IMC_H */
