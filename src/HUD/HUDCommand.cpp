#include "HUD/HUDCommand.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "BML/IBML.h"
#include "Console/Shell/ShellIo.h"
#include "HUD/HUDRuntime.h"
#include "UI/AnsiPalette.h"
#include "StringUtils.h"

HUDCommand::HUDCommand(HUDRuntime *hud) : m_HUD(hud) {
    m_State = m_HUD->GetMode();
}

static AnchorPoint ParseAnchor(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s);
    ok = true;
    if (t == "tl" || t == "topleft") return AnchorPoint::TopLeft;
    if (t == "tc" || t == "topcenter") return AnchorPoint::TopCenter;
    if (t == "tr" || t == "topright") return AnchorPoint::TopRight;
    if (t == "ml" || t == "middleleft") return AnchorPoint::MiddleLeft;
    if (t == "mc" || t == "middlecenter" || t == "center") return AnchorPoint::MiddleCenter;
    if (t == "mr" || t == "middleright") return AnchorPoint::MiddleRight;
    if (t == "bl" || t == "bottomleft") return AnchorPoint::BottomLeft;
    if (t == "bc" || t == "bottomcenter") return AnchorPoint::BottomCenter;
    if (t == "br" || t == "bottomright") return AnchorPoint::BottomRight;
    ok = false; return AnchorPoint::TopLeft;
}

static bool ParseColor(const std::string &s, ImU32 &out) {
    std::string v = utils::TrimStringCopy(s);
    if (!v.empty() && v[0] == '#') {
        std::string hex = v.substr(1);
        out = AnsiPalette::HexToImU32(hex.c_str());
        return true;
    }
    // r,g,b[,a]
    int r = 0, g = 0, b = 0, a = 255; int n = 0;
    char c;
    std::stringstream ss(v);
    if (ss >> r) { ++n; if (ss >> c && (c == ',' || c == ';' || c == ' ')) {} if (ss >> g) { ++n; if (ss >> c) {} if (ss >> b) { ++n; if (ss >> c) {} if (ss >> a) ++n; } } }
    if (n >= 3) { out = IM_COL32(std::clamp(r,0,255), std::clamp(g,0,255), std::clamp(b,0,255), std::clamp(a,0,255)); return true; }
    return false;
}

static AlignX ParseAlignX(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "left" || t == "l") return AlignX::Left;
    if (t == "center" || t == "c" || t == "middle" || t == "m") return AlignX::Center;
    if (t == "right" || t == "r") return AlignX::Right;
    ok = false;
    return AlignX::Left;
}
static AlignY ParseAlignY(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "top" || t == "t") return AlignY::Top;
    if (t == "middle" || t == "center" || t == "m" || t == "c") return AlignY::Middle;
    if (t == "bottom" || t == "b") return AlignY::Bottom;
    ok = false;
    return AlignY::Top;
}

// The shell already removed quotes and resolved escapes, so the text of a
// multi-word argument is simply the remaining words joined by single spaces.
static std::string JoinArguments(const std::vector<std::string> &args, size_t start) {
    return utils::JoinString(args, ' ', start);
}

void HUDCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    // Basic toggles preserved
    if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
        if (ParseBoolean(args[1])) {
            m_HUD->SetMode(m_State);
        } else {
            m_State = m_HUD->GetMode();
            m_HUD->SetMode(0);
        }
        return;
    }
    if (args.size() == 3 && (args[1] == "title" || args[1] == "fps" || args[1] == "sr")) {
        int state = m_HUD->GetMode();
        const bool on = ParseBoolean(args[2]);
        if (args[1] == "title") state = on ? (state | HUD_TITLE) : (state & ~HUD_TITLE);
        else if (args[1] == "fps") state = on ? (state | HUD_FPS) : (state & ~HUD_FPS);
        else if (args[1] == "sr") state = on ? (state | HUD_SR) : (state & ~HUD_SR);
        m_HUD->SetMode(state);
        return;
    }

    // TUI-like custom text management
    HUD &hud = m_HUD->GetWindow();
    if (args.size() >= 3 && args[1] == "add") {
        const std::string &id = args[2];
        size_t dot = id.find('.');
        if (dot == std::string::npos) {
            auto e = hud.GetOrCreate(id);
            if (args.size() >= 4) {
                std::string txt = JoinArguments(args, 3);
                if (auto textElement = HUDCast<HUDText>(e)) {
                    textElement->SetText(txt.c_str());
                }
            }
            bml->SendIngameMessage(("[hud] added '" + id + "'\n").c_str());
        } else {
            std::string contId = id.substr(0, dot);
            std::string childId = id.substr(dot + 1);
            auto child = hud.GetOrCreateChild(contId, childId);
            if (!child) { BML::Shell::Fail(bml, "[hud] container not found\n"); return; }
            if (args.size() >= 4) {
                std::string txt = JoinArguments(args, 3);
                if (auto textElement = HUDCast<HUDText>(child)) {
                    textElement->SetText(txt.c_str());
                }
            }
            bml->SendIngameMessage(("[hud] added child '" + childId + "' in '" + contId + "'\n").c_str());
        }
        return;
    }
    if (args.size() >= 3 && (args[1] == "vstack" || args[1] == "hstack")) {
        const std::string &id = args[2];
        size_t lastDot = id.find_last_of('.');
        std::shared_ptr<HUDContainer> c = nullptr;
        if (lastDot == std::string::npos) {
            c = (args[1] == std::string("vstack")) ? hud.AddVStack(id) : hud.AddHStack(id);
        } else {
            std::string parentPath = id.substr(0, lastDot);
            std::string childName = id.substr(lastDot + 1);
            auto pe = hud.FindByPath(parentPath);
            auto pc = HUDCast<HUDContainer>(pe);
            if (!pc) { BML::Shell::Fail(bml, "[hud] parent container not found\n"); return; }
            HUDLayoutKind kind = (args[1] == std::string("hstack")) ? HUDLayoutKind::Horizontal : HUDLayoutKind::Vertical;
            c = pc->AddContainerChild(kind, childName, 1);
        }
        bml->SendIngameMessage(((std::string("[hud] ") + args[1] + " '") + id + "' created\n").c_str());
        return;
    }
    if (args.size() >= 4 && args[1] == "grid") {
        const std::string &id = args[2]; int cols = std::max(1, ParseInteger(args[3]));
        size_t lastDot = id.find_last_of('.'); std::shared_ptr<HUDContainer> c = nullptr;
        if (lastDot == std::string::npos) {
            c = hud.AddGrid(id, cols);
        } else {
            std::string parentPath = id.substr(0, lastDot);
            std::string childName = id.substr(lastDot + 1);
            auto pe = hud.FindByPath(parentPath);
            auto pc = HUDCast<HUDContainer>(pe);
            if (!pc) { BML::Shell::Fail(bml, "[hud] parent container not found\n"); return; }
            c = pc->AddContainerChild(HUDLayoutKind::Grid, childName, cols);
        }
        bml->SendIngameMessage(("[hud] grid '" + id + "' created\n").c_str());
        return;
    }
    if (args.size() >= 5 && args[1] == "child" && args[2] == std::string("add")) {
        const std::string &containerId = args[3];
        const std::string &childId = args[4];
        auto cont = (containerId.find('.') == std::string::npos) ? hud.Find(containerId) : hud.FindByPath(containerId);
        auto c = HUDCast<HUDContainer>(cont);
        if (!c) { BML::Shell::Fail(bml, "[hud] container not found\n"); return; }
        std::string txt = JoinArguments(args, 5);
        auto child = c->AddChildNamed(childId, "");
        if (!txt.empty()) {
            if (child) {
                child->SetText(txt.c_str());
            }
        }
        // no global registry for child id to avoid conflicts; future work could add nested naming
        bml->SendIngameMessage("[hud] child added\n");
        return;
    }
    if (args.size() >= 4 && args[1] == "mv") {
        const std::string &src = args[2];
        const std::string &dst = args[3];
        std::string newName = (args.size() >= 5) ? args[4] : std::string();
        auto dest = hud.EnsureContainerPath(dst, HUDLayoutKind::Vertical);
        if (!dest) { BML::Shell::Fail(bml, "[hud] invalid destination\n"); return; }
        auto up = hud.StealByPath(src);
        if (!up) { BML::Shell::Fail(bml, "[hud] source not found\n"); return; }
        // If no newName provided, derive from src last segment
        if (newName.empty()) {
            size_t dot = src.find_last_of('.');
            newName = (dot == std::string::npos) ? src : src.substr(dot + 1);
        }
        hud.AttachToContainer(dest, std::move(up), newName);
        bml->SendIngameMessage("[hud] moved\n");
        return;
    }
    if (args.size() >= 4 && args[1] == "rename") {
        const std::string &path = args[2];
        const std::string &newName = args[3];
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos) {
            // root-level
            auto e = hud.Find(path);
            if (!e) { BML::Shell::Fail(bml, "[hud] not found\n"); return; }
            // Update global name mapping
            // Remove old
            // Simple: steal and reattach to root with new name
            auto up = hud.StealByPath(path);
            if (!up) { BML::Shell::Fail(bml, "[hud] rename failed\n"); return; }
            hud.AttachToRoot(std::move(up), newName);
            bml->SendIngameMessage("[hud] renamed\n");
        } else {
            std::string parentPath = path.substr(0, dot);
            std::string oldName = path.substr(dot + 1);
            auto pe = hud.FindByPath(parentPath);
            auto pc = HUDCast<HUDContainer>(pe);
            if (!pc) { BML::Shell::Fail(bml, "[hud] parent not found\n"); return; }
            // Re-map
            auto up = pc->StealChild(oldName);
            if (!up) { BML::Shell::Fail(bml, "[hud] child not found\n"); return; }
            pc->InsertChild(std::move(up), newName);
            bml->SendIngameMessage("[hud] renamed\n");
        }
        return;
    }
    if (args.size() >= 4 && (args[1] == "cp" || args[1] == "copy")) {
        const std::string &src = args[2];
        const std::string &dst = args[3];
        std::string newName = (args.size() >= 5) ? args[4] : std::string();
        auto srcE = hud.FindByPath(src);
        if (!srcE) { BML::Shell::Fail(bml, "[hud] source not found\n"); return; }
        auto dest = hud.EnsureContainerPath(dst, HUDLayoutKind::Vertical);
        if (!dest) { BML::Shell::Fail(bml, "[hud] invalid destination\n"); return; }
        auto up = hud.CloneElement(srcE);
        if (!up) { BML::Shell::Fail(bml, "[hud] copy failed\n"); return; }
        if (newName.empty()) { size_t dot = src.find_last_of('.'); newName = (dot == std::string::npos) ? src : src.substr(dot + 1); }
        hud.AttachToContainer(dest, std::move(up), newName);
        bml->SendIngameMessage("[hud] copied\n");
        return;
    }
    if (args.size() >= 3 && args[1] == "clone") {
        const std::string &src = args[2];
        std::string newName = (args.size() >= 4) ? args[3] : std::string();
        auto srcE = hud.FindByPath(src);
        if (!srcE) { BML::Shell::Fail(bml, "[hud] source not found\n"); return; }
        auto up = hud.CloneElement(srcE);
        if (!up) { BML::Shell::Fail(bml, "[hud] clone failed\n"); return; }
        if (newName.empty()) { size_t dot = src.find_last_of('.'); newName = (dot == std::string::npos) ? src : src.substr(dot + 1); }
        // If active page is set and default container mapping exists, attach under it; else root
        if (!hud.GetActivePage().empty()) {
            const std::string &pc = hud.GetPageDefaultContainer(hud.GetActivePage());
            if (!pc.empty()) {
                auto dest = hud.EnsureContainerPath(pc, HUDLayoutKind::Vertical);
                if (dest) { hud.AttachToContainer(dest, std::move(up), newName); bml->SendIngameMessage("[hud] cloned\n"); return; }
            }
            // fallback: set page and attach to root
            up->SetPage(hud.GetActivePage());
        }
        hud.AttachToRoot(std::move(up), newName);
        bml->SendIngameMessage("[hud] cloned\n");
        return;
    }
    if (args.size() >= 4 && args[1] == "page" && args[2] == "container") {
        if (args.size() == 4 && utils::ToLower(args[3]) == "show") {
            auto all = hud.ListPageDefaultContainers();
            if (all.empty()) { bml->SendIngameMessage("[hud] page containers: <none>\n"); return; }
            std::string line = "[hud] page containers:";
            for (const auto &kv : all) { line += " "; line += kv.first; line += "=\""; line += kv.second; line += "\""; }
            line += "\n"; bml->SendIngameMessage(line.c_str());
            return;
        }
        const std::string &page = args[3];
        if (args.size() >= 5) {
            const std::string &path = args[4];
            if (utils::ToLower(path) == "none") hud.ClearPageDefaultContainer(page);
            else hud.SetPageDefaultContainer(page, path);
        }
        const std::string &pc = hud.GetPageDefaultContainer(page);
        std::string msg = "[hud] page container for '" + page + "' = ";
        msg += pc.empty()?"<none>":pc; msg += "\n"; bml->SendIngameMessage(msg.c_str());
        return;
    }
    if ((args.size() == 2 && args[1] == "policy") || (args.size() >= 3 && args[1] == "policy")) {
        if (args.size() == 2) {
            std::string cur = hud.GetAutoCreatePolicyModeEffective();
            bml->SendIngameMessage((std::string("[hud] policy ") + cur + "; usage: hud policy <builtin|vertical|horizontal|grid|clear|reset|show|help>\n").c_str());
            return;
        } else {
            std::string mode = utils::ToLower(args[2]);
            if (mode == "reset") mode = "builtin"; // alias
            if (mode == "help") {
                bml->SendIngameMessage("[hud] policy inference (builtin):\n");
                bml->SendIngameMessage("  segment contains 'grid'                -> Grid\n");
                bml->SendIngameMessage("  'hstack'/'hbox'/'row'/'horiz'         -> Horizontal\n");
                bml->SendIngameMessage("  'vstack'/'vbox'/'col'                 -> Vertical\n");
                bml->SendIngameMessage("Use 'hud policy vertical|horizontal|grid|builtin|clear' to override.\n");
                return;
            }
            if (mode == "show") {
                std::string cur = hud.GetAutoCreatePolicyModeEffective();
                bml->SendIngameMessage((std::string("[hud] policy ") + cur + "\n").c_str());
                return;
            }
            hud.SetAutoCreatePolicyMode(mode);
            bml->SendIngameMessage((std::string("[hud] policy ") + mode + "\n").c_str());
            return;
        }
    }
    if (args.size() == 2 && args[1] == "help") {
        bml->SendIngameMessage("HUD commands:\n");
        bml->SendIngameMessage("  hud on|off                        : toggle HUD\n");
        bml->SendIngameMessage("  hud title|fps|sr on|off          : toggle built-ins\n");
        bml->SendIngameMessage("  hud add <id> [ansi_text]         : add/update element\n");
        bml->SendIngameMessage("  hud text <id> <ansi_text>        : set element text\n");
        bml->SendIngameMessage("  hud del <id> [--yes]             : delete element/container (--yes for non-empty container)\n");
        bml->SendIngameMessage("  hud list                          : list element ids\n");
        bml->SendIngameMessage("  hud get <id>                      : show basic info\n");
        bml->SendIngameMessage("  hud set <id> pos <anchor> [x y]   : set position\n");
        bml->SendIngameMessage("  hud set <id> scale <f>|visible on|off|wrap_px <px>|wrap_frac <f>|tabs <n>\n");
        bml->SendIngameMessage("  hud set <id> panel on|off|panel_bg <col>|panel_border <col>|padding <px>|border_thickness <px>|rounding <px>\n");
        bml->SendIngameMessage("  hud set <id> page <name>\n");
        bml->SendIngameMessage("  hud vstack|hstack <id>            : create container\n");
        bml->SendIngameMessage("  hud grid <id> <cols>              : create grid container\n");
        bml->SendIngameMessage("  hud child add <container> <child> [ansi_text]\n");
        bml->SendIngameMessage("  hud set <container> spacing <px>|cols <n>|align_x <l|c|r>|align_y <t|m|b>|cell_align_x <...>|cell_align_y <...>\n");
        bml->SendIngameMessage("  hud mv <src> <dstContainer> [new] : move element/container under container\n");
        bml->SendIngameMessage("  hud rename <path> <newName>       : rename element/container\n");
        bml->SendIngameMessage("  hud cp <src> <dstContainer> [new] : copy element/container under container\n");
        bml->SendIngameMessage("  hud clone <src> [new]             : clone to root or active page container\n");
        bml->SendIngameMessage("  hud policy <builtin|vertical|horizontal|grid|clear|reset|show|help> : set/show/describe policy\n");
        bml->SendIngameMessage("  hud page container <page> <path|none>|show : set/show page container mapping\n");
        return;
    }
    if (args.size() == 2 && args[1] == "page") {
        std::string cur = hud.GetActivePage(); if (cur.empty()) cur = "<all>";
        bml->SendIngameMessage((std::string("[hud] active page = ") + cur + "\n").c_str());
        return;
    }
    if (args.size() >= 3 && args[1] == "page") {
        const std::string &name = args[2];
        hud.SetActivePage(name);
        bml->SendIngameMessage((std::string("[hud] active page = ") + (name.empty()?"<all>":name) + "\n").c_str());
        return;
    }
    if (args.size() >= 3 && args[1] == "text") {
        const std::string &id = args[2];
        auto e = (id.find('.') == std::string::npos) ? hud.GetOrCreate(id) : hud.FindByPath(id);
        if (!e) { BML::Shell::Fail(bml, "[hud] not found\n"); return; }
        std::string txt = JoinArguments(args, 3);
        if (auto textElement = HUDCast<HUDText>(e)) {
            textElement->SetText(txt.c_str());
        }
        return;
    }
    if (args.size() >= 3 && args[1] == "del") {
        const std::string &id = args[2];
        auto e = (id.find('.') == std::string::npos) ? hud.Find(id) : hud.FindByPath(id);
        if (!e) { BML::Shell::Fail(bml, "[hud] not found\n"); return; }
        auto c = HUDCast<HUDContainer>(e);
        if (c) {
            bool confirmed = false;
            for (size_t i = 3; i < args.size(); ++i) {
                std::string f = utils::ToLower(args[i]);
                if (f == "--yes" || f == "-y" || f == "--force") { confirmed = true; break; }
            }
            if (!confirmed && c->GetChildCount() > 0) {
                BML::Shell::Fail(bml, "[hud] container not empty. Use --yes to confirm.\n");
                return;
            }
        }
        auto up = hud.StealByPath(id);
        if (up)
            bml->SendIngameMessage("[hud] removed\n");
        else
            BML::Shell::Fail(bml, "[hud] remove failed\n");
        return;
    }
    if (args.size() == 2 && args[1] == "list") {
        auto ids = hud.ListIds();
        std::string line = "[hud] elements:";
        for (auto &s : ids) { line += " "; line += s; }
        line += "\n";
        bml->SendIngameMessage(line.c_str());
        return;
    }
    if (args.size() >= 3 && args[1] == "get") {
        const std::string &id = args[2];
        auto e = (id.find('.') == std::string::npos) ? hud.Find(id) : hud.FindByPath(id);
        if (!e) { BML::Shell::Fail(bml, "[hud] not found\n"); return; }
        std::string info = "[hud] "; info += id; info += ": "; info += (e->IsVisible()?"visible":"hidden"); info += "; page="; info += e->GetPage(); info += "\n";
        bml->SendIngameMessage(info.c_str());
        return;
    }
    if (args.size() >= 5 && args[1] == "set") {
        const std::string &id = args[2];
        auto e = (id.find('.') == std::string::npos) ? hud.GetOrCreate(id) : hud.FindByPath(id);
        const std::string what = utils::ToLower(args[3]);
        if (what == "pos") {
            bool ok=false; AnchorPoint ap = ParseAnchor(args[4], ok);
            if (!ok) { BML::Shell::Fail(bml, "[hud] pos <anchor> [x y]\n"); return; }
            float ox = 0.0f, oy = 0.0f;
            if (args.size() >= 7) { ox = (float) atof(args[5].c_str()); oy = (float) atof(args[6].c_str()); }
            e->SetAnchor(ap); e->SetOffsetPixels(ox, oy); return;
        }
        // Text-specific properties - only apply to HUDText elements
        if (auto textElement = HUDCast<HUDText>(e)) {
            if (what == "scale" && args.size() >= 5) { textElement->SetScale((float) atof(args[4].c_str())); return; }
            if (what == "wrap_px" && args.size() >= 5) { textElement->SetWrapWidthPx((float) atof(args[4].c_str())); return; }
            if (what == "wrap_frac" && args.size() >= 5) { textElement->SetWrapWidthFrac((float) atof(args[4].c_str())); return; }
            if (what == "tabs" && args.size() >= 5) { textElement->SetTabColumns(std::max(1, ParseInteger(args[4]))); return; }
        }
        if (what == "visible" && args.size() >= 5) { e->SetVisible(ParseBoolean(args[4])); return; }
        if (what == "panel" && args.size() >= 5) { e->EnablePanel(ParseBoolean(args[4])); return; }
        if (what == "panel_bg" && args.size() >= 5) {
            ImU32 col;
            if (!ParseColor(args[4], col)) { BML::Shell::Fail(bml, "[hud] invalid panel background color\n"); return; }
            e->SetPanelBgColor(col);
            return;
        }
        if (what == "panel_border" && args.size() >= 5) {
            ImU32 col;
            if (!ParseColor(args[4], col)) { BML::Shell::Fail(bml, "[hud] invalid panel border color\n"); return; }
            e->SetPanelBorderColor(col);
            return;
        }
        if (what == "padding" && args.size() >= 5) { e->SetPanelPadding((float) atof(args[4].c_str())); return; }
        if (what == "border_thickness" && args.size() >= 5) { e->SetPanelBorderThickness((float) atof(args[4].c_str())); return; }
        if (what == "rounding" && args.size() >= 5) { e->SetPanelRounding((float) atof(args[4].c_str())); return; }
        if (what == "page" && args.size() >= 5) { e->SetPage(args[4]); return; }
        // Container-specific options
        auto c = HUDCast<HUDContainer>(e);
        if (c) {
            if (what == "spacing" && args.size() >= 5) { c->SetSpacing((float)atof(args[4].c_str())); return; }
            if (what == "cols" && args.size() >= 5) { c->SetGridCols(std::max(1, ParseInteger(args[4]))); return; }
            if (what == "align_x" && args.size() >= 5) {
                bool ok = true; const AlignX value = ParseAlignX(args[4], ok);
                if (!ok) { BML::Shell::Fail(bml, "[hud] invalid horizontal alignment\n"); return; }
                c->SetAlignX(value); return;
            }
            if (what == "align_y" && args.size() >= 5) {
                bool ok = true; const AlignY value = ParseAlignY(args[4], ok);
                if (!ok) { BML::Shell::Fail(bml, "[hud] invalid vertical alignment\n"); return; }
                c->SetAlignY(value); return;
            }
            if (what == "cell_align_x" && args.size() >= 5) {
                bool ok = true; const AlignX value = ParseAlignX(args[4], ok);
                if (!ok) { BML::Shell::Fail(bml, "[hud] invalid cell horizontal alignment\n"); return; }
                c->SetCellAlignX(value); return;
            }
            if (what == "cell_align_y" && args.size() >= 5) {
                bool ok = true; const AlignY value = ParseAlignY(args[4], ok);
                if (!ok) { BML::Shell::Fail(bml, "[hud] invalid cell vertical alignment\n"); return; }
                c->SetCellAlignY(value); return;
            }
        }
        BML::Shell::Fail(bml, "[hud] unknown or incomplete property\n");
        return;
    }

    BML::Shell::Fail(bml, "Usage: hud <action>; use 'hud help' for commands.\n");
}

const std::vector<std::string> HUDCommand::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2)
        return {"on", "off", "title", "fps", "sr", "add", "text", "set", "del", "list", "get", "vstack", "hstack", "grid", "child", "page", "help", "mv", "rename", "cp", "clone"};
    if (args.size() >= 3 && args[1] == std::string("set"))
        return {"pos", "scale", "visible", "wrap_px", "wrap_frac", "tabs", "text_align", "panel", "panel_bg", "panel_border", "padding", "border_thickness", "rounding", "page", "spacing", "cols", "align_x", "align_y", "cell_align_x", "cell_align_y"};
    return {};
}
