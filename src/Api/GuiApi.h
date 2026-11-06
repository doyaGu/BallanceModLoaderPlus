#ifndef BML_API_GUIAPI_H
#define BML_API_GUIAPI_H
#include "BML/Gui.h"
#include <memory>
#include <cstdint>
class ModContext;
class IMod;
namespace BML::Api {
class GuiApi {
  public:
    GuiApi();
    ~GuiApi();
    int Create(ModContext &, IMod &, uint32_t, const char *, BML_ObjectRef *);
    int Destroy(ModContext &, IMod &, BML_ObjectRef);
    int Read(ModContext &, IMod &, BML_ObjectRef, BML_GuiElementInfo *);
    int Position(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int Size(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int ZOrder(ModContext &, IMod &, BML_ObjectRef, int32_t);
    int Visible(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int Active(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int Color(ModContext &, IMod &, BML_ObjectRef, BML_Color);
    int Text(ModContext &, IMod &, BML_ObjectRef, const char *);
    int GetText(ModContext &, IMod &, BML_ObjectRef, char *, size_t, size_t *);
    int Font(ModContext &, IMod &, BML_ObjectRef, const char *, int32_t, int32_t, uint32_t);
    int Alignment(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int ButtonType(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int Hovered(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int GameFont(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int TextFlags(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int Offset(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int Focused(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int Key(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int CharTyped(ModContext &, IMod &, BML_ObjectRef, uint32_t, BML_GuiEvent *);
    int Process(ModContext &, IMod &, BML_ObjectRef);
    void ObjectsToBeDeleted(const uint32_t *, int) noexcept;
    void ResetWorld() noexcept;
    void Collect() noexcept;
    bool CleanupOwner(IMod &) noexcept;

  private:
    struct Record;
    struct Storage;
    Record *Find(ModContext &, IMod &, BML_ObjectRef);
    static void Sanitize(Record &, bool all = false) noexcept;
    std::unique_ptr<Storage> m_Storage;
};
const BML_GuiInterface &GuiInterface();
} // namespace BML::Api
#endif
