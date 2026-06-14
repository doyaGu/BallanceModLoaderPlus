from __future__ import annotations

from .model import HandWrittenGlobalBinding, ObjectMethodBinding


def script_friendly_global_bindings() -> list[HandWrittenGlobalBinding]:
    bindings = [
        HandWrittenGlobalBinding("bool Begin(const string &in name, ImGuiWindowFlags flags = ImGuiWindowFlags_None)", "BMLImGuiAS_BeginNoOpen"),
        HandWrittenGlobalBinding("ImVec2 CalcTextSize(const string &in text, bool hide_text_after_double_hash = false, float wrap_width = -1.0f)", "BMLImGuiAS_CalcTextSizeShort"),
        HandWrittenGlobalBinding("void TextUnformatted(const string &in text)", "BMLImGuiAS_TextUnformattedShort"),
        HandWrittenGlobalBinding("void TextUnformattedRange(const string &in text, uint length)", "BMLImGuiAS_TextUnformattedRange"),
        HandWrittenGlobalBinding("ImVec4 GetStyleColorVec4(ImGuiCol idx)", "BMLImGuiAS_GetStyleColorVec4Copy"),
    ]
    for suffix, as_type in [
        ("Float", "float"),
        ("Double", "double"),
        ("Int", "int"),
        ("UInt", "uint"),
        ("Int64", "int64"),
        ("UInt64", "uint64"),
    ]:
        bindings.append(HandWrittenGlobalBinding(f"bool SliderScalar{suffix}(const string &in label, {as_type} &inout v, {as_type} v_min, {as_type} v_max, const string &in format = \"\", ImGuiSliderFlags flags = ImGuiSliderFlags_None)", f"BMLImGuiAS_SliderScalar{suffix}", f"bool SliderScalar{suffix}"))
        bindings.append(HandWrittenGlobalBinding(f"bool DragScalar{suffix}(const string &in label, {as_type} &inout v, float v_speed, {as_type} v_min, {as_type} v_max, const string &in format = \"\", ImGuiSliderFlags flags = ImGuiSliderFlags_None)", f"BMLImGuiAS_DragScalar{suffix}", f"bool DragScalar{suffix}"))
        bindings.append(HandWrittenGlobalBinding(f"bool InputScalar{suffix}(const string &in label, {as_type} &inout v, {as_type} step, {as_type} step_fast, const string &in format = \"\", ImGuiInputTextFlags flags = ImGuiInputTextFlags_None)", f"BMLImGuiAS_InputScalar{suffix}", f"bool InputScalar{suffix}"))
    bindings.extend([
        HandWrittenGlobalBinding("bool SetDragDropPayloadString(const string &in type, const string &in payload, ImGuiCond cond = ImGuiCond_None)", "BMLImGuiAS_SetDragDropPayloadString"),
        HandWrittenGlobalBinding("bool AcceptDragDropPayloadString(const string &in type, string &out payload, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None)", "BMLImGuiAS_AcceptDragDropPayloadString"),
        HandWrittenGlobalBinding("bool ComboText(const string &in label, int &inout current_item, const string &in newline_separated_items, int popup_max_height_in_items = -1)", "BMLImGuiAS_ComboText"),
        HandWrittenGlobalBinding("bool ListBoxText(const string &in label, int &inout current_item, const string &in newline_separated_items, int height_in_items = -1)", "BMLImGuiAS_ListBoxText"),
        HandWrittenGlobalBinding("bool ColorPicker4(const string &in label, ImVec4 &inout col, ImGuiColorEditFlags flags = ImGuiColorEditFlags_None)", "BMLImGuiAS_ColorPicker4NoRef"),
        HandWrittenGlobalBinding("void Image(CKTexture@ texture, const ImVec2 &in size, const ImVec2 &in uv0 = ImVec2(0,0), const ImVec2 &in uv1 = ImVec2(1,1))", "BMLImGuiAS_Image"),
        HandWrittenGlobalBinding("void ImageWithBg(CKTexture@ texture, const ImVec2 &in size, const ImVec2 &in uv0 = ImVec2(0,0), const ImVec2 &in uv1 = ImVec2(1,1), const ImVec4 &in bg_col = ImVec4(0,0,0,0), const ImVec4 &in tint_col = ImVec4(1,1,1,1))", "BMLImGuiAS_ImageWithBg"),
        HandWrittenGlobalBinding("bool ImageButton(const string &in id, CKTexture@ texture, const ImVec2 &in size, const ImVec2 &in uv0 = ImVec2(0,0), const ImVec2 &in uv1 = ImVec2(1,1), const ImVec4 &in bg_col = ImVec4(0,0,0,0), const ImVec4 &in tint_col = ImVec4(1,1,1,1))", "BMLImGuiAS_ImageButton"),
        HandWrittenGlobalBinding("ImDrawList@ GetBackgroundDrawList()", "BMLImGuiASGetBackgroundDrawList"),
        HandWrittenGlobalBinding("ImDrawList@ GetForegroundDrawList()", "BMLImGuiASGetForegroundDrawList"),
    ])
    return bindings


