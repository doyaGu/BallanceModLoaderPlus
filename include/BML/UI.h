#ifndef BML_UI_H
#define BML_UI_H
#include "BML/Generated/bml_ui_imc.hpp"
#include <string>
namespace BML::UI {
struct HUDState { int Mode = 0; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Ui;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }
[[nodiscard]] inline int EmptyCommand(int (Api::Client::*command)(std::uint32_t)) { int status = RequireApi(); return status == BML_OK ? (Client().*command)(5000u) : status; }
[[nodiscard]] inline int VisibleCommand(int (Api::Client::*command)(const Api::VisibleInputValue &, std::uint32_t), bool visible) { Api::VisibleInputValue input{}; input.Visible = visible; int status = RequireApi(); return status == BML_OK ? (Client().*command)(input, 5000u) : status; }
}
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }
[[nodiscard]] inline int ReadHUDState(HUDState &out) { Detail::Api::HudStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallState(wire); if (status == BML_OK) out.Mode = wire.Mode; return status; }
[[nodiscard]] inline int AddMessage(const std::string &message) { Detail::Api::MessageInputValue input{}; input.Message = message; int status = RequireApi(); return status == BML_OK ? Detail::Client().CallMessageAdd(input) : status; }
[[nodiscard]] inline int ClearMessages() { return Detail::EmptyCommand(&Detail::Api::Client::CallMessageClear); }
[[nodiscard]] inline int OpenModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuOpen); }
[[nodiscard]] inline int CloseModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuClose); }
[[nodiscard]] inline int OpenMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuOpen); }
[[nodiscard]] inline int CloseMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuClose); }
[[nodiscard]] inline int SetHUDMode(int mode) { Detail::Api::HudModeInputValue input{}; input.Mode = mode; int status = RequireApi(); return status == BML_OK ? Detail::Client().CallHudSet(input) : status; }
[[nodiscard]] inline int ShowTitle(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CallHudTitleShow, visible); }
[[nodiscard]] inline int ShowFPS(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CallHudFpsShow, visible); }
} // namespace BML::UI
#endif // BML_UI_H
