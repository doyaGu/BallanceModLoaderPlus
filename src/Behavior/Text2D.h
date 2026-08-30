#ifndef BML_BEHAVIOR_TEXT2D_H
#define BML_BEHAVIOR_TEXT2D_H

#include <string>

#include "Behavior/Runtime.h"

namespace BML::Behavior::Text2D {

struct Options {
    CK2dEntity *Target = nullptr;
    int FontIndex = 0;
    std::string Text;
    int Alignment = 0;
    VxRect Margin{2.0f, 2.0f, 2.0f, 2.0f};
    Vx2DVector Offset{0.0f, 0.0f};
    Vx2DVector ParagraphIndentation{0.0f, 0.0f};
    CKMaterial *BackgroundMaterial = nullptr;
    float CaretSize = 0.1f;
    CKMaterial *CaretMaterial = nullptr;
    int Flags = 1;
};

Spec Make(const Options &options);

} // namespace BML::Behavior::Text2D

#endif // BML_BEHAVIOR_TEXT2D_H
