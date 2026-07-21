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
inline int EmptyCommand(int (Api::Client::*command)(const Api::EmptyInputValue &, Api::CommandResultValue &, std::uint32_t)) { Api::EmptyInputValue input{}; Api::CommandResultValue output{}; int status = RequireApi(); return status == BML_OK ? (Client().*command)(input, output, 5000u) : status; }
inline int VisibleCommand(int (Api::Client::*command)(const Api::VisibleInputValue &, Api::CommandResultValue &, std::uint32_t), bool visible) { Api::VisibleInputValue input{}; input.Visible = visible; Api::CommandResultValue output{}; int status = RequireApi(); return status == BML_OK ? (Client().*command)(input, output, 5000u) : status; }
}
inline int RequireApi() { return Detail::RequireApi(); }
inline int ReadHUDState(HUDState &out) { Detail::Api::HudStateValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().ReadState(wire); if (status == BML_OK) out = {wire.Mode, wire.SrTime}; return status; }
inline int AddMessage(const std::string &message) { Detail::Api::MessageInputValue input{}; input.Message = message; Detail::Api::CommandResultValue output{}; int status = RequireApi(); return status == BML_OK ? Detail::Client().CommandMessageAdd(input, output) : status; }
inline int ClearMessages() { return Detail::EmptyCommand(&Detail::Api::Client::CommandMessageClear); }
inline int OpenModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CommandModsMenuOpen); }
inline int CloseModsMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CommandModsMenuClose); }
inline int OpenMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CommandMapMenuOpen); }
inline int CloseMapMenu() { return Detail::EmptyCommand(&Detail::Api::Client::CommandMapMenuClose); }
inline int SetHUDMode(int mode) { Detail::Api::HudModeInputValue input{}; input.Mode = mode; Detail::Api::CommandResultValue output{}; int status = RequireApi(); return status == BML_OK ? Detail::Client().CommandHudSet(input, output) : status; }
inline int ShowTitle(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CommandHudTitleShow, visible); }
inline int ShowFPS(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CommandHudFpsShow, visible); }
inline int ShowSRTimer(bool visible) { return Detail::VisibleCommand(&Detail::Api::Client::CommandHudSrShow, visible); }
inline int StartSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CommandHudSrStart); }
inline int PauseSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CommandHudSrPause); }
inline int ResetSRTimer() { return Detail::EmptyCommand(&Detail::Api::Client::CommandHudSrReset); }
} // namespace BML::UI
#endif // BML_UI_H
