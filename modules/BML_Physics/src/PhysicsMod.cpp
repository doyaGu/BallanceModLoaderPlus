/**
 * @file PhysicsMod.cpp
 * @brief BML Physics Module - hooks into Virtools IpionManager and Physicalize behaviors.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "PhysicsHook.h"

class PhysicsMod : public bml::HookModule {
    const char *HookLogCategory() const override { return "BML_Physics"; }

    bool InitHook(CKContext *ctx, const BML_HookContext *hctx) override {
        return BML_Physics::InitializePhysicsHook(ctx, hctx);
    }

    void ShutdownHook() override {
        BML_Physics::ShutdownPhysicsHook();
    }
};

BML_DEFINE_MODULE(PhysicsMod)
