#ifndef BML_UI_H
#define BML_UI_H

#include "BML/Generated/bml_ui_api.h"
#include "BML/InteropClient.h"

#include <string>

namespace BML::UI {

struct HUDState {
    int Mode = 0;
    float SRTime = 0.0f;
};

inline int RequireApi() {
    return Interop::RequireApi(Interop::Generated::Bml::Ui::Descriptor);
}

inline int ReadHUDState(HUDState &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Ui::ApiId, "state", record);
    HUDState value{};
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Ui::HudStateField::Mode, value.Mode);
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Ui::HudStateField::SrTime, value.SRTime);
    if (status == BML_OK) out = value;
    return status;
}

namespace Detail {
inline int EmptyCommand(const char *endpoint) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Ui::ApiId, Interop::Generated::Bml::Ui::EmptyInput.Id);
    return status == BML_OK ? Interop::Detail::InvokeCommand(Interop::Generated::Bml::Ui::ApiId, endpoint, input) : status;
}

inline int VisibleCommand(const char *endpoint, bool visible) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Ui::ApiId, Interop::Generated::Bml::Ui::VisibleInput.Id);
    if (status == BML_OK) status = input.SetBool(Interop::Generated::Bml::Ui::VisibleInputField::Visible, visible);
    return status == BML_OK ? Interop::Detail::InvokeCommand(Interop::Generated::Bml::Ui::ApiId, endpoint, input) : status;
}
} // namespace Detail

inline int AddMessage(const std::string &message) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Ui::ApiId, Interop::Generated::Bml::Ui::MessageInput.Id);
    if (status == BML_OK) status = input.SetString(Interop::Generated::Bml::Ui::MessageInputField::Message, message);
    return status == BML_OK ? Interop::Detail::InvokeCommand(Interop::Generated::Bml::Ui::ApiId, "message_add", input) : status;
}

inline int ClearMessages() { return Detail::EmptyCommand("message_clear"); }
inline int OpenModsMenu() { return Detail::EmptyCommand("mods_menu_open"); }
inline int CloseModsMenu() { return Detail::EmptyCommand("mods_menu_close"); }
inline int OpenMapMenu() { return Detail::EmptyCommand("map_menu_open"); }
inline int CloseMapMenu() { return Detail::EmptyCommand("map_menu_close"); }

inline int SetHUDMode(int mode) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Ui::ApiId, Interop::Generated::Bml::Ui::HudModeInput.Id);
    if (status == BML_OK) status = input.SetInt(Interop::Generated::Bml::Ui::HudModeInputField::Mode, mode);
    return status == BML_OK ? Interop::Detail::InvokeCommand(Interop::Generated::Bml::Ui::ApiId, "hud_set", input) : status;
}

inline int ShowTitle(bool visible) { return Detail::VisibleCommand("hud_title_show", visible); }
inline int ShowFPS(bool visible) { return Detail::VisibleCommand("hud_fps_show", visible); }
inline int ShowSRTimer(bool visible) { return Detail::VisibleCommand("hud_sr_show", visible); }
inline int StartSRTimer() { return Detail::EmptyCommand("hud_sr_start"); }
inline int PauseSRTimer() { return Detail::EmptyCommand("hud_sr_pause"); }
inline int ResetSRTimer() { return Detail::EmptyCommand("hud_sr_reset"); }

} // namespace BML::UI

#endif // BML_UI_H
