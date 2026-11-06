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
    int SetPosition(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int SetSize(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int SetZOrder(ModContext &, IMod &, BML_ObjectRef, int32_t);
    int SetVisible(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetActive(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetColor(ModContext &, IMod &, BML_ObjectRef, BML_Color);
    int SetText(ModContext &, IMod &, BML_ObjectRef, const char *);
    int GetText(ModContext &, IMod &, BML_ObjectRef, char *, size_t, size_t *);
    int SetFont(ModContext &, IMod &, BML_ObjectRef, const char *, int32_t, int32_t, uint32_t);
    int SetAlignment(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetButtonType(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetHovered(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetGameFont(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetTextFlags(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetOffset(ModContext &, IMod &, BML_ObjectRef, BML_Vec2);
    int SetFocused(ModContext &, IMod &, BML_ObjectRef, uint32_t);
    int SetKey(ModContext &, IMod &, BML_ObjectRef, uint32_t);
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
