// Generated Interop AngelScript binding. Do not edit by hand.
// Include this API binding from a mod. CreateApi() is for providers;
// the typed facade below is for consumers.

namespace BMLInteropGenerated {
namespace BmlUi {

::BML::Interop::ApiBuilder@ CreateApi() {
  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(
      "bml.ui", 1, 0, uint64(0xD21CC17B2334E884));
  if (api is null)
    return null;
  if (api.AddSchema(1, "command_result") != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(2, "message_input") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 1, "message", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(3, "hud_mode_input") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 1, "mode", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(4, "visible_input") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(4, 1, "visible", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(5, "empty_input") != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(6, "hud_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(6, 1, "mode", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(6, 2, "sr_time", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_fps_show", ::BML::Interop::ENDPOINT_COMMAND, 4, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_set", ::BML::Interop::ENDPOINT_COMMAND, 3, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_sr_pause", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_sr_reset", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_sr_show", ::BML::Interop::ENDPOINT_COMMAND, 4, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_sr_start", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("hud_title_show", ::BML::Interop::ENDPOINT_COMMAND, 4, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("map_menu_close", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("map_menu_open", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("message_add", ::BML::Interop::ENDPOINT_COMMAND, 2, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("message_clear", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("mods_menu_close", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("mods_menu_open", ::BML::Interop::ENDPOINT_COMMAND, 5, 1, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("state", ::BML::Interop::ENDPOINT_RESOURCE, 0, 6, false) != ::BML::ERROR_OK)
    return null;
  return api;
}

// Typed consumer facade.  It is generated from this API and uses
// BML::Interop only as its private transport layer.
const uint Major = 1;
const uint64 Hash = uint64(0xD21CC17B2334E884);

int Require() {
  return ::BML::Interop::RequireApi("bml.ui", Major, Hash);
}

class CommandResultValue {
}

int DecodeCommandResult(::BML::Interop::Record@ record, CommandResultValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 1)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  CommandResultValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class MessageInputValue {
  string Message;
}

int DecodeMessageInput(::BML::Interop::Record@ record, MessageInputValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 2)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  MessageInputValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetString(1, decoded.Message);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class HudModeInputValue {
  int Mode;
}

int DecodeHudModeInput(::BML::Interop::Record@ record, HudModeInputValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 3)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  HudModeInputValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Mode);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class VisibleInputValue {
  bool Visible;
}

int DecodeVisibleInput(::BML::Interop::Record@ record, VisibleInputValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 4)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  VisibleInputValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetBool(1, decoded.Visible);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class EmptyInputValue {
}

int DecodeEmptyInput(::BML::Interop::Record@ record, EmptyInputValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 5)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  EmptyInputValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class HudStateValue {
  int Mode;
  float SrTime;
}

int DecodeHudState(::BML::Interop::Record@ record, HudStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 6)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  HudStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Mode);
  if (status == ::BML::ERROR_OK) status = record.GetFloat(2, decoded.SrTime);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

int EncodeMessageInput(const MessageInputValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.ui", 2, input);
  if (status == ::BML::ERROR_OK) status = input.SetString(1, value.Message);
  return status;
}

int EncodeHudModeInput(const HudModeInputValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.ui", 3, input);
  if (status == ::BML::ERROR_OK) status = input.SetInt(1, value.Mode);
  return status;
}

int EncodeVisibleInput(const VisibleInputValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.ui", 4, input);
  if (status == ::BML::ERROR_OK) status = input.SetBool(1, value.Visible);
  return status;
}

int EncodeEmptyInput(const EmptyInputValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.ui", 5, input);
  return status;
}

int CommandHudFpsShow(const VisibleInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeVisibleInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_fps_show", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudSet(const HudModeInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeHudModeInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_set", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudSrPause(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_sr_pause", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudSrReset(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_sr_reset", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudSrShow(const VisibleInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeVisibleInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_sr_show", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudSrStart(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_sr_start", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandHudTitleShow(const VisibleInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeVisibleInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "hud_title_show", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandMapMenuClose(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "map_menu_close", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandMapMenuOpen(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "map_menu_open", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandMessageAdd(const MessageInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeMessageInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "message_add", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandMessageClear(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "message_clear", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandModsMenuClose(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "mods_menu_close", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int CommandModsMenuOpen(const EmptyInputValue &in inputValue, CommandResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeEmptyInput(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeCommand("bml.ui", "mods_menu_open", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeCommandResult(record, value);
  return status;
}

int ReadState(HudStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.ui", "state", record);
  if (status == ::BML::ERROR_OK) status = DecodeHudState(record, value);
  return status;
}

} // namespace BmlUi
} // namespace BMLInteropGenerated
