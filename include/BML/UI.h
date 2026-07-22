#ifndef BML_UI_H
#define BML_UI_H
#include "BML/Generated/bml_ui_imc.hpp"
#include <string>
namespace BML::UI {
struct HUDState { int Mode = 0; float SRTime = 0.0f; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Ui;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
inline int RequireApi() { return ClientState().EnsureOpen(); }
inline int EmptyCommand(int (Api::Client::*command)(std::uint32_t)) { int status = RequireApi(); return status == BML_OK ? (Client().*command)(5000u) : status; }
inline int VisibleCommand(int (Api::Client::*command)(const Api::VisibleInputValue &, std::uint32_t), bool visible) { Api::VisibleInputValue input{}; input.Visible = visible; int status = RequireApi(); return status == BML_OK ? (Client().*command)(input, 5000u) : status; }
}
inline int RequireApi() { return Detail::RequireApi(); }
inline int ReadHUDState(HUDState &out) { Detail::Api::HudStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallState(wire); if (status == BML_OK) out = {wire.Mode, wire.SrTime}; return status; }
inline int AddMessage(const std::string &message) { Detail::Api::MessageInputValue input{}; input.Message = message; int status = RequireApi(); return status == BML_OK ? Detail::Client().CallMessageAdd(input) : status; }
inline int ClearMessages() { return Detail::EmptyCommand(&Detail::Api::Client::CallMessageClear); }
inline int OpenModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuOpen); }
inline int CloseModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallModsMenuClose); }
inline int OpenMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuOpen); }
inline int CloseMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CallMapMenuClose); }
inline int SetHUDMode(int mode) { Detail::Api::HudModeInputValue input{}; input.Mode = mode; int status = RequireApi(); return status == BML_OK ? Detail::Client().CallHudSet(input) : status; }
inline int ShowTitle(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CallHudTitleShow, visible); }
inline int ShowFPS(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CallHudFpsShow, visible); }
inline int ShowSRTimer(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CallHudSrShow, visible); }
inline int StartSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallHudSrStart); }
inline int PauseSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallHudSrPause); }
inline int ResetSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CallHudSrReset); }
} // namespace BML::UI
#endif // BML_UI_H
