/**
 * @file RenderHook.h
 * @brief Public API for BML_Render hook subsystem
 *
 * Provides hooks for CK2_3D.dll render context to enable:
 * - Render skip functionality
 * - Widescreen FOV fix via UpdateProjection hook
 */

#ifndef BML_RENDER_RENDERHOOK_H
#define BML_RENDER_RENDERHOOK_H

namespace bml { class ModuleServices; }

namespace BML_Render {

struct HookAddresses {
    void *render = nullptr;
    void *updateProjection = nullptr;
};

bool InitRenderHook(const bml::ModuleServices &services);
void ShutdownRenderHook();
void DisableRender(bool disable);
void EnableWidescreenFix(bool enable);
bool IsRenderHookActive();
HookAddresses GetHookAddresses();

} // namespace BML_Render

#endif // BML_RENDER_RENDERHOOK_H
