#ifndef BML_MOD_MENU_PRESENTATION_H
#define BML_MOD_MENU_PRESENTATION_H

#include <memory>

#include "BML/ModMenu.h"

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
    ModMenuRouteAction action = ModMenuRouteAction::None;
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
        ModMenuModel &model, ModMenuPageStatus status);

private:
    struct State;
    std::unique_ptr<State> m_State;
};

#endif // BML_MOD_MENU_PRESENTATION_H