def script_friendly_global_declarations() -> list[str]:
    return [binding.declaration for binding in script_friendly_global_bindings()]


def object_properties() -> dict[str, list[tuple[str, str, bool]]]:
    read_only = False
    return {
        "ImGuiIO": [
            ("ImGuiConfigFlags", "ConfigFlags", True),
            ("ImGuiBackendFlags", "BackendFlags", True),
            ("ImVec2", "DisplaySize", True),
            ("ImVec2", "DisplayFramebufferScale", True),
            ("float", "DeltaTime", True),
            ("bool", "FontAllowUserScaling", True),
            ("bool", "MouseDrawCursor", True),
            ("float", "MouseDoubleClickTime", True),
            ("float", "MouseDoubleClickMaxDist", True),
            ("float", "MouseDragThreshold", True),
            ("float", "KeyRepeatDelay", True),
            ("float", "KeyRepeatRate", True),
            ("bool", "WantCaptureMouse", True),
            ("bool", "WantCaptureKeyboard", True),
            ("bool", "WantTextInput", True),
            ("bool", "WantSetMousePos", True),
            ("bool", "WantSaveIniSettings", True),
            ("bool", "NavActive", True),
            ("bool", "NavVisible", True),
            ("float", "Framerate", True),
            ("int", "MetricsRenderVertices", True),
            ("int", "MetricsRenderIndices", True),
            ("int", "MetricsRenderWindows", True),
            ("int", "MetricsActiveWindows", True),
            ("ImVec2", "MouseDelta", True),
            ("ImVec2", "MousePos", True),
            ("float", "MouseWheel", True),
        ],
        "ImGuiStyle": [
            ("float", "Alpha", read_only),
            ("float", "DisabledAlpha", read_only),
            ("ImVec2", "WindowPadding", read_only),
            ("float", "WindowRounding", read_only),
            ("float", "WindowBorderSize", read_only),
            ("ImVec2", "WindowMinSize", read_only),
            ("ImVec2", "WindowTitleAlign", read_only),
            ("ImGuiDir", "WindowMenuButtonPosition", read_only),
            ("float", "ChildRounding", read_only),
            ("float", "ChildBorderSize", read_only),
            ("float", "PopupRounding", read_only),
            ("float", "PopupBorderSize", read_only),
            ("ImVec2", "FramePadding", read_only),
            ("float", "FrameRounding", read_only),
            ("float", "FrameBorderSize", read_only),
            ("ImVec2", "ItemSpacing", read_only),
            ("ImVec2", "ItemInnerSpacing", read_only),
            ("ImVec2", "CellPadding", read_only),
            ("float", "IndentSpacing", read_only),
            ("float", "ScrollbarSize", read_only),
            ("float", "ScrollbarRounding", read_only),
            ("float", "GrabMinSize", read_only),
            ("float", "GrabRounding", read_only),
            ("float", "LogSliderDeadzone", read_only),
            ("float", "ImageRounding", read_only),
            ("float", "ImageBorderSize", read_only),
            ("float", "TabRounding", read_only),
            ("float", "TabBorderSize", read_only),
            ("ImGuiDir", "ColorButtonPosition", read_only),
            ("ImVec2", "ButtonTextAlign", read_only),
            ("ImVec2", "SelectableTextAlign", read_only),
            ("ImVec2", "DisplayWindowPadding", read_only),
            ("ImVec2", "DisplaySafeAreaPadding", read_only),
            ("float", "MouseCursorScale", read_only),
            ("bool", "AntiAliasedLines", read_only),
            ("bool", "AntiAliasedLinesUseTex", read_only),
            ("bool", "AntiAliasedFill", read_only),
            ("float", "CurveTessellationTol", read_only),
            ("float", "CircleTessellationMaxError", read_only),
        ],
        "ImGuiViewport": [
            ("uint", "ID", True),
            ("ImGuiViewportFlags", "Flags", True),
            ("ImVec2", "Pos", True),
            ("ImVec2", "Size", True),
            ("ImVec2", "FramebufferScale", True),
            ("ImVec2", "WorkPos", True),
            ("ImVec2", "WorkSize", True),
        ],
        "ImDrawList": [
            ("ImDrawListFlags", "Flags", True),
        ],
        "ImFont": [
            ("ImFontFlags", "Flags", True),
            ("float", "CurrentRasterizerDensity", True),
            ("uint", "FontId", True),
            ("float", "LegacySize", True),
            ("uint", "EllipsisChar", True),
            ("uint", "FallbackChar", True),
            ("bool", "EllipsisAutoBake", True),
        ],
    }


