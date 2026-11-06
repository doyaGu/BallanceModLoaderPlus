#ifndef BML_GUI_ENGINE_HPP
#define BML_GUI_ENGINE_HPP
#include "BML/Gui.hpp"
#include "BML/Engine.hpp"
namespace BGui {
inline Vx2DVector GetPosition(Element &e) {
    auto v = e.GetPosition();
    return {v.x, v.y};
}
inline void SetPosition(Element &e, Vx2DVector v) { e.SetPosition({v.x, v.y}); }
inline Vx2DVector GetSize(Element &e) {
    auto v = e.GetSize();
    return {v.x, v.y};
}
inline void SetSize(Element &e, Vx2DVector v) { e.SetSize({v.x, v.y}); }
inline Vx2DVector GetPosition(Text &e) {
    auto v = e.GetPosition();
    return {v.x, v.y};
}
inline void SetPosition(Text &e, Vx2DVector v) { e.SetPosition({v.x, v.y}); }
inline Vx2DVector GetSize(Text &e) {
    auto v = e.GetSize();
    return {v.x, v.y};
}
inline void SetSize(Text &e, Vx2DVector v) { e.SetSize({v.x, v.y}); }
inline VxColor GetColor(Panel &e) {
    auto c = e.GetColor();
    return {c.r, c.g, c.b, c.a};
}
inline void SetColor(Panel &e, VxColor c) { e.SetColor({c.r, c.g, c.b, c.a}); }
inline void SetOffset(Label &e, Vx2DVector v) { e.SetOffset({v.x, v.y}); }
inline void OnCharTyped(Input &e, CKDWORD key) { e.OnCharTyped(static_cast<uint32_t>(key)); }
inline CKKEYBOARD GetKey(KeyInput &e) { return static_cast<CKKEYBOARD>(e.GetKey()); }
inline void SetKey(KeyInput &e, CKKEYBOARD key) { e.SetKey(static_cast<uint32_t>(key)); }
} // namespace BGui
#endif
