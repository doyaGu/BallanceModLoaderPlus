// SDL native Gui is module-local; the binary boundary is the C table.
#include "BML/Gui.hpp"
#include <type_traits>
#include <cstddef>
static_assert(std::is_standard_layout_v<BML_GuiElementInfo>);
static_assert(std::is_trivially_copyable_v<BML_GuiElementInfo>);
static_assert(!std::is_copy_constructible_v<BGui::Gui>);
static_assert(!std::is_move_constructible_v<BGui::Gui>);
static_assert(!std::is_copy_constructible_v<BGui::Element>);
static_assert(!std::is_move_constructible_v<BGui::Text>);
static_assert(offsetof(BML_GuiInterface, CreateElement) == sizeof(BML_InterfaceHeader));
static_assert(BML_GUI_INTERFACE_1_0_SIZE <= sizeof(BML_GuiInterface));

static_assert(std::is_same_v<BML::Gui::Panel, BGui::Panel>);
static_assert(std::is_same_v<BML::Gui::Gui, BGui::Gui>);
static_assert(std::is_same_v<BML::Bui::Window, Bui::Window>);
