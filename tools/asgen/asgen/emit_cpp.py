from __future__ import annotations

import re

from .handwritten import draw_list_method_bindings, object_properties, script_friendly_global_bindings
from .model import FunctionBinding
from .type_mapper import is_available_enum_value
from .policy import ADDRESS_OF_RETURN_FUNCTIONS, BORROWED_HANDLE_RETURNS, HEADER


def emit_header() -> str:
    return HEADER + """#ifndef BML_IMGUI_ANGELSCRIPT_BINDINGS_GENERATED_H
#define BML_IMGUI_ANGELSCRIPT_BINDINGS_GENERATED_H

class asIScriptEngine;

int RegisterBMLImGuiAngelScriptBindings(asIScriptEngine *engine, const char **errorMessage);

#endif
"""


def property_cpp_type(as_type: str) -> str:
    if as_type == "uint":
        return "unsigned int"
    return as_type


def safe_cpp_suffix(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", text)


def default_cpp_value(cpp_type: str) -> str:
    if cpp_type in {"ImVec2", "ImVec4"}:
        return f"{cpp_type}()"
    if cpp_type == "bool":
        return "false"
    if cpp_type in {"float", "double"}:
        return "0.0f"
    return f"{cpp_type}()"


def cpp_string(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\"", "\\\"")


def needs_string_bridge(declaration: str) -> bool:
    return "string" in declaration


def emit_object_member_wrappers() -> list[str]:
    out: list[str] = []
    for type_name, fields in object_properties().items():
        for as_type, field, read_only in fields:
            cpp_type = property_cpp_type(as_type)
            suffix = safe_cpp_suffix(f"{type_name}_{field}")
            out.append(f"{cpp_type} BMLImGuiAS_Get_{suffix}({type_name} *self) {{ BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return {default_cpp_value(cpp_type)}; {cpp_type} value = self->{field}; BMLImGuiASEndCall(&scope); return value; }}\n")
            if not read_only:
                out.append(f"void BMLImGuiAS_Set_{suffix}({type_name} *self, {cpp_type} value) {{ BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->{field} = value; BMLImGuiASEndCall(&scope); }}\n")
    out.append("\n")
    return out


def emit_object_members() -> list[str]:
    out: list[str] = []
    properties = object_properties()
    for type_name, fields in properties.items():
        for as_type, field, read_only in fields:
            suffix = safe_cpp_suffix(f"{type_name}_{field}")
            out.append(f'    r = engine->RegisterObjectMethod("{type_name}", "{as_type} get_{field}() property", asFUNCTION(BMLImGuiAS_Get_{suffix}), asCALL_CDECL_OBJFIRST); BML_IMGUI_AS_CHECK(r, "{type_name}.get_{field}");\n')
            if not read_only:
                out.append(f'    r = engine->RegisterObjectMethod("{type_name}", "void set_{field}({as_type} value) property", asFUNCTION(BMLImGuiAS_Set_{suffix}), asCALL_CDECL_OBJFIRST); BML_IMGUI_AS_CHECK(r, "{type_name}.set_{field}");\n')

    for binding in draw_list_method_bindings():
        if needs_string_bridge(binding.declaration):
            out.append(f'    r = engine->RegisterObjectMethod("ImDrawList", "{binding.declaration}", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&{binding.function}), asCALL_GENERIC); BML_IMGUI_AS_CHECK(r, "ImDrawList.{binding.declaration}");\n')
        else:
            out.append(f'    r = engine->RegisterObjectMethod("ImDrawList", "{binding.declaration}", asFUNCTION({binding.function}), asCALL_CDECL_OBJFIRST); BML_IMGUI_AS_CHECK(r, "ImDrawList.{binding.declaration}");\n')
    return out


def emit_wrapper(fn: FunctionBinding) -> list[str]:
    out: list[str] = []
    params = ", ".join(f"{p.cpp_type} {p.name}" for p in fn.params)
    out.append(f"{fn.cpp_return_type} {fn.wrapper_name}({params}) {{\n")
    if fn.needs_frame_scope:
        out.append("    BMLImGuiASCallScope scope;\n")
        out.append("    if (!BMLImGuiASBeginCall(&scope)) {\n")
        if fn.return_kind == "void":
            out.append("        return;\n")
        elif fn.return_kind == "string":
            out.append("        return std::string();\n")
        elif fn.cpp_return_type == "ImVec2":
            out.append("        return ImVec2();\n")
        elif fn.cpp_return_type == "ImVec4":
            out.append("        return ImVec4();\n")
        elif fn.cpp_return_type in {"float", "double"}:
            out.append("        return 0.0f;\n")
        else:
            out.append("        return 0;\n")
        out.append("    }\n")
    if fn.char_buffer_name:
        name = fn.char_buffer_name
        cap = next((p.name for p in fn.params if p.name != name and p.cpp_type == "size_t"), f"{name}_size")
        out.append(f"    std::vector<char> {name}Buffer;\n")
        out.append(f"    size_t {name}Capacity = {cap} > 0 ? {cap} : ({name}.size() + 256);\n")
        out.append(f"    if ({name}Capacity <= {name}.size()) {{ {name}Capacity = {name}.size() + 1; }}\n")
        out.append(f"    {name}Buffer.resize({name}Capacity, '\\0');\n")
        out.append(f"    if (!{name}.empty()) {{ memcpy({name}Buffer.data(), {name}.c_str(), {name}.size() < {name}Buffer.size() ? {name}.size() : {name}Buffer.size() - 1); }}\n")
    call = f"ImGui::{fn.imgui_name}({', '.join(fn.call_args)})"
    if fn.return_kind == "void":
        out.append(f"    {call};\n")
        if fn.char_buffer_name:
            out.append(f"    {fn.char_buffer_name}.assign({fn.char_buffer_name}Buffer.data());\n")
        if fn.needs_frame_scope:
            out.append("    BMLImGuiASEndCall(&scope);\n")
        out.append("}\n\n")
        return out
    if fn.return_kind == "string":
        out.append(f"    const char *result = {call};\n")
        out.append("    std::string copy = result ? result : \"\";\n")
        if fn.needs_frame_scope:
            out.append("    BMLImGuiASEndCall(&scope);\n")
        out.append("    return copy;\n")
        out.append("}\n\n")
        return out
    if fn.return_kind == "handle" and fn.imgui_name in ADDRESS_OF_RETURN_FUNCTIONS:
        out.append(f"    {fn.cpp_return_type} result = &{call};\n")
    else:
        out.append(f"    {fn.cpp_return_type} result = {call};\n")
    if fn.char_buffer_name:
        out.append(f"    {fn.char_buffer_name}.assign({fn.char_buffer_name}Buffer.data());\n")
    if fn.needs_frame_scope:
        out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    return out


def emit_script_friendly_global_registrations() -> list[str]:
    out: list[str] = []
    for binding in script_friendly_global_bindings():
        declaration = cpp_string(binding.declaration)
        diagnostic_name = cpp_string(binding.diagnostic_name or binding.declaration)
        if needs_string_bridge(binding.declaration):
            out.append(f'    r = engine->RegisterGlobalFunction("{declaration}", BML_AS_GENERIC_FUNCTION(&{binding.wrapper}), asCALL_GENERIC); BML_IMGUI_AS_CHECK_RESET_NAMESPACE(engine, r, "{diagnostic_name}");\n')
        else:
            out.append(f'    r = engine->RegisterGlobalFunction("{declaration}", asFUNCTION({binding.wrapper}), {binding.call_convention}); BML_IMGUI_AS_CHECK_RESET_NAMESPACE(engine, r, "{diagnostic_name}");\n')
    return out


def emit_generated_global_registrations(functions: list[FunctionBinding]) -> list[str]:
    out: list[str] = []
    for fn in functions:
        decl = cpp_string(fn.script_decl)
        if needs_string_bridge(fn.script_decl):
            out.append(f'    r = engine->RegisterGlobalFunction("{decl}", BML_AS_GENERIC_FUNCTION(&{fn.wrapper_name}), asCALL_GENERIC); BML_IMGUI_AS_CHECK_RESET_NAMESPACE(engine, r, "{decl}");\n')
        else:
            out.append(f'    r = engine->RegisterGlobalFunction("{decl}", asFUNCTION({fn.wrapper_name}), asCALL_CDECL); BML_IMGUI_AS_CHECK_RESET_NAMESPACE(engine, r, "{decl}");\n')
    return out


def emit_source(ctx, functions: list[FunctionBinding]) -> str:
    out: list[str] = []
    out.append(HEADER)
    out.append('#include "AngelScript/generated/BMLImGuiAngelScriptBindings.h"\n')
    out.append('#include "ScriptStringInterop.h"\n')
    out.append('#include "AngelScriptImGuiBindings.h"\n')
    out.append('#include "CKTexture.h"\n')
    out.append('#include "imgui.h"\n')
    out.append("#include <angelscript.h>\n")
    out.append("#include <new>\n#include <stddef.h>\n#include <string>\n#include <string.h>\n#include <vector>\n\n")
    out.append("namespace {\n\n")
    out.append("struct BMLImGuiASVec3 { float x, y, z; };\n")
    out.append("struct BMLImGuiASInt2 { int x, y; };\n")
    out.append("struct BMLImGuiASInt3 { int x, y, z; };\n")
    out.append("struct BMLImGuiASInt4 { int x, y, z, w; };\n\n")
    out.append("void BMLImGuiAS_ConstructImVec2(ImVec2 *self) { new (self) ImVec2(); }\n")
    out.append("void BMLImGuiAS_ConstructImVec2XY(float x, float y, ImVec2 *self) { new (self) ImVec2(x, y); }\n")
    out.append("void BMLImGuiAS_ConstructImVec3(BMLImGuiASVec3 *self) { self->x = 0.0f; self->y = 0.0f; self->z = 0.0f; }\n")
    out.append("void BMLImGuiAS_ConstructImVec3XYZ(float x, float y, float z, BMLImGuiASVec3 *self) { self->x = x; self->y = y; self->z = z; }\n")
    out.append("void BMLImGuiAS_ConstructImVec4(ImVec4 *self) { new (self) ImVec4(); }\n")
    out.append("void BMLImGuiAS_ConstructImVec4XYZW(float x, float y, float z, float w, ImVec4 *self) { new (self) ImVec4(x, y, z, w); }\n\n")
    out.append("void BMLImGuiAS_ConstructInt2(BMLImGuiASInt2 *self) { self->x = 0; self->y = 0; }\n")
    out.append("void BMLImGuiAS_ConstructInt2XY(int x, int y, BMLImGuiASInt2 *self) { self->x = x; self->y = y; }\n")
    out.append("void BMLImGuiAS_ConstructInt3(BMLImGuiASInt3 *self) { self->x = 0; self->y = 0; self->z = 0; }\n")
    out.append("void BMLImGuiAS_ConstructInt3XYZ(int x, int y, int z, BMLImGuiASInt3 *self) { self->x = x; self->y = y; self->z = z; }\n")
    out.append("void BMLImGuiAS_ConstructInt4(BMLImGuiASInt4 *self) { self->x = 0; self->y = 0; self->z = 0; self->w = 0; }\n")
    out.append("void BMLImGuiAS_ConstructInt4XYZW(int x, int y, int z, int w, BMLImGuiASInt4 *self) { self->x = x; self->y = y; self->z = z; self->w = w; }\n\n")
    out.append("bool BMLImGuiAS_BeginNoOpen(const std::string &name, ImGuiWindowFlags flags) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    bool result = ImGui::Begin(name.c_str(), nullptr, flags);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("ImVec2 BMLImGuiAS_CalcTextSizeShort(const std::string &text, bool hideTextAfterDoubleHash, float wrapWidth) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return ImVec2(); }\n")
    out.append("    ImVec2 result = ImGui::CalcTextSize(text.c_str(), nullptr, hideTextAfterDoubleHash, wrapWidth);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("void BMLImGuiAS_TextUnformattedShort(const std::string &text) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return; }\n")
    out.append("    ImGui::TextUnformatted(text.c_str(), nullptr);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("}\n\n")
    out.append("void BMLImGuiAS_TextUnformattedRange(const std::string &text, unsigned int length) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return; }\n")
    out.append("    const size_t clamped = length < text.size() ? (size_t)length : text.size();\n")
    out.append("    ImGui::TextUnformatted(text.c_str(), text.c_str() + clamped);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("}\n\n")
    out.append("ImVec4 BMLImGuiAS_GetStyleColorVec4Copy(ImGuiCol idx) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return ImVec4(); }\n")
    out.append("    ImVec4 result = ImGui::GetStyleColorVec4(idx);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("void BMLImGuiAS_Image(CKTexture *texture, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return; }\n")
    out.append("    if (!texture) { BMLImGuiASReportRuntimeWarning(\"Image called with null CKTexture.\"); BMLImGuiASEndCall(&scope); return; }\n")
    out.append("    ImGui::Image(ImTextureRef((void *)texture), size, uv0, uv1);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("}\n\n")
    out.append("void BMLImGuiAS_ImageWithBg(CKTexture *texture, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &bg, const ImVec4 &tint) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return; }\n")
    out.append("    if (!texture) { BMLImGuiASReportRuntimeWarning(\"ImageWithBg called with null CKTexture.\"); BMLImGuiASEndCall(&scope); return; }\n")
    out.append("    ImGui::ImageWithBg(ImTextureRef((void *)texture), size, uv0, uv1, bg, tint);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiAS_ImageButton(const std::string &id, CKTexture *texture, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &bg, const ImVec4 &tint) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    if (!texture) { BMLImGuiASReportRuntimeWarning(\"ImageButton called with null CKTexture.\"); BMLImGuiASEndCall(&scope); return false; }\n")
    out.append("    bool result = ImGui::ImageButton(id.c_str(), ImTextureRef((void *)texture), size, uv0, uv1, bg, tint);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    for suffix, cpp_type, data_type in [
        ("Float", "float", "ImGuiDataType_Float"),
        ("Double", "double", "ImGuiDataType_Double"),
        ("Int", "int", "ImGuiDataType_S32"),
        ("UInt", "unsigned int", "ImGuiDataType_U32"),
        ("Int64", "ImS64", "ImGuiDataType_S64"),
        ("UInt64", "ImU64", "ImGuiDataType_U64"),
    ]:
        out.append(f"bool BMLImGuiAS_SliderScalar{suffix}(const std::string &label, {cpp_type} &v, {cpp_type} v_min, {cpp_type} v_max, const std::string &format, ImGuiSliderFlags flags) {{\n")
        out.append("    BMLImGuiASCallScope scope;\n")
        out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
        out.append(f"    bool result = ImGui::SliderScalar(label.c_str(), {data_type}, &v, &v_min, &v_max, format.empty() ? nullptr : format.c_str(), flags);\n")
        out.append("    BMLImGuiASEndCall(&scope);\n")
        out.append("    return result;\n")
        out.append("}\n\n")
        out.append(f"bool BMLImGuiAS_DragScalar{suffix}(const std::string &label, {cpp_type} &v, float v_speed, {cpp_type} v_min, {cpp_type} v_max, const std::string &format, ImGuiSliderFlags flags) {{\n")
        out.append("    BMLImGuiASCallScope scope;\n")
        out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
        out.append(f"    bool result = ImGui::DragScalar(label.c_str(), {data_type}, &v, v_speed, &v_min, &v_max, format.empty() ? nullptr : format.c_str(), flags);\n")
        out.append("    BMLImGuiASEndCall(&scope);\n")
        out.append("    return result;\n")
        out.append("}\n\n")
        out.append(f"bool BMLImGuiAS_InputScalar{suffix}(const std::string &label, {cpp_type} &v, {cpp_type} step, {cpp_type} step_fast, const std::string &format, ImGuiInputTextFlags flags) {{\n")
        out.append("    BMLImGuiASCallScope scope;\n")
        out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
        out.append(f"    bool result = ImGui::InputScalar(label.c_str(), {data_type}, &v, &step, &step_fast, format.empty() ? nullptr : format.c_str(), flags);\n")
        out.append("    BMLImGuiASEndCall(&scope);\n")
        out.append("    return result;\n")
        out.append("}\n\n")
    out.append("bool BMLImGuiAS_SetDragDropPayloadString(const std::string &type, const std::string &payload, ImGuiCond cond) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    bool result = ImGui::SetDragDropPayload(type.c_str(), payload.data(), payload.size(), cond);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiAS_AcceptDragDropPayloadString(const std::string &type, std::string &payload, ImGuiDragDropFlags flags) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    payload.clear();\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    const ImGuiPayload *accepted = ImGui::AcceptDragDropPayload(type.c_str(), flags);\n")
    out.append("    if (!accepted || !accepted->Data || accepted->DataSize <= 0) { BMLImGuiASEndCall(&scope); return false; }\n")
    out.append("    payload.assign((const char *)accepted->Data, (size_t)accepted->DataSize);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return true;\n")
    out.append("}\n\n")
    out.append("std::string BMLImGuiAS_ZeroSeparatedItemsFromLines(const std::string &itemsText) {\n")
    out.append("    std::string items;\n")
    out.append("    items.reserve(itemsText.size() + 2);\n")
    out.append("    bool wroteItem = false;\n")
    out.append("    bool atLineStart = true;\n")
    out.append("    for (char ch : itemsText) {\n")
    out.append("        if (ch == '\\r') { continue; }\n")
    out.append("        if (ch == '\\n') { if (!atLineStart) { items.push_back('\\0'); wroteItem = true; atLineStart = true; } continue; }\n")
    out.append("        items.push_back(ch);\n")
    out.append("        atLineStart = false;\n")
    out.append("    }\n")
    out.append("    if (!atLineStart) { items.push_back('\\0'); wroteItem = true; }\n")
    out.append("    if (!wroteItem) { items.push_back('\\0'); }\n")
    out.append("    items.push_back('\\0');\n")
    out.append("    return items;\n")
    out.append("}\n\n")
    out.append("std::vector<std::string> BMLImGuiAS_LineItems(const std::string &itemsText) {\n")
    out.append("    std::vector<std::string> items;\n")
    out.append("    std::string current;\n")
    out.append("    for (char ch : itemsText) {\n")
    out.append("        if (ch == '\\r') { continue; }\n")
    out.append("        if (ch == '\\n') { if (!current.empty()) { items.push_back(current); current.clear(); } continue; }\n")
    out.append("        current.push_back(ch);\n")
    out.append("    }\n")
    out.append("    if (!current.empty()) { items.push_back(current); }\n")
    out.append("    return items;\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiAS_ComboText(const std::string &label, int &currentItem, const std::string &itemsText, int popupMaxHeightInItems) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    std::string items = BMLImGuiAS_ZeroSeparatedItemsFromLines(itemsText);\n")
    out.append("    bool result = ImGui::Combo(label.c_str(), &currentItem, items.c_str(), popupMaxHeightInItems);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiAS_ListBoxText(const std::string &label, int &currentItem, const std::string &itemsText, int heightInItems) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    std::vector<std::string> items = BMLImGuiAS_LineItems(itemsText);\n")
    out.append("    std::vector<const char *> itemPointers;\n")
    out.append("    itemPointers.reserve(items.size());\n")
    out.append("    for (const std::string &item : items) { itemPointers.push_back(item.c_str()); }\n")
    out.append("    bool result = !itemPointers.empty() && ImGui::ListBox(label.c_str(), &currentItem, itemPointers.data(), (int)itemPointers.size(), heightInItems);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiAS_ColorPicker4NoRef(const std::string &label, ImVec4 &color, ImGuiColorEditFlags flags) {\n")
    out.append("    BMLImGuiASCallScope scope;\n")
    out.append("    if (!BMLImGuiASBeginCall(&scope)) { return false; }\n")
    out.append("    bool result = ImGui::ColorPicker4(label.c_str(), &color.x, flags, nullptr);\n")
    out.append("    BMLImGuiASEndCall(&scope);\n")
    out.append("    return result;\n")
    out.append("}\n\n")
    out.append("bool BMLImGuiASBeginBorrowedObjectCall(const void *self, BMLImGuiASCallScope *scope) {\n")
    out.append("    if (!self) { return false; }\n")
    out.append("    return BMLImGuiASBeginCall(scope);\n")
    out.append("}\n\n")
    out.append("void BMLImGuiAS_DrawListAddLine(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, ImU32 col, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddLine(p1, p2, col, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddRect(ImDrawList *self, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddRect(p_min, p_max, col, rounding, flags, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddRectFilled(ImDrawList *self, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddRectFilled(p_min, p_max, col, rounding, flags); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddCircle(ImDrawList *self, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddCircle(center, radius, col, num_segments, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddCircleFilled(ImDrawList *self, const ImVec2 &center, float radius, ImU32 col, int num_segments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddCircleFilled(center, radius, col, num_segments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddTriangle(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddTriangle(p1, p2, p3, col, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddTriangleFilled(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddTriangleFilled(p1, p2, p3, col); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddQuad(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddQuad(p1, p2, p3, p4, col, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddQuadFilled(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddQuadFilled(p1, p2, p3, p4, col); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddBezierCubic(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddBezierCubic(p1, p2, p3, p4, col, thickness, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddBezierQuadratic(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddBezierQuadratic(p1, p2, p3, col, thickness, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddNgon(ImDrawList *self, const ImVec2 &center, float radius, ImU32 col, int numSegments, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddNgon(center, radius, col, numSegments, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddNgonFilled(ImDrawList *self, const ImVec2 &center, float radius, ImU32 col, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddNgonFilled(center, radius, col, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddPolyline4(ImDrawList *self, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, ImDrawFlags flags, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; ImVec2 points[4] = {p1, p2, p3, p4}; self->AddPolyline(points, 4, col, flags, thickness); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddText(ImDrawList *self, const ImVec2 &pos, ImU32 col, const std::string &text) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->AddText(pos, col, text.c_str()); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListAddImage(ImDrawList *self, CKTexture *texture, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; if (!texture) { BMLImGuiASReportRuntimeWarning(\"ImDrawList.AddImage called with null CKTexture.\"); BMLImGuiASEndCall(&scope); return; } self->AddImage(ImTextureRef((void *)texture), p_min, p_max, uv_min, uv_max, col); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathClear(ImDrawList *self) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathClear(); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathLineTo(ImDrawList *self, const ImVec2 &pos) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathLineTo(pos); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathArcTo(ImDrawList *self, const ImVec2 &center, float radius, float aMin, float aMax, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathArcTo(center, radius, aMin, aMax, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathArcToFast(ImDrawList *self, const ImVec2 &center, float radius, int aMinOf12, int aMaxOf12) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathArcToFast(center, radius, aMinOf12, aMaxOf12); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathBezierCubicCurveTo(ImDrawList *self, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathBezierCubicCurveTo(p2, p3, p4, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathBezierQuadraticCurveTo(ImDrawList *self, const ImVec2 &p2, const ImVec2 &p3, int numSegments) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathBezierQuadraticCurveTo(p2, p3, numSegments); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathRect(ImDrawList *self, const ImVec2 &rectMin, const ImVec2 &rectMax, float rounding, ImDrawFlags flags) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathRect(rectMin, rectMax, rounding, flags); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathFillConvex(ImDrawList *self, ImU32 col) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathFillConvex(col); BMLImGuiASEndCall(&scope); }\n")
    out.append("void BMLImGuiAS_DrawListPathStroke(ImDrawList *self, ImU32 col, ImDrawFlags flags, float thickness) { BMLImGuiASCallScope scope; if (!BMLImGuiASBeginBorrowedObjectCall(self, &scope)) return; self->PathStroke(col, flags, thickness); BMLImGuiASEndCall(&scope); }\n\n")
    out.extend(emit_object_member_wrappers())

    for fn in functions:
        out.extend(emit_wrapper(fn))

    out.append("} // namespace\n\n")
    out.append("int RegisterBMLImGuiAngelScriptBindings(asIScriptEngine *engine, const char **errorMessage) {\n")
    out.append("    if (!engine) {\n        BMLImGuiASSetRegistrationError(errorMessage, \"engine\", -1);\n        return -1;\n    }\n")
    out.append("    int r = 0;\n")
    out.append('    r = engine->RegisterObjectType("ImVec2", sizeof(ImVec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<ImVec2>()); BML_IMGUI_AS_CHECK(r, "RegisterObjectType(ImVec2)");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(BMLImGuiAS_ConstructImVec2, (ImVec2 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec2()");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTIONPR(BMLImGuiAS_ConstructImVec2XY, (float, float, ImVec2 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec2(float,float)");\n')
    out.append('    r = engine->RegisterObjectProperty("ImVec2", "float x", (int)offsetof(ImVec2, x)); BML_IMGUI_AS_CHECK(r, "ImVec2.x");\n')
    out.append('    r = engine->RegisterObjectProperty("ImVec2", "float y", (int)offsetof(ImVec2, y)); BML_IMGUI_AS_CHECK(r, "ImVec2.y");\n')
    out.append('    r = engine->RegisterObjectType("ImVec3", sizeof(BMLImGuiASVec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<BMLImGuiASVec3>()); BML_IMGUI_AS_CHECK(r, "RegisterObjectType(ImVec3)");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(BMLImGuiAS_ConstructImVec3, (BMLImGuiASVec3 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec3()");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTIONPR(BMLImGuiAS_ConstructImVec3XYZ, (float, float, float, BMLImGuiASVec3 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec3(float,float,float)");\n')
    for field in ["x", "y", "z"]:
        out.append(f'    r = engine->RegisterObjectProperty("ImVec3", "float {field}", (int)offsetof(BMLImGuiASVec3, {field})); BML_IMGUI_AS_CHECK(r, "ImVec3.{field}");\n')
    out.append('    r = engine->RegisterObjectType("ImVec4", sizeof(ImVec4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<ImVec4>()); BML_IMGUI_AS_CHECK(r, "RegisterObjectType(ImVec4)");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(BMLImGuiAS_ConstructImVec4, (ImVec4 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec4()");\n')
    out.append('    r = engine->RegisterObjectBehaviour("ImVec4", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTIONPR(BMLImGuiAS_ConstructImVec4XYZW, (float, float, float, float, ImVec4 *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct ImVec4(float,float,float,float)");\n')
    for field in ["x", "y", "z", "w"]:
        out.append(f'    r = engine->RegisterObjectProperty("ImVec4", "float {field}", (int)offsetof(ImVec4, {field})); BML_IMGUI_AS_CHECK(r, "ImVec4.{field}");\n')

    for as_name, cpp_name, ctor0, ctor_args, fields in [
        ("ImGuiInt2", "BMLImGuiASInt2", "BMLImGuiAS_ConstructInt2", "BMLImGuiAS_ConstructInt2XY", ["x", "y"]),
        ("ImGuiInt3", "BMLImGuiASInt3", "BMLImGuiAS_ConstructInt3", "BMLImGuiAS_ConstructInt3XYZ", ["x", "y", "z"]),
        ("ImGuiInt4", "BMLImGuiASInt4", "BMLImGuiAS_ConstructInt4", "BMLImGuiAS_ConstructInt4XYZW", ["x", "y", "z", "w"]),
    ]:
        out.append(f'    r = engine->RegisterObjectType("{as_name}", sizeof({cpp_name}), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<{cpp_name}>()); BML_IMGUI_AS_CHECK(r, "RegisterObjectType({as_name})");\n')
        out.append(f'    r = engine->RegisterObjectBehaviour("{as_name}", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR({ctor0}, ({cpp_name} *), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct {as_name}()");\n')
        arg_types = ", ".join(["int"] * len(fields))
        fn_types = ", ".join(["int"] * len(fields) + [f"{cpp_name} *"])
        out.append(f'    r = engine->RegisterObjectBehaviour("{as_name}", asBEHAVE_CONSTRUCT, "void f({arg_types})", asFUNCTIONPR({ctor_args}, ({fn_types}), void), asCALL_CDECL_OBJLAST); BML_IMGUI_AS_CHECK(r, "Construct {as_name}({arg_types})");\n')
        for field in fields:
            out.append(f'    r = engine->RegisterObjectProperty("{as_name}", "int {field}", (int)offsetof({cpp_name}, {field})); BML_IMGUI_AS_CHECK(r, "{as_name}.{field}");\n')

    ctx.opaque_handle_types.update(BORROWED_HANDLE_RETURNS)
    for type_name in sorted(ctx.opaque_handle_types):
        out.append(f'    r = engine->RegisterObjectType("{type_name}", 0, asOBJ_REF | asOBJ_NOCOUNT); BML_IMGUI_AS_CHECK(r, "RegisterObjectType({type_name})");\n')

    ctx.used_enums.update({
        "ImDrawFlags",
        "ImDrawListFlags",
        "ImFontFlags",
        "ImGuiBackendFlags",
        "ImGuiConfigFlags",
        "ImGuiDir",
        "ImGuiCond",
        "ImGuiDataType",
        "ImGuiDragDropFlags",
        "ImGuiInputTextFlags",
        "ImGuiSliderFlags",
        "ImGuiViewportFlags",
    })
    for enum_name in sorted(ctx.used_enums):
        values = ctx.enum_groups.get(enum_name)
        if not values:
            continue
        out.append(f'    r = engine->RegisterEnum("{enum_name}"); BML_IMGUI_AS_CHECK(r, "RegisterEnum({enum_name})");\n')
        for value in values[1]:
            name = value.get("name")
            if not name or not is_available_enum_value(enum_name, str(name)):
                continue
            calc_value = value.get("calc_value")
            if not isinstance(calc_value, int) or calc_value < -2147483648 or calc_value > 2147483647:
                continue
            out.append(f'    r = engine->RegisterEnumValue("{enum_name}", "{name}", {calc_value}); BML_IMGUI_AS_CHECK(r, "RegisterEnumValue({name})");\n')

    out.extend(emit_object_members())

    out.append('    r = engine->SetDefaultNamespace("ImGui"); BML_IMGUI_AS_CHECK(r, "SetDefaultNamespace(ImGui)");\n')
    out.extend(emit_script_friendly_global_registrations())
    out.extend(emit_generated_global_registrations(functions))
    out.append('    r = engine->SetDefaultNamespace(""); BML_IMGUI_AS_CHECK(r, "SetDefaultNamespace(empty)");\n')
    out.append("    return 0;\n}\n")
    return "".join(out)
