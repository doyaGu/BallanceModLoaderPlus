#include "ModMenu/ModsMenuEntry.h"

#include "BML/Guids/Interface.h"
#include "BML/Guids/TT_Toolbox_RT.h"
#include "BML/IBML.h"
#include "BML/ILogger.h"

#include "Loader/ModContext.h"

namespace Behavior = BML::Behavior;

namespace {

template <class T>
T *Live(CKContext *context, CK_ID id) {
    return context && id ? T::Cast(context->GetObject(id)) : nullptr;
}

const Behavior::Status &PatchFailure(const Behavior::PatchInfo &info) {
    if (info.ApplyFailure.Error != Behavior::Error::None)
        return info.ApplyFailure;
    if (info.RestoreFailure.Error != Behavior::Error::None)
        return info.RestoreFailure;
    return info.LastStatus;
}

int CountPorts(const Behavior::Node &node, Behavior::SlotKind kind) {
    int count = 0;
    for (const Behavior::Port port : node.Ports()) {
        if (port.Kind() == kind)
            ++count;
    }
    return count;
}

void OpenModsMenu() {
    BML_GetModContext()->OpenModsMenu();
}

Behavior::Result<Behavior::Node> FindFirstNeighbor(
    const Behavior::Graph &graph, std::string_view name,
    std::string_view adjacent, bool previous) {
    Behavior::Node match;
    for (const Behavior::Node node : graph.FindAll(name)) {
        bool found = false;
        for (const Behavior::Link link : graph.Links()) {
            const Behavior::Port endpoint = previous
                ? link.Target() : link.Source();
            if (endpoint.Node() != node.Id())
                continue;

            const std::uint64_t neighborId = previous
                ? link.Source().Node() : link.Target().Node();
            for (const Behavior::Node candidate : graph.Nodes()) {
                if (candidate.Id() == neighborId &&
                    candidate.Name() == adjacent) {
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found)
            continue;
        if (match) {
            Behavior::Status status;
            status.Error = Behavior::Error::QueryAmbiguous;
            status.Message = "More than one '" + std::string(name) +
                "' is adjacent to '" + std::string(adjacent) + "'.";
            return Behavior::Result<Behavior::Node>::Failure(
                BML_ERROR_INVALID_PARAMETER, std::move(status));
        }
        match = node;
    }
    if (match)
        return Behavior::Result<Behavior::Node>::Success(match);

    Behavior::Status status;
    status.Error = Behavior::Error::QueryNotFound;
    status.Message = "No '" + std::string(name) + "' is adjacent to '" +
        std::string(adjacent) + "'.";
    return Behavior::Result<Behavior::Node>::Failure(
        BML_ERROR_NOT_FOUND, std::move(status));
}

} // namespace

void ModsMenuEntry::Load(Behavior::Session &behavior, CKBehavior *script,
                         IBML &bml, ILogger &logger) {
    if (m_Patch || m_Retiring) {
        m_PendingLoad = {&behavior, script ? script->GetID() : 0,
                         &bml, &logger};
        m_CloseBlocked = false;
        Retire(true);
        return;
    }
    BeginLoad(behavior, script, bml, logger);
}

void ModsMenuEntry::BeginLoad(Behavior::Session &behavior,
                              CKBehavior *script, IBML &bml,
                              ILogger &logger) {
    m_Context = bml.GetCKContext();
    m_BML = &bml;
    m_Logger = &logger;

    logger.Info("Preparing the Mods entry in the Options menu");
    if (!behavior) {
        logger.Error("Cannot edit the Options menu without Behavior authoring");
        return;
    }

    auto outer = behavior.Inspect(script, Behavior::View::Logical);
    if (!outer) {
        logger.Error("Cannot inspect Menu_Options: %s",
                     outer.GetStatus().Message.empty()
                         ? "the script graph was not found"
                         : outer.GetStatus().Message.c_str());
        return;
    }
    auto optionsNode = outer->Find(Behavior::Named("Options Menu", 0));
    if (!optionsNode) {
        logger.Error("Cannot find Menu_Options/Options Menu: %s",
                     optionsNode.GetStatus().Message.empty()
                         ? "the graph was not found"
                         : optionsNode.GetStatus().Message.c_str());
        return;
    }
    auto options = outer->Inspect(optionsNode.Value());
    if (!options) {
        logger.Error("Cannot inspect Menu_Options/Options Menu: %s",
                     options.GetStatus().Message.empty()
                         ? "the graph was not found"
                         : options.GetStatus().Message.c_str());
        return;
    }

    auto textReference = options->Find(Behavior::Named("2D Text", 0));
    auto nopNode = options->Find(Behavior::Named("Nop", 0));
    auto keyboardNode = options->Find(Behavior::Named("Keyboard", 0));
    auto upSwitchNode = FindFirstNeighbor(
        options.Value(), "Switch On Parameter", "Set 2D Material", true);
    auto downSwitchNode = FindFirstNeighbor(
        options.Value(), "Switch On Parameter", "Send Message", true);
    auto upSelectorNode = FindFirstNeighbor(
        options.Value(), "Parameter Selector", "Keyboard", false);
    auto downSelectorNode = FindFirstNeighbor(
        options.Value(), "Parameter Selector", "Send Message", false);
    if (!upSwitchNode || !downSwitchNode ||
        !upSelectorNode || !downSelectorNode) {
        const Behavior::Status &status = !upSwitchNode
            ? upSwitchNode.GetStatus()
            : !downSwitchNode ? downSwitchNode.GetStatus()
            : !upSelectorNode ? upSelectorNode.GetStatus()
                             : downSelectorNode.GetStatus();
        logger.Error("Cannot identify the Options menu selection loop: %s",
                     status.Message.c_str());
        return;
    }
    if (!textReference || !nopNode || !keyboardNode) {
        logger.Error("Cannot find the Options menu presentation or keyboard reference");
        return;
    }

    const int upSwitchOutputs = CountPorts(
        upSwitchNode.Value(), Behavior::SlotKind::Out);
    const int downSwitchOutputs = CountPorts(
        downSwitchNode.Value(), Behavior::SlotKind::Out);
    const int upSelectorInputs = CountPorts(
        upSelectorNode.Value(), Behavior::SlotKind::In);
    const int downSelectorInputs = CountPorts(
        downSelectorNode.Value(), Behavior::SlotKind::In);
    const int menuOutputs = CountPorts(
        optionsNode.Value(), Behavior::SlotKind::Out);
    if (upSelectorInputs < 1 ||
        upSelectorInputs != downSelectorInputs ||
        menuOutputs != upSelectorInputs ||
        downSwitchOutputs != upSelectorInputs + 1 ||
        upSwitchOutputs != downSwitchOutputs + 1) {
        logger.Error("Cannot edit the Options menu because its selection interfaces are inconsistent");
        return;
    }

    m_ModsRow = upSelectorInputs - 1;
    m_BackRow = upSelectorInputs;
    const int previousBackBranch = downSwitchOutputs - 1;
    const int backBranch = upSwitchOutputs - 1;
    const int previousBackOutput = menuOutputs - 1;
    const std::string switchOutputName = "Out " +
        std::to_string(backBranch);
    const std::string switchPinName = "Pin " +
        std::to_string(backBranch);
    const std::string selectorInputName = "In " +
        std::to_string(m_BackRow);
    const std::string selectorPinName = "pIn " +
        std::to_string(m_BackRow);
    const std::string backOutputName = "Button " +
        std::to_string(menuOutputs + 1) + " Pressed";

    auto oldBackRoute = outer->Leaving(
        optionsNode->Out(previousBackOutput));
    auto nativeBackNode = outer->Next(optionsNode->Out(previousBackOutput));
    auto exitNode = nativeBackNode
        ? outer->Next(nativeBackNode->Out())
        : Behavior::Result<Behavior::Node>::Failure(BML_ERROR_NOT_FOUND);
    if (!oldBackRoute || !nativeBackNode || nativeBackNode->Name() != "back" ||
        !exitNode || exitNode->Name() != "Exit") {
        logger.Error("Cannot identify the Options menu Back route");
        return;
    }

    auto flags = options->Read(textReference->Setting(0));
    const auto *textFlags = flags
        ? std::get_if<std::int32_t>(&flags->Data) : nullptr;
    if (!textFlags) {
        logger.Error("Cannot read the Options menu text style");
        return;
    }

    CK2dEntity *items[6] = {nullptr};
    items[0] = bml.Get2dEntityByName("M_Options_Title");
    char name[] = "M_Options_But_X";
    for (int i = 1; i < 4; ++i) {
        name[14] = static_cast<char>('0' + i);
        items[i] = bml.Get2dEntityByName(name);
    }
    items[5] = bml.Get2dEntityByName("M_Options_But_Back");
    CKDataArray *showHide = bml.GetArrayByName("Menu_Options_ShowHide");
    if (!items[0] || !items[1] || !items[2] || !items[3] || !items[5] ||
        !showHide) {
        logger.Error("Cannot edit the Options menu because its presentation is incomplete");
        Clear(false);
        return;
    }

    items[4] = CK2dEntity::Cast(m_Context->CopyObject(items[1]));
    if (!items[4]) {
        logger.Error("Cannot create the Mods button entity");
        Clear(false);
        return;
    }
    items[4]->SetName("M_Options_But_4");
    for (int i = 0; i < 6; ++i) {
        m_Items[static_cast<std::size_t>(i)] = items[i]->GetID();
        Vx2DVector position;
        items[i]->GetPosition(position, TRUE);
        m_OriginalY[static_cast<std::size_t>(i)] = position.y;
    }
    m_ShowHide = showHide->GetID();

    auto button = behavior.Reference(items[4]);
    if (!button) {
        logger.Error("Cannot retain the Mods button identity: %s",
                     button.GetStatus().Message.c_str());
        Clear(true);
        return;
    }

    Behavior::Block text = behavior.Use(VT_INTERFACE_2DTEXT);
    text.Target(CKPGUID_2DENTITY, button.Value());
    text.Settings({{Behavior::At(0), Behavior::Value::As(
        CKPGUID_TEXTPROPERTIES, *textFlags)}});
    Behavior::Block pushButton = behavior.Use(TT_TOOLBOX_RT_TTPUSHBUTTON2);
    pushButton.Target(CKPGUID_2DENTITY, button.Value());

    Behavior::Edit edit;
    auto root = edit.Root();
    const auto optionsMenu = root.Require(optionsNode.Value());
    auto menu = optionsMenu.Graph();

    const auto upSwitch = menu.Require(upSwitchNode.Value());
    const auto downSwitch = menu.Require(downSwitchNode.Value());
    const auto upSelector = menu.Require(upSelectorNode.Value());
    const auto downSelector = menu.Require(downSelectorNode.Value());
    const auto textReferenceEdit = menu.Require(textReference.Value());
    const auto nop = menu.Require(nopNode.Value());

    const auto backRow = menu.AppendLocal("Back Row", CKPGUID_INT);
    menu.Bind(backRow, static_cast<std::int32_t>(m_BackRow));
    const auto downSwitchOut = menu.AppendOut(downSwitch, switchOutputName);
    menu.Bind(downSwitch.Pin(switchPinName, CKPGUID_INT), backRow);
    const auto upSelectorIn = menu.AppendIn(upSelector, selectorInputName);
    menu.Bind(upSelector.Pin(selectorPinName, CKPGUID_INT), backRow);
    const auto downSelectorIn = menu.AppendIn(downSelector, selectorInputName);
    menu.Bind(downSelector.Pin(selectorPinName, CKPGUID_INT), backRow);

    const auto textNode = menu.Add(text);
    const auto pushButtonNode = menu.Add(pushButton);
    menu.Share(textNode.Pin(0), textReferenceEdit.Pin(0));
    menu.Bind(textNode.Pin(1), "Mods");
    for (int i = 2; i < 6; ++i)
        menu.Share(textNode.Pin(i), textReferenceEdit.Pin(i));

    menu.Reconnect(menu.Leaving(upSwitch, previousBackBranch), upSwitch.Out(backBranch),
                   menu.Next(upSwitch, previousBackBranch).In(0));
    // The native Options menu is a same-frame interaction loop. These edges
    // add one more branch to that existing loop; confirm that intent at the
    // authoring site instead of weakening cycle validation globally.
    menu.FlowCycle(upSwitch.Out(previousBackBranch), textNode.In(0));
    menu.FlowCycle(textNode.Out(0), nop.In(0));
    menu.FlowCycle(textNode.Out(0), pushButtonNode.In(0));

    menu.Reconnect(menu.Entering(upSelector, m_ModsRow),
                   menu.Previous(upSelector, m_ModsRow).Out(1), upSelectorIn);
    menu.Reconnect(menu.Entering(downSelector, m_ModsRow),
                   menu.Previous(downSelector, m_ModsRow).Out(2), downSelectorIn);
    menu.FlowCycle(pushButtonNode.Out(1), upSelector.In(m_ModsRow));
    menu.FlowCycle(pushButtonNode.Out(2), downSelector.In(m_ModsRow));

    const auto back = menu.AppendOut(backOutputName);
    menu.Flow(downSwitchOut, back);

    const auto oldBack = root.Require(oldBackRoute.Value());
    const auto nativeBack = root.Require(nativeBackNode.Value());
    const auto exit = root.Require(exitNode.Value());
    root.Reconnect(oldBack, optionsMenu.Out(backOutputName),
                   nativeBack.In(0));
    root.Flow(optionsMenu.Out(previousBackOutput), Behavior::Hook(&OpenModsMenu),
              exit.In(0));

    auto keyboard = menu.Require(keyboardNode.Value()).Graph();
    Behavior::NodePattern escapeKey("Secure Key");
    escapeKey.Pin(0, Behavior::Value::As(
        CKPGUID_KEY, static_cast<std::int32_t>(CKKEY_ESCAPE)));
    const auto secureKey = keyboard.Require(std::move(escapeKey));
    const auto selection = keyboard.Next(secureKey);
    keyboard.Set(selection.Pin(0, CKPGUID_INT),
                 static_cast<std::int32_t>(m_BackRow));

    auto applied = outer->Apply("Mods button", edit);
    if (!applied) {
        logger.Error("Cannot apply the Mods button Patch: %s",
                     applied.GetStatus().Message.empty()
                         ? "Behavior Patch creation failed"
                         : applied.GetStatus().Message.c_str());
        Clear(true);
        return;
    }
    m_Patch = applied.Take();
    OnProcess();
}

void ModsMenuEntry::OnProcess() {
    if (m_Retiring) {
        if (m_CloseBlocked)
            return;
        const auto closed = m_Patch.Close();
        if (!closed) {
            if (!m_CloseFailureReported && m_Logger) {
                m_Logger->Error(
                    "The Mods menu Patch could not be restored: %s",
                    closed.GetStatus().Message.empty()
                        ? "Behavior Patch close failed"
                        : closed.GetStatus().Message.c_str());
                m_CloseFailureReported = true;
            }
            m_CloseBlocked = true;
            return;
        }
        if (closed.Value() == Behavior::CloseState::Closed)
            ResumePendingLoad();
        return;
    }
    if (!m_Patch || m_Published)
        return;

    auto info = m_Patch.Info();
    if (!info) {
        Fail(info.GetStatus());
        return;
    }
    switch (info->State) {
    case Behavior::PatchState::Pending:
        return;
    case Behavior::PatchState::Active:
        Publish();
        return;
    case Behavior::PatchState::Failed:
    case Behavior::PatchState::Conflicted:
    case Behavior::PatchState::Closed:
        Fail(PatchFailure(info.Value()));
        return;
    case Behavior::PatchState::Disabled:
    case Behavior::PatchState::Closing:
        return;
    }
}

void ModsMenuEntry::Publish() {
    std::array<CK2dEntity *, 6> items{};
    for (std::size_t i = 0; i < items.size(); ++i) {
        items[i] = Live<CK2dEntity>(m_Context, m_Items[i]);
        if (!items[i]) {
            Behavior::Status status;
            status.Error = Behavior::Error::Unavailable;
            status.Message = "An Options menu entity disappeared before the Patch became active.";
            Fail(status);
            return;
        }
    }
    CKDataArray *showHide = Live<CKDataArray>(m_Context, m_ShowHide);
    if (!showHide || m_ModsRow < 0 || m_BackRow <= 0 ||
        !showHide->InsertRow(m_ModsRow)) {
        Behavior::Status status;
        status.Error = Behavior::Error::Unavailable;
        status.Message = "Menu_Options_ShowHide cannot publish the Mods entry.";
        Fail(status);
        return;
    }

    CKBOOL visible = TRUE;
    if (!showHide->SetElementObject(m_ModsRow, 0, items[4]) ||
        !showHide->SetElementValue(m_ModsRow, 1, &visible, sizeof(visible))) {
        showHide->RemoveRow(m_ModsRow);
        Behavior::Status status;
        status.Error = Behavior::Error::Unavailable;
        status.Message = "Menu_Options_ShowHide rejected the Mods entry.";
        Fail(status);
        return;
    }

    Vx2DVector first;
    Vx2DVector back;
    items[1]->GetPosition(first, TRUE);
    items[5]->GetPosition(back, TRUE);
    const float rowStep = (back.y - first.y) /
        static_cast<float>(m_BackRow);
    for (int i = 1; i <= 5; ++i) {
        Vx2DVector position;
        items[i]->GetPosition(position, TRUE);
        position.y = first.y + rowStep * static_cast<float>(i - 1);
        items[i]->SetPosition(position, TRUE);
    }
    m_BML->SetIC(showHide);
    m_Published = true;
    m_Logger->Info("Mods Button inserted");
}

void ModsMenuEntry::Restore() {
    if (!m_Published)
        return;

    CK2dEntity *button = Live<CK2dEntity>(m_Context, m_Items[4]);
    for (int i : {1, 2, 3, 5}) {
        if (CK2dEntity *item = Live<CK2dEntity>(m_Context, m_Items[i])) {
            Vx2DVector position;
            item->GetPosition(position, TRUE);
            position.y = m_OriginalY[static_cast<std::size_t>(i)];
            item->SetPosition(position, TRUE);
        }
    }
    if (CKDataArray *showHide = Live<CKDataArray>(m_Context, m_ShowHide)) {
        for (int row = 0; row < showHide->GetRowCount(); ++row) {
            if (showHide->GetElementObject(row, 0) != button)
                continue;
            showHide->RemoveRow(row);
            m_BML->SetIC(showHide);
            break;
        }
    }
    m_Published = false;
}

void ModsMenuEntry::Fail(const Behavior::Status &status) {
    if (m_Logger) {
        m_Logger->Error("The Mods menu entry was not installed: %s",
                        status.Message.empty()
                            ? "the Behavior Patch failed"
                            : status.Message.c_str());
    }
    Retire(true);
}

void ModsMenuEntry::Unload() {
    m_PendingLoad = {};
    Retire(false);
}

void ModsMenuEntry::Retire(bool deferCleanup) {
    Restore();
    if (!m_Patch) {
        ResumePendingLoad();
        return;
    }

    const auto closed = m_Patch.Close();
    const bool canDestroy = closed &&
        closed.Value() == Behavior::CloseState::Closed;
    if (canDestroy || !deferCleanup) {
        if (!canDestroy && !closed && m_Logger) {
            m_Logger->Error(
                "The Mods menu Patch could not be restored during unload: %s",
                closed.GetStatus().Message.empty()
                    ? "Behavior Patch close failed"
                    : closed.GetStatus().Message.c_str());
        }
        if (canDestroy)
            ResumePendingLoad();
        else
            Clear(false);
        return;
    }

    HideButton();
    m_Retiring = true;
    m_CloseBlocked = !closed;
    if (!closed && !m_CloseFailureReported && m_Logger) {
        m_Logger->Error(
            "The Mods menu Patch could not be restored: %s",
            closed.GetStatus().Message.empty()
                ? "Behavior Patch close failed"
                : closed.GetStatus().Message.c_str());
        m_CloseFailureReported = true;
    }
}

void ModsMenuEntry::ResumePendingLoad() {
    const PendingLoad pending = m_PendingLoad;
    m_PendingLoad = {};
    Clear(true);
    if (!pending.Behavior || !pending.Script ||
        !pending.Bml || !pending.Logger) {
        return;
    }

    CKContext *context = pending.Bml->GetCKContext();
    CKBehavior *script = Live<CKBehavior>(context, pending.Script);
    if (!script) {
        pending.Logger->Error(
            "Cannot replace the Mods menu Patch because Menu_Options is no longer live");
        return;
    }
    BeginLoad(*pending.Behavior, script, *pending.Bml, *pending.Logger);
}

void ModsMenuEntry::HideButton() {
    if (CK2dEntity *button = Live<CK2dEntity>(m_Context, m_Items[4]))
        button->Show(CKHIDE);
}

void ModsMenuEntry::Clear(bool destroyButton) {
    if (destroyButton) {
        if (CK2dEntity *button = Live<CK2dEntity>(m_Context, m_Items[4]))
            m_Context->DestroyObject(button);
    } else {
        HideButton();
    }
    m_Patch = {};
    m_Items = {};
    m_OriginalY = {};
    m_ShowHide = 0;
    m_ModsRow = -1;
    m_BackRow = -1;
    m_Published = false;
    m_Retiring = false;
    m_CloseBlocked = false;
    m_CloseFailureReported = false;
    m_Context = nullptr;
    m_BML = nullptr;
    m_Logger = nullptr;
}
