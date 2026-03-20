// Auto-generated from Interface.dll -- do not edit manually.
#ifndef BML_BEHAVIORS_INTERFACE_H
#define BML_BEHAVIORS_INTERFACE_H

#include "BML/Behavior.h"
#include "CKAll.h"

namespace bml::Interface {

// ============================================================================
// _2DText
//
// Displays text on a 2D entity
// ============================================================================

class _2DText {
public:
    static constexpr CKGUID Guid = CKGUID(0x4B815D01, 0x5381C0FE);

    _2DText &Font(int v) { m_Font = v; return *this; }
    _2DText &Text(const char* v) { m_Text = v; return *this; }
    _2DText &Alignment(int v) { m_Alignment = v; return *this; }
    _2DText &Margins(VxRect v) { m_Margins = v; return *this; }
    _2DText &Offset(Vx2DVector v) { m_Offset = v; return *this; }
    _2DText &ParagraphIndentation(Vx2DVector v) { m_ParagraphIndentation = v; return *this; }
    _2DText &BackgroundMaterial(CKMaterial* v) { m_BackgroundMaterial = v; return *this; }
    _2DText &CaretSize(float v) { m_CaretSize = v; return *this; }
    _2DText &CaretMaterial(CKMaterial* v) { m_CaretMaterial = v; return *this; }

    void Run(BehaviorCache &cache, CK2dEntity *target, int slot = 0) const {
        auto runner = cache.Get(Guid, true);
        Run(runner, target, slot);
    }

    void Run(BehaviorRunner &runner, CK2dEntity *target, int slot = 0) const {
        runner.Target(target);
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script, CK2dEntity *target) const {
        BehaviorFactory factory(script, Guid, true);
        factory.Target(target);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_Font)
          .Input(1, m_Text)
          .Input(2, m_Alignment)
          .Input(3, m_Margins)
          .Input(4, m_Offset)
          .Input(5, m_ParagraphIndentation)
          .Input(6, m_BackgroundMaterial)
          .Input(7, m_CaretSize)
          .Input(8, m_CaretMaterial);
    }

    int m_Font = 0;
    const char* m_Text = "";
    int m_Alignment = 0;
    VxRect m_Margins = VxRect();
    Vx2DVector m_Offset = Vx2DVector(0,0);
    Vx2DVector m_ParagraphIndentation = Vx2DVector(0,0);
    CKMaterial* m_BackgroundMaterial = nullptr;
    float m_CaretSize = 0.0f;
    CKMaterial* m_CaretMaterial = nullptr;
};

} // namespace bml::Interface

#endif // BML_BEHAVIORS_INTERFACE_H
