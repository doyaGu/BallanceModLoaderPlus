#ifndef BML_UI_GUI_LEGACYTEXTFONT_H
#define BML_UI_GUI_LEGACYTEXTFONT_H

#include <string>
#include <vector>

class CKSpriteText;

namespace BGui {

std::string ChooseLegacyTextDefaultFace(const std::vector<std::string> &availableFaces);
void SelectLegacyTextDefaultFace(const std::vector<std::string> &availableFaces);

void InitializeLegacyTextFont(CKSpriteText *sprite, int viewportHeight);
void SetLegacyTextFont(CKSpriteText *sprite, const char *face, int size, int weight, bool italic, bool underline);
void RefreshLegacyTextFont(CKSpriteText *sprite, int viewportHeight);
void ForgetLegacyTextFont(CKSpriteText *sprite);

} // namespace BGui

#endif // BML_UI_GUI_LEGACYTEXTFONT_H