def draw_list_method_bindings() -> list[ObjectMethodBinding]:
    return [
        ObjectMethodBinding('void AddLine(const ImVec2 &in p1, const ImVec2 &in p2, uint col, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddLine'),
        ObjectMethodBinding('void AddRect(const ImVec2 &in p_min, const ImVec2 &in p_max, uint col, float rounding = 0.0f, ImDrawFlags flags = ImDrawFlags_None, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddRect'),
        ObjectMethodBinding('void AddRectFilled(const ImVec2 &in p_min, const ImVec2 &in p_max, uint col, float rounding = 0.0f, ImDrawFlags flags = ImDrawFlags_None)', 'BMLImGuiAS_DrawListAddRectFilled'),
        ObjectMethodBinding('void AddCircle(const ImVec2 &in center, float radius, uint col, int num_segments = 0, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddCircle'),
        ObjectMethodBinding('void AddCircleFilled(const ImVec2 &in center, float radius, uint col, int num_segments = 0)', 'BMLImGuiAS_DrawListAddCircleFilled'),
        ObjectMethodBinding('void AddTriangle(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, uint col, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddTriangle'),
        ObjectMethodBinding('void AddTriangleFilled(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, uint col)', 'BMLImGuiAS_DrawListAddTriangleFilled'),
        ObjectMethodBinding('void AddQuad(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, const ImVec2 &in p4, uint col, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddQuad'),
        ObjectMethodBinding('void AddQuadFilled(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, const ImVec2 &in p4, uint col)', 'BMLImGuiAS_DrawListAddQuadFilled'),
        ObjectMethodBinding('void AddBezierCubic(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, const ImVec2 &in p4, uint col, float thickness = 1.0f, int num_segments = 0)', 'BMLImGuiAS_DrawListAddBezierCubic'),
        ObjectMethodBinding('void AddBezierQuadratic(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, uint col, float thickness = 1.0f, int num_segments = 0)', 'BMLImGuiAS_DrawListAddBezierQuadratic'),
        ObjectMethodBinding('void AddNgon(const ImVec2 &in center, float radius, uint col, int num_segments, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddNgon'),
        ObjectMethodBinding('void AddNgonFilled(const ImVec2 &in center, float radius, uint col, int num_segments)', 'BMLImGuiAS_DrawListAddNgonFilled'),
        ObjectMethodBinding('void AddPolyline4(const ImVec2 &in p1, const ImVec2 &in p2, const ImVec2 &in p3, const ImVec2 &in p4, uint col, ImDrawFlags flags = ImDrawFlags_None, float thickness = 1.0f)', 'BMLImGuiAS_DrawListAddPolyline4'),
        ObjectMethodBinding('void AddText(const ImVec2 &in pos, uint col, const string &in text)', 'BMLImGuiAS_DrawListAddText'),
        ObjectMethodBinding('void AddImage(CKTexture@ texture, const ImVec2 &in p_min, const ImVec2 &in p_max, const ImVec2 &in uv_min = ImVec2(0,0), const ImVec2 &in uv_max = ImVec2(1,1), uint col = 0xffffffff)', 'BMLImGuiAS_DrawListAddImage'),
        ObjectMethodBinding('void PathClear()', 'BMLImGuiAS_DrawListPathClear'),
        ObjectMethodBinding('void PathLineTo(const ImVec2 &in pos)', 'BMLImGuiAS_DrawListPathLineTo'),
        ObjectMethodBinding('void PathArcTo(const ImVec2 &in center, float radius, float a_min, float a_max, int num_segments = 0)', 'BMLImGuiAS_DrawListPathArcTo'),
        ObjectMethodBinding('void PathArcToFast(const ImVec2 &in center, float radius, int a_min_of_12, int a_max_of_12)', 'BMLImGuiAS_DrawListPathArcToFast'),
        ObjectMethodBinding('void PathBezierCubicCurveTo(const ImVec2 &in p2, const ImVec2 &in p3, const ImVec2 &in p4, int num_segments = 0)', 'BMLImGuiAS_DrawListPathBezierCubicCurveTo'),
        ObjectMethodBinding('void PathBezierQuadraticCurveTo(const ImVec2 &in p2, const ImVec2 &in p3, int num_segments = 0)', 'BMLImGuiAS_DrawListPathBezierQuadraticCurveTo'),
        ObjectMethodBinding('void PathRect(const ImVec2 &in rect_min, const ImVec2 &in rect_max, float rounding = 0.0f, ImDrawFlags flags = ImDrawFlags_None)', 'BMLImGuiAS_DrawListPathRect'),
        ObjectMethodBinding('void PathFillConvex(uint col)', 'BMLImGuiAS_DrawListPathFillConvex'),
        ObjectMethodBinding('void PathStroke(uint col, ImDrawFlags flags = ImDrawFlags_None, float thickness = 1.0f)', 'BMLImGuiAS_DrawListPathStroke'),
    ]




def hand_written_global_count() -> int:
    return len(script_friendly_global_bindings())


def hand_written_object_method_count() -> int:
    return len(draw_list_method_bindings())
