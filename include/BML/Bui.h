#ifndef BML_BUI_H
#define BML_BUI_H

#include "imgui.h"

#include "CKContext.h"
#include "CKRenderContext.h"

#include "BML/Export.h"

namespace Bui {
    BML_EXPORT ImGuiContext *GetImGuiContext();

    BML_EXPORT CKTexture *LoadTexture(CKContext *context, const char *id, const char *filename, int slot = 0);

    BML_EXPORT bool InitTextures(CKContext *context);

    BML_EXPORT bool NormalButton(const char *name, const char *text, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool BackButton(const char *name, const char *text = "Back", ImGuiButtonFlags flags = 0);
    BML_EXPORT bool SettingButton(const char *name, const char *text, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LevelButton(const char *name, const char *text, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool SmallButton(const char *name, const char *text, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool LeftButton(const char *name, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool RightButton(const char *name, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool PlusButton(const char *name, ImGuiButtonFlags flags = 0);
    BML_EXPORT bool MinusButton(const char *name, ImGuiButtonFlags flags = 0);
}

#endif // BML_BUI_H