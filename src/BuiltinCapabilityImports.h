#ifndef BML_BUILTINCAPABILITYIMPORTS_H
#define BML_BUILTINCAPABILITYIMPORTS_H

#include "BML/Import.h"

class BuiltinCapabilityImports {
public:
    void SendIngameMessage(const char *message);
    void ClearIngameMessages();

    void OpenModsMenu();
    void CloseModsMenu();
    void OpenMapMenu();
    void CloseMapMenu();

    int GetHUD();
    void SetHUD(int mode);
    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);
    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime();

private:
    template <typename ImportType>
    static void BindIfNeeded(ImportType &import, const char *name, const char *signature) {
        if (!import.IsBound())
            import.Bind("BML", name, signature);
    }

    BML::Import<void(const char *)> m_SendMessage;
    BML::Import<void()> m_ClearMessages;

    BML::Import<void()> m_OpenModsMenu;
    BML::Import<void()> m_CloseModsMenu;
    BML::Import<void()> m_OpenMapMenu;
    BML::Import<void()> m_CloseMapMenu;

    BML::Import<int()> m_GetHUD;
    BML::Import<void(int)> m_SetHUD;
    BML::Import<void(bool)> m_ShowTitle;
    BML::Import<void(bool)> m_ShowFPS;
    BML::Import<void(bool)> m_ShowSRTimer;
    BML::Import<void()> m_StartSRTimer;
    BML::Import<void()> m_PauseSRTimer;
    BML::Import<void()> m_ResetSRTimer;
    BML::Import<float()> m_GetSRTime;
};

#endif
