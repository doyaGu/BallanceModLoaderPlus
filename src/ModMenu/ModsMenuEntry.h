#ifndef BML_MODSMENUENTRY_H
#define BML_MODSMENUENTRY_H

#include <array>

#include "BML/Behavior.hpp"

class CKBehavior;
class CKContext;
class ModContext;
class Logger;

// Owns the "Mods" entry in Ballance's Options menu. The Behavior Patch and
// the visible menu row are one feature: the row is published only after
// the Patch is active, and is removed before the Patch is retired.
class ModsMenuEntry {
public:
    void Load(BML::Behavior::Session &behavior, CKBehavior *script, ModContext &bml,
              Logger &logger);
    void OnProcess();
    void Unload();

private:
    struct PendingLoad {
        BML::Behavior::Session *Behavior = nullptr;
        CK_ID Script = 0;
        ModContext *Bml = nullptr;
        Logger *Log = nullptr;
    };

    void BeginLoad(BML::Behavior::Session &behavior, CKBehavior *script, ModContext &bml,
                   Logger &logger);
    void ResumePendingLoad();
    void Publish();
    void Restore();
    void Fail(const BML::Behavior::Status &status);
    void Retire(bool deferCleanup);
    void HideButton();
    void Clear(bool destroyButton);

    CKContext *m_Context = nullptr;
    ModContext *m_BML = nullptr;
    Logger *m_Logger = nullptr;
    BML::Behavior::Patch m_Patch;
    std::array<CK_ID, 6> m_Items{};
    std::array<float, 6> m_OriginalY{};
    CK_ID m_ShowHide = 0;
    int m_ModsRow = -1;
    int m_BackRow = -1;
    bool m_Published = false;
    bool m_Retiring = false;
    bool m_CloseBlocked = false;
    bool m_CloseFailureReported = false;
    PendingLoad m_PendingLoad;
};

#endif // BML_MODSMENUENTRY_H
