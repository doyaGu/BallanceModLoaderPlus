#ifndef BML_MOD_MENU_PRESENTATION_H
#define BML_MOD_MENU_PRESENTATION_H

#include <memory>
#include <string>

#include "ModMenu/ModMenuSession.h"

class ModMenuModel;

enum class ModMenuRouteAction {
    None,
    Back,
    Close,
    OpenDetails,
    OpenSettings,
    OpenPage,
};

enum class ModMenuPageStatus {
    Ready,
    EnterFailed,
    DrawFailed,
    Unavailable,
};

struct ModMenuPagePresentationResult {
    ModMenuPageNavigation navigation;
    ModMenuPageStatus status = ModMenuPageStatus::Ready;
};

class ModMenuPresentation final {
public:
    ModMenuPresentation();
    ~ModMenuPresentation();

    ModMenuPresentation(const ModMenuPresentation &) = delete;
    ModMenuPresentation &operator=(const ModMenuPresentation &) = delete;

    void Reset();
    ModMenuRouteAction DrawModListPage(ModMenuModel &model);
    ModMenuRouteAction DrawDetailsPage(ModMenuModel &model);
    ModMenuRouteAction DrawSettingsPage(ModMenuModel &model);
    ModMenuPagePresentationResult DrawPage(
        ModMenuModel &model, const ModMenuOwner &owner,
        const ModMenuPageInfo &page, ModMenuPageStatus status,
        const std::string &notice);

private:
    struct State;
    std::unique_ptr<State> m_State;
};

#endif // BML_MOD_MENU_PRESENTATION_H
