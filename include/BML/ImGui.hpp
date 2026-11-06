#ifndef BML_IMGUI_HPP
#define BML_IMGUI_HPP
// Opt-in host ImGui ABI. Link BML::ImGui; never compile a second ImGui runtime.
#include "BML/ImGui.h"
#include "BML/Mod.hpp"
#include <imgui.h>
namespace BML::ImGui {
namespace Detail {
inline const BML_ImGuiInterface *Api() noexcept {
    return BML::Detail::Bind<BML_ImGuiInterface>(
        BML_IMGUI_INTERFACE_ID, 1, BML_IMGUI_INTERFACE_1_0_SIZE,
        +[](const BML_ImGuiInterface *table) {
            return BML_IFACE_HAS(table, BML_ImGuiInterface, BorrowContext) &&
                   BML_IFACE_HAS(table, BML_ImGuiInterface, AddButtonImage);
        });
}
} // namespace Detail
[[nodiscard]] inline Result<ImGuiContext *> BorrowContext(const Mod::Context &ctx) try {
    void *value = nullptr;
    const auto *api = Detail::Api();
    const int code = api ? api->BorrowContext(ctx.Handle(), &value) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<ImGuiContext *>::Success(static_cast<ImGuiContext *>(value))
                          : Result<ImGuiContext *>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<ImGuiContext *>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<ImGuiContext *>::Failure(BML_ERROR_FAIL);
}
inline int AddButtonImage(const Mod::Context &ctx, ImDrawList *list, BML_Vec2 position,
                          uint32_t kind, int32_t state, std::string_view text,
                          BML_Vec2 alignment) try {
    if (!BML::Detail::IsText(text))
        return BML_ERROR_INVALID_PARAMETER;
    const std::string label(text);
    const auto *api = Detail::Api();
    return api ? api->AddButtonImage(ctx.Handle(), list, position, kind, state, label.c_str(),
                                     alignment)
               : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
} // namespace BML::ImGui
#endif
