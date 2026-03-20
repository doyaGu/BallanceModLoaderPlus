#ifndef BML_IMGUI_HPP
#define BML_IMGUI_HPP

/**
 * @file bml_imgui.hpp
 * @brief C++ wrapper that redirects ImGui calls through the BML_ImGuiApi vtable.
 *
 * @code
 *   #include "bml_imgui.hpp"   // MUST come before any imgui.h include
 * @endcode
 *
 * Only wraps the public imgui.h API.  Modules that need imgui_internal.h
 * must include it separately and manage GImGui themselves.
 */

#if defined(IMGUI_VERSION) && !defined(BML_IMGUI_CPP_WRAPPER)
#error "bml_imgui.hpp must be included BEFORE imgui.h -- see file doc comment."
#endif

#ifndef BML_IMGUI_CPP_WRAPPER
#define BML_IMGUI_CPP_WRAPPER 1
#endif

#include "imgui.h"
#include "bml_imgui_api.h"

namespace bml::imgui {

inline thread_local const BML_ImGuiApi *g_CurrentApi = nullptr;

inline const BML_ImGuiApi *GetCurrentApi() noexcept {
    return g_CurrentApi;
}

inline void SetCurrentApi(const BML_ImGuiApi *api) noexcept {
    g_CurrentApi = api;
}

class ApiScope {
public:
    explicit ApiScope(const BML_ImGuiApi *api) noexcept
        : m_Previous(g_CurrentApi) {
        if (api) {
            IM_ASSERT(api->struct_size >= sizeof(BML_ImGuiApi) &&
                      "BML_ImGuiApi struct_size mismatch");
            if (api->ValidateCurrentFrameAccess) {
                IM_ASSERT(api->ValidateCurrentFrameAccess() == BML_RESULT_OK &&
                          "ImGui scope created outside active BML_UI frame");
            }
        }
        g_CurrentApi = api;
    }

    ~ApiScope() { g_CurrentApi = m_Previous; }

    ApiScope(const ApiScope &) = delete;
    ApiScope &operator=(const ApiScope &) = delete;
    ApiScope(ApiScope &&) = delete;
    ApiScope &operator=(ApiScope &&) = delete;

private:
    const BML_ImGuiApi *m_Previous;
};

} // namespace bml::imgui

#define BML_DETAIL_CONCAT_IMPL(a, b) a##b
#define BML_DETAIL_CONCAT(a, b) BML_DETAIL_CONCAT_IMPL(a, b)
#define BML_IMGUI_SCOPE_FROM_CONTEXT(ctx) \
    ::bml::imgui::ApiScope BML_DETAIL_CONCAT(_bmlImguiScope_, __LINE__)((ctx) ? (ctx)->imgui : nullptr)

#include "bml_imgui_cpp_defs.inl"

#endif /* BML_IMGUI_HPP */
