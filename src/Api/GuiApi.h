#ifndef BML_API_GUIAPI_H
#define BML_API_GUIAPI_H

#include "BML/Gui.h"
#include "CKTypes.h"
#include <memory>
#include <cstdint>

class ModContext;
class ModRecord;

namespace BML::Api {
class GuiApi {
public:
    GuiApi();
    ~GuiApi();
    int Create(ModContext &, ModRecord &, uint32_t, const char *, BML_ObjectRef *);
    int Destroy(ModContext &, ModRecord &, BML_ObjectRef);
    int Read(ModContext &, ModRecord &, BML_ObjectRef, BML_GuiElementInfo *);
    int SetPosition(ModContext &, ModRecord &, BML_ObjectRef, BML_Vec2);
    int SetSize(ModContext &, ModRecord &, BML_ObjectRef, BML_Vec2);
    int SetZOrder(ModContext &, ModRecord &, BML_ObjectRef, int32_t);
    int SetVisible(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetActive(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetColor(ModContext &, ModRecord &, BML_ObjectRef, BML_Color);
    int SetText(ModContext &, ModRecord &, BML_ObjectRef, const char *);
    int GetText(ModContext &, ModRecord &, BML_ObjectRef, char *, size_t, size_t *);
    int SetFont(ModContext &, ModRecord &, BML_ObjectRef, const char *, int32_t, int32_t, uint32_t);
    int SetAlignment(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetButtonType(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetHovered(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetGameFont(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetTextFlags(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetOffset(ModContext &, ModRecord &, BML_ObjectRef, BML_Vec2);
    int SetFocused(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int SetKey(ModContext &, ModRecord &, BML_ObjectRef, uint32_t);
    int CharTyped(ModContext &, ModRecord &, BML_ObjectRef, uint32_t, BML_GuiEvent *);
    int Process(ModContext &, ModRecord &, BML_ObjectRef);
    void ObjectsToBeDeleted(const CK_ID *, int) noexcept;
    void ResetWorld() noexcept;
    void Collect() noexcept;
    bool CleanupOwner(ModRecord &) noexcept;

private:
    struct Record;
    struct Storage;
    Record *Find(ModContext &, ModRecord &, BML_ObjectRef);
    static void Sanitize(Record &, bool all = false) noexcept;
    std::unique_ptr<Storage> m_Storage;
};

const BML_GuiInterface &GuiInterface();
} // namespace BML::Api
#endif
