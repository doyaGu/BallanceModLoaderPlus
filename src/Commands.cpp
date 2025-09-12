#include "Commands.h"

#include <sstream>

#include "BML/IBML.h"
#include "BML/BML.h"
#include "BMLMod.h"

#include "ModContext.h"
#include "StringUtils.h"
#include "AnsiPalette.h"

void CommandBML::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->SendIngameMessage("Ballance Mod Loader Plus " BML_VERSION);
    bml->SendIngameMessage((std::to_string(bml->GetModCount()) + " Mods Installed:").data());

    int count = bml->GetModCount();
    for (int i = 0; i < count; ++i) {
        auto *mod = bml->GetMod(i);
        std::string str = std::string("  ") + mod->GetID() + ": " + mod->GetName() + " " + mod->GetVersion() + " by " + mod->GetAuthor();
        bml->SendIngameMessage(str.data());
    }
}

void CommandHelp::Execute(IBML *bml, const std::vector<std::string> &args) {
    const int cmdCount = bml->GetCommandCount();
    bml->SendIngameMessage((std::to_string(cmdCount) + " Existing Commands:").data());
    for (int i = 0; i < cmdCount; i++) {
        ICommand *cmd = bml->GetCommand(i);
        std::string str = std::string("\t") + cmd->GetName();
        if (!cmd->GetAlias().empty())
            str += "(" + cmd->GetAlias() + ")";
        if (cmd->IsCheat())
            str += "[Cheat]";
        str += ": " + cmd->GetDescription();
        bml->SendIngameMessage(str.data());
    }
}

void CommandCheat::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        bml->EnableCheat(!bml->IsCheatEnabled());
    } else {
        bml->EnableCheat(ParseBoolean(args[1]));
    }
    bml->SendIngameMessage(bml->IsCheatEnabled() ? "Cheat Mode On" : "Cheat Mode Off");
}

void CommandEcho::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml) return;

    // No args -> print newline
    if (args.size() <= 1) {
        bml->SendIngameMessage("\n");
        return;
    }

    EchoOpts opt;
    size_t idx = 1;

    // Option parsing:
    // - consume a standalone "--"
    // - consume only tokens of the form -[n,e,E]+
    // - stop (do not consume) on first non-option or unknown option
    while (idx < args.size()) {
        const std::string &tok = args[idx];

        if (tok == "--") {
            // explicit end of options, do not emit it
            ++idx;
            break;
        }

        if (tok.size() >= 2 && tok[0] == '-') {
            // Check if token is entirely composed of recognized flags
            bool recognized = true;
            for (size_t i = 1; i < tok.size(); ++i) {
                char c = tok[i];
                if (c != 'n' && c != 'e' && c != 'E') {
                    recognized = false;
                    break;
                }
            }

            if (recognized) {
                ParseEchoOptionToken(tok, opt); // update flags
                ++idx;                          // consume this option token
                continue;
            }
            // Unknown option like "-x": stop parsing and treat it as data
        }

        // Non-option token: stop parsing
        break;
    }

    std::string out = JoinArgs(args, idx);

    bool suppressNewlineViaC = false;
    if (opt.interpretEscapes) {
        // \c truncation is handled before unescaping, as in bash echo -e
        suppressNewlineViaC = ApplyBackslashCTrunc(out);
        out = utils::UnescapeString(out.c_str());
    }

    if (!(opt.noNewline || suppressNewlineViaC)) {
        out.push_back('\n');
    }

    bml->SendIngameMessage(out.c_str());
}

// Parse echo options within a single recognized token like "-neE"
void CommandEcho::ParseEchoOptionToken(const std::string &tok, EchoOpts &opt) {
    // precondition: tok.size() >= 2 && tok[0] == '-'
    for (size_t i = 1; i < tok.size(); ++i) {
        const char c = tok[i];
        switch (c) {
        case 'n': opt.noNewline = true;
            break;
        case 'e': opt.interpretEscapes = true;
            break;
        case 'E': opt.interpretEscapes = false;
            break;
        default: opt.parsingOptions = false;
            return; // unknown flag -> stop option mode
        }
    }
}

// Join args with spaces from given index
std::string CommandEcho::JoinArgs(const std::vector<std::string> &args, size_t start) {
    std::string s;
    for (size_t i = start; i < args.size(); ++i) {
        if (i > start) s.push_back(' ');
        s.append(args[i]);
    }
    return s;
}

// Handle \c (truncate output and suppress newline)
bool CommandEcho::ApplyBackslashCTrunc(std::string &s) {
    bool stop = false;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\\') {
            size_t j = i;
            while (j < s.size() && s[j] == '\\') ++j;
            bool escaped = ((j - i) % 2 == 1);
            if (escaped && j < s.size() && s[j] == 'c') {
                s.erase(i);
                stop = true;
                break;
            }
            i = j + (escaped ? 1 : 0);
        } else {
            ++i;
        }
    }
    return stop;
}

void CommandClear::Execute(IBML *bml, const std::vector<std::string> &args) {
    m_BMLMod->ClearIngameMessages();
}

void CommandHistory::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 1) {
        m_BMLMod->PrintHistory();
    } else if (args.size() == 2) {
        if (args[1] == "clear") {
            m_BMLMod->ClearHistory();
        } else {
            int i = ParseInteger(args[1]);
            if (i != 0)
                m_BMLMod->ExecuteHistory(i + 1);
        }
    }
}

void CommandExit::Execute(IBML *bml, const std::vector<std::string> &args) {
    bml->ExitGame();
}

CommandHUD::CommandHUD(BMLMod *mod) : m_BMLMod(mod) {
    m_State = m_BMLMod->GetHUD();
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
    int r=0,g=0,b=0,a=255; int n=0;
    char c;
    std::stringstream ss(v);
    if (ss >> r) { ++n; if (ss >> c && (c==','||c==';'||c==' ')) {} if (ss >> g) { ++n; if (ss >> c) {} if (ss >> b) { ++n; if (ss >> c) {} if (ss >> a) ++n; } } }
    if (n>=3) { out = IM_COL32(std::clamp(r,0,255), std::clamp(g,0,255), std::clamp(b,0,255), std::clamp(a,0,255)); return true; }
    return false;
}

static AlignX ParseAlignX(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "left" || t == "l") return AlignX::Left;
    if (t == "center" || t == "c" || t == "middle" || t == "m") return AlignX::Center;
    if (t == "right" || t == "r") return AlignX::Right;
    ok=false; return AlignX::Left;
}
static AlignY ParseAlignY(const std::string &s, bool &ok) {
    std::string t = utils::ToLower(s); ok = true;
    if (t == "top" || t == "t") return AlignY::Top;
    if (t == "middle" || t == "center" || t == "m" || t == "c") return AlignY::Middle;
    if (t == "bottom" || t == "b") return AlignY::Bottom;
    ok = false;
    return AlignY::Top;
}

void CommandHUD::Execute(IBML *bml, const std::vector<std::string> &args) {
    // Basic toggles preserved
    if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
        if (ParseBoolean(args[1])) {
            m_BMLMod->SetHUD(m_State);
        } else {
            m_State = m_BMLMod->GetHUD();
            m_BMLMod->SetHUD(0);
        }
        return;
    }
    if (args.size() == 3 && (args[1] == "title" || args[1] == "fps" || args[1] == "sr")) {
        int state = m_BMLMod->GetHUD();
        const bool on = ParseBoolean(args[2]);
        if (args[1] == "title") state = on ? (state | HUD_TITLE) : (state & ~HUD_TITLE);
        else if (args[1] == "fps") state = on ? (state | HUD_FPS) : (state & ~HUD_FPS);
        else if (args[1] == "sr") state = on ? (state | HUD_SR) : (state & ~HUD_SR);
        m_BMLMod->SetHUD(state);
        return;
    }

    // TUI-like custom text management
    HUD &hud = m_BMLMod->GetHUDWindow();
    if (args.size() >= 3 && args[1] == "add") {
        const std::string &id = args[2];
        size_t dot = id.find('.');
        if (dot == std::string::npos) {
            HUDElement *e = hud.GetOrCreate(id);
            if (args.size() >= 4) {
                std::string txt; for (size_t i = 3; i < args.size(); ++i) { if (i>3) txt.push_back(' '); txt += args[i]; }
                e->SetText(txt.c_str());
            }
            bml->SendIngameMessage(("[hud] added '" + id + "'\n").c_str());
        } else {
            std::string contId = id.substr(0, dot);
            std::string childId = id.substr(dot + 1);
            HUDElement *child = hud.GetOrCreateChild(contId, childId);
            if (!child) { bml->SendIngameMessage("[hud] container not found\n"); return; }
            if (args.size() >= 4) {
                std::string txt; for (size_t i = 3; i < args.size(); ++i) { if (i>3) txt.push_back(' '); txt += args[i]; }
                child->SetText(txt.c_str());
            }
            bml->SendIngameMessage(("[hud] added child '" + childId + "' in '" + contId + "'\n").c_str());
        }
        return;
    }
    if (args.size() >= 3 && (args[1] == "vstack" || args[1] == "hstack")) {
        const std::string &id = args[2];
        size_t lastDot = id.find_last_of('.');
        HUDContainer *c = nullptr;
        if (lastDot == std::string::npos) {
            c = (args[1] == std::string("vstack")) ? hud.AddVStack() : hud.AddHStack();
            hud.Register(id, c);
        } else {
            std::string parentPath = id.substr(0, lastDot);
            std::string childName = id.substr(lastDot + 1);
            HUDElement *pe = hud.FindByPath(parentPath);
            auto *pc = dynamic_cast<HUDContainer *>(pe);
            if (!pc) { bml->SendIngameMessage("[hud] parent container not found\n"); return; }
            HUDLayoutKind kind = (args[1] == std::string("hstack")) ? HUDLayoutKind::Horizontal : HUDLayoutKind::Vertical;
            c = pc->AddContainerChild(kind, childName, 1);
            hud.Register(id, c);
        }
        bml->SendIngameMessage(((std::string("[hud] ") + args[1] + " '") + id + "' created\n").c_str());
        return;
    }
    if (args.size() >= 4 && args[1] == "grid") {
        const std::string &id = args[2]; int cols = std::max(1, ParseInteger(args[3]));
        size_t lastDot = id.find_last_of('.'); HUDContainer *c = nullptr;
        if (lastDot == std::string::npos) {
            c = hud.AddGrid(cols); hud.Register(id, c);
        } else {
            std::string parentPath = id.substr(0, lastDot);
            std::string childName = id.substr(lastDot + 1);
            HUDElement *pe = hud.FindByPath(parentPath);
            auto *pc = dynamic_cast<HUDContainer *>(pe);
            if (!pc) { bml->SendIngameMessage("[hud] parent container not found\n"); return; }
            c = pc->AddContainerChild(HUDLayoutKind::Grid, childName, cols);
            hud.Register(id, c);
        }
        bml->SendIngameMessage(("[hud] grid '" + id + "' created\n").c_str());
        return;
    }
    if (args.size() >= 5 && args[1] == "child" && args[2] == std::string("add")) {
        const std::string &containerId = args[3];
        const std::string &childId = args[4];
        HUDElement *cont = (containerId.find('.') == std::string::npos) ? hud.Find(containerId) : hud.FindByPath(containerId);
        auto *c = dynamic_cast<HUDContainer *>(cont);
        if (!c) { bml->SendIngameMessage("[hud] container not found\n"); return; }
        std::string txt;
        for (size_t i = 5; i < args.size(); ++i) {
            if (i > 5) txt.push_back(' ');
            txt += args[i];
        }
        HUDElement *child = c->AddChildNamed(childId, ""); if (!txt.empty()) child->SetText(txt.c_str());
        // no global registry for child id to avoid conflicts; future work could add nested naming
        bml->SendIngameMessage("[hud] child added\n");
        return;
    }
    if (args.size() >= 3 && args[1] == "load") {
        // hud load <relative|absolute path>
        std::string p; for (size_t i=2;i<args.size();++i){ if(i>2) p.push_back(' '); p+=args[i]; }
        std::wstring wp = utils::ToWString(p);
        if (hud.LoadConfig(wp)) bml->SendIngameMessage("[hud] config loaded\n"); else bml->SendIngameMessage("[hud] failed to load config\n");
        return;
    }
    if (args.size() >= 2 && args[1] == "save") {
        std::wstring wp;
        if (args.size() >= 3) wp = utils::ToWString(args[2]);
        else {
            std::string dir = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);
            wp = utils::ToWString(dir + "\\hud.ini");
        }
        if (hud.SaveConfigIni(wp)) bml->SendIngameMessage("[hud] config saved\n"); else bml->SendIngameMessage("[hud] failed to save config\n");
        return;
    }
    if (args.size() >= 3 && args[1] == "savetree") {
        const std::string &pathArg = args[2];
        std::wstring wp;
        if (args.size() >= 4) wp = utils::ToWString(args[3]);
        else {
            std::string dir = BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER);
            wp = utils::ToWString(dir + "\\hud_subtree.ini");
        }
        if (hud.SaveSubtreeIni(wp, pathArg)) bml->SendIngameMessage("[hud] subtree saved\n"); else bml->SendIngameMessage("[hud] subtree save failed\n");
        return;
    }
    if (args.size() >= 2 && args[1] == "sample") {
        bool wrote = hud.SaveSampleIfMissing();
        bml->SendIngameMessage(wrote ? "[hud] sample created (hud.ini)\n" : "[hud] sample exists\n");
        return;
    }
    if (args.size() >= 4 && args[1] == "mv") {
        const std::string &src = args[2];
        const std::string &dst = args[3];
        std::string newName = (args.size() >= 5) ? args[4] : std::string();
        HUDContainer *dest = hud.EnsureContainerPath(dst, HUDLayoutKind::Vertical);
        if (!dest) { bml->SendIngameMessage("[hud] invalid destination\n"); return; }
        auto up = hud.StealByPath(src);
        if (!up) { bml->SendIngameMessage("[hud] source not found\n"); return; }
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
            HUDElement *e = hud.Find(path);
            if (!e) { bml->SendIngameMessage("[hud] not found\n"); return; }
            // Update global name mapping
            // Remove old
            // Simple: steal and reattach to root with new name
            auto up = hud.StealByPath(path);
            if (!up) { bml->SendIngameMessage("[hud] rename failed\n"); return; }
            hud.AttachToRoot(std::move(up), newName);
            bml->SendIngameMessage("[hud] renamed\n");
        } else {
            std::string parentPath = path.substr(0, dot);
            std::string oldName = path.substr(dot + 1);
            HUDElement *pe = hud.FindByPath(parentPath);
            auto *pc = dynamic_cast<HUDContainer *>(pe);
            if (!pc) { bml->SendIngameMessage("[hud] parent not found\n"); return; }
            // Re-map
            auto up = pc->StealChild(oldName);
            if (!up) { bml->SendIngameMessage("[hud] child not found\n"); return; }
            pc->InsertChild(std::move(up), newName);
            bml->SendIngameMessage("[hud] renamed\n");
        }
        return;
    }
    if (args.size() >= 4 && (args[1] == "cp" || args[1] == "copy")) {
        const std::string &src = args[2];
        const std::string &dst = args[3];
        std::string newName = (args.size() >= 5) ? args[4] : std::string();
        HUDElement *srcE = hud.FindByPath(src);
        if (!srcE) { bml->SendIngameMessage("[hud] source not found\n"); return; }
        HUDContainer *dest = hud.EnsureContainerPath(dst, HUDLayoutKind::Vertical);
        if (!dest) { bml->SendIngameMessage("[hud] invalid destination\n"); return; }
        auto up = hud.CloneElement(srcE);
        if (!up) { bml->SendIngameMessage("[hud] copy failed\n"); return; }
        if (newName.empty()) { size_t dot = src.find_last_of('.'); newName = (dot==std::string::npos)?src:src.substr(dot+1); }
        hud.AttachToContainer(dest, std::move(up), newName);
        bml->SendIngameMessage("[hud] copied\n");
        return;
    }
    if (args.size() >= 3 && args[1] == "clone") {
        const std::string &src = args[2];
        std::string newName = (args.size() >= 4) ? args[3] : std::string();
        HUDElement *srcE = hud.FindByPath(src);
        if (!srcE) { bml->SendIngameMessage("[hud] source not found\n"); return; }
        auto up = hud.CloneElement(srcE);
        if (!up) { bml->SendIngameMessage("[hud] clone failed\n"); return; }
        if (newName.empty()) { size_t dot = src.find_last_of('.'); newName = (dot==std::string::npos)?src:src.substr(dot+1); }
        // If active page is set and default container mapping exists, attach under it; else root
        if (!hud.GetActivePage().empty()) {
            const std::string &pc = hud.GetPageDefaultContainer(hud.GetActivePage());
            if (!pc.empty()) {
                HUDContainer *dest = hud.EnsureContainerPath(pc, HUDLayoutKind::Vertical);
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
        bml->SendIngameMessage("  hud set <id> page <name>|template <text>\n");
        bml->SendIngameMessage("  hud vstack|hstack <id>            : create container\n");
        bml->SendIngameMessage("  hud grid <id> <cols>              : create grid container\n");
        bml->SendIngameMessage("  hud child add <container> <child> [ansi_text]\n");
        bml->SendIngameMessage("  hud set <container> spacing <px>|cols <n>|align_x <l|c|r>|align_y <t|m|b>|cell_align_x <...>|cell_align_y <...>\n");
        bml->SendIngameMessage("  hud load <path>                    : load config (ini)\n");
        bml->SendIngameMessage("  hud save [path]                    : save current HUD\n");
        bml->SendIngameMessage("  hud savetree <path> [file]        : save subtree to INI\n");
        bml->SendIngameMessage("  hud sample                         : create sample hud.ini\n");
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
    if (args.size() >= 2 && args[1] == "dump") {
        std::string root;
        if (args.size() >= 3) root = args[2];
        std::string outline = hud.DumpOutline(root);
        if (outline.empty()) bml->SendIngameMessage("[hud] no elements\n");
        else bml->SendIngameMessage(outline.c_str());
        return;
    }
    if (args.size() >= 3 && args[1] == "text") {
        const std::string &id = args[2];
        HUDElement *e = (id.find('.') == std::string::npos) ? hud.GetOrCreate(id) : hud.FindByPath(id);
        if (!e) { bml->SendIngameMessage("[hud] not found\n"); return; }
        std::string txt; for (size_t i = 3; i < args.size(); ++i) { if (i>3) txt.push_back(' '); txt += args[i]; }
        e->SetText(txt.c_str());
        return;
    }
    if (args.size() >= 3 && args[1] == "del") {
        const std::string &id = args[2];
        HUDElement *e = (id.find('.') == std::string::npos) ? hud.Find(id) : hud.FindByPath(id);
        if (!e) { bml->SendIngameMessage("[hud] not found\n"); return; }
        auto *c = dynamic_cast<HUDContainer*>(e);
        if (c) {
            bool confirmed = false;
            for (size_t i = 3; i < args.size(); ++i) {
                std::string f = utils::ToLower(args[i]);
                if (f == "--yes" || f == "-y" || f == "--force") { confirmed = true; break; }
            }
            if (!confirmed && !c->Children().empty()) {
                bml->SendIngameMessage("[hud] container not empty. Use --yes to confirm.\n");
                return;
            }
        }
        auto up = hud.StealByPath(id);
        bml->SendIngameMessage(up ? "[hud] removed\n" : "[hud] remove failed\n");
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
        HUDElement *e = (id.find('.') == std::string::npos) ? hud.Find(id) : hud.FindByPath(id);
        if (!e) { bml->SendIngameMessage("[hud] not found\n"); return; }
        std::string info = "[hud] "; info += id; info += ": "; info += (e->IsVisible()?"visible":"hidden"); info += "; page="; info += e->GetPage(); info += "\n";
        bml->SendIngameMessage(info.c_str());
        return;
    }
    if (args.size() >= 5 && args[1] == "set") {
        const std::string &id = args[2];
        HUDElement *e = (id.find('.') == std::string::npos) ? hud.GetOrCreate(id) : hud.FindByPath(id);
        const std::string what = utils::ToLower(args[3]);
        if (what == "pos") {
            bool ok=false; AnchorPoint ap = ParseAnchor(args[4], ok);
            if (!ok) { bml->SendIngameMessage("[hud] pos <anchor> [x y]\n"); return; }
            float ox = 0.0f, oy = 0.0f;
            if (args.size() >= 7) { ox = (float) atof(args[5].c_str()); oy = (float) atof(args[6].c_str()); }
            e->SetAnchor(ap); e->SetOffset(ox, oy); return;
        }
        if (what == "scale" && args.size() >= 5) { e->SetScale((float) atof(args[4].c_str())); return; }
        if (what == "visible" && args.size() >= 5) { e->SetVisible(ParseBoolean(args[4])); return; }
        if (what == "wrap_px" && args.size() >= 5) { e->SetWrapWidthPx((float) atof(args[4].c_str())); return; }
        if (what == "wrap_frac" && args.size() >= 5) { e->SetWrapWidthFrac((float) atof(args[4].c_str())); return; }
        if (what == "tabs" && args.size() >= 5) { e->SetTabColumns(std::max(1, ParseInteger(args[4]))); return; }
        if (what == "panel" && args.size() >= 5) { e->EnablePanel(ParseBoolean(args[4])); return; }
        if (what == "panel_bg" && args.size() >= 5) { ImU32 col; if (ParseColor(args[4], col)) { e->SetPanelBgColor(col); } return; }
        if (what == "panel_border" && args.size() >= 5) { ImU32 col; if (ParseColor(args[4], col)) { e->SetPanelBorderColor(col); } return; }
        if (what == "padding" && args.size() >= 5) { e->SetPanelPadding((float) atof(args[4].c_str())); return; }
        if (what == "border_thickness" && args.size() >= 5) { e->SetPanelBorderThickness((float) atof(args[4].c_str())); return; }
        if (what == "rounding" && args.size() >= 5) { e->SetPanelRounding((float) atof(args[4].c_str())); return; }
        if (what == "page" && args.size() >= 5) { e->SetPage(args[4]); return; }
        if (what == "template" && args.size() >= 5) { std::string txt; for (size_t i=4;i<args.size();++i){ if(i>4) txt.push_back(' '); txt+=args[i]; } e->SetTemplate(txt); return; }
        // Container-specific options
        auto *c = dynamic_cast<HUDContainer*>(e);
        if (c) {
            if (what == "spacing" && args.size() >= 5) { c->SetSpacing((float)atof(args[4].c_str())); return; }
            if (what == "cols" && args.size() >= 5) { c->SetGridCols(std::max(1, ParseInteger(args[4]))); return; }
            if (what == "align_x" && args.size() >= 5) { bool ok=true; c->SetAlignX(ParseAlignX(args[4], ok)); return; }
            if (what == "align_y" && args.size() >= 5) { bool ok=true; c->SetAlignY(ParseAlignY(args[4], ok)); return; }
            if (what == "cell_align_x" && args.size() >= 5) { bool ok=true; c->SetCellAlignX(ParseAlignX(args[4], ok)); return; }
            if (what == "cell_align_y" && args.size() >= 5) { bool ok=true; c->SetCellAlignY(ParseAlignY(args[4], ok)); return; }
        }
        return;
    }
}

void CommandPalette::Execute(IBML *bml, const std::vector<std::string> &args) {
    AnsiPalette &palette = AnsiText::Renderer::DefaultPalette();

    if (args.size() <= 1 || args[1] == "reload") {
        const bool loaded = palette.ReloadFromFile();
        const std::wstring path = palette.GetConfigPathW();
        if (loaded) {
            char *pathAnsi = BML_Utf16ToAnsi(path.c_str());
            bml->SendIngameMessage((std::string("[palette] reloaded from ") + pathAnsi + "\n").c_str());
            delete[] pathAnsi;
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "sample") {
        const bool written = palette.ReloadFromFile();
        const std::wstring path = palette.GetConfigPathW();
        char *pathAnsi = BML_Utf16ToAnsi(path.c_str());
        if (written) {
            bml->SendIngameMessage((std::string("[palette] sample created: ") + pathAnsi + "\n").c_str());
        } else {
            bml->SendIngameMessage((std::string("[palette] sample exists: ") + pathAnsi + "\n").c_str());
        }
        delete[] pathAnsi;
    } else if (args[1] == "list") {
        // List available themes using palette API
        AnsiPalette pal;
        auto names = pal.GetAvailableThemes();
        std::string activeTheme = pal.GetActiveThemeName();
        if (names.empty()) {
            std::wstring themesDir = pal.GetThemesDirW();
            char *dirAnsi = BML_Utf16ToAnsi(themesDir.c_str());
            std::string msg = std::string("[palette] no themes found in ") + (dirAnsi ? dirAnsi : "(null)") + ".";
            if (dirAnsi) delete[] dirAnsi;
            msg += " active: ";
            msg += (activeTheme.empty() ? "none*" : activeTheme.c_str());
            msg += "\n";
            bml->SendIngameMessage(msg.c_str());
        } else {
            std::string line = "[palette] themes";
            line += " (active: ";
            line += (activeTheme.empty() ? "none*" : activeTheme.c_str());
            line += "):";
            std::string activeLower = activeTheme;
            for (char &c : activeLower) c = (char) tolower((unsigned char) c);
            for (const auto &nm : names) {
                std::string nmLower = nm;
                for (char &c : nmLower) c = (char) tolower((unsigned char) c);
                line += " ";
                line += nm;
                if (!activeLower.empty() && nmLower == activeLower) line += "*"; // highlight current
            }
            line += "\n";
            bml->SendIngameMessage(line.c_str());
        }
    } else if (args[1] == "show") {
        // Print resolved theme chain top -> parent -> ... -> root
        AnsiPalette tmp;
        const auto chain = tmp.GetResolvedThemeChain();
        if (chain.empty()) {
            bml->SendIngameMessage("[palette] chain: none*\n");
            return;
        }
        std::string line = "[palette] chain: ";
        for (size_t i = 0; i < chain.size(); ++i) {
            if (i) line += " -> ";
            line += chain[i].name;
            if (!chain[i].exists) line += " (missing)";
        }
        line += "\n";
        bml->SendIngameMessage(line.c_str());
    } else if (args[1] == "theme") {
        if (args.size() < 3) {
            bml->SendIngameMessage("Usage: palette theme <name>\n");
            return;
        }
        const std::string &name = args[2];
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetActiveThemeName(name);
        // Reload
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) {
            std::string nameLower = utils::ToLower(name);
            std::string msg = (nameLower == "none")
                                  ? "[palette] theme cleared, using defaults + local overrides.\n"
                                  : (std::string("[palette] theme set to ") + name + ", reloaded.\n");
            bml->SendIngameMessage(msg.c_str());
        } else if (!ok) {
            bml->SendIngameMessage("[palette] failed to update config.\n");
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "info") {
        // Print current palette options/modes
        AnsiPalette pal;
        pal.ReloadFromFile();
        std::string theme = pal.GetActiveThemeName();
        std::string cube = pal.GetCubeMixFromTheme() ? "theme" : "standard";
        std::string gray = pal.GetGrayMixFromTheme() ? "theme" : "standard";
        std::string space = pal.GetLinearMix() ? "linear" : "srgb";
        float mix = pal.GetMixStrength();
        bool toning = pal.GetToningEnabled();
        float tb = pal.GetToneBrightness();
        float ts = pal.GetToneSaturation();
        char buf[256];
        snprintf(buf, sizeof(buf), "[palette] info: theme=%s cube=%s gray=%s mix=%.2f space=%s toning=%s tb=%.2f ts=%.2f\n",
                 (theme.empty() ? "none" : theme.c_str()), cube.c_str(), gray.c_str(), mix, space.c_str(),
                 toning ? "on" : "off", tb, ts);
        bml->SendIngameMessage(buf);
        // Explanations & tips
        bml->SendIngameMessage("[palette] cube: standard=xterm 6x6x6; theme=from bright primaries\n");
        bml->SendIngameMessage("[palette] gray: standard=xterm gray ramp; theme=black-white mix\n");
        bml->SendIngameMessage("[palette] tips: set cube gray/mix/space/toning via 'palette set'\n");
        bml->SendIngameMessage("          e.g. palette set cube theme | palette set mix_strength 0.7\n");
        bml->SendIngameMessage("          e.g. palette set mix_space linear | palette set gray standard\n");
    } else if (args[1] == "set") {
        // palette set <option> <value>
        if (args.size() < 4) {
            bml->SendIngameMessage("Usage: palette set <cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation> <value>\n");
            return;
        }
        std::string opt = utils::ToLower(args[2]);
        // Join remaining tokens to accept values like "70 %" with space or quoted
        std::string val;
        for (size_t i = 3; i < args.size(); ++i) {
            if (i > 3) val.push_back(' ');
            val += args[i];
        }
        // Strip surrounding quotes and trim
        val = utils::TrimStringCopy(val);
        if (!val.empty() && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\'')) && val.size() >= 2) {
            val = val.substr(1, val.size() - 2);
            val = utils::TrimStringCopy(val);
        }
        // Normalize some common shorthands
        if (opt == "linear") opt = "mix_space";
        if (opt == "mix") opt = "mix_strength";
        if (opt == "grey") opt = "gray";
        if (opt == "tone_enable" || opt == "enable_toning") opt = "toning";
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.SetThemeOption(opt, val);
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) {
            bml->SendIngameMessage("[palette] option updated.\n");
        } else if (!ok) {
            // Provide range tips for numeric keys
            if (opt == "mix_strength") {
                bml->SendIngameMessage("[palette] invalid mix_strength. Expect 0..1 or percent (e.g., 70%).\n");
            } else if (opt == "tone_brightness" || opt == "tone_saturation") {
                bml->SendIngameMessage("[palette] invalid value. Expect in [-1..1].\n");
            } else {
                bml->SendIngameMessage("[palette] failed to update config.\n");
            }
        } else {
            bml->SendIngameMessage("[palette] no config found, using default.\n");
        }
    } else if (args[1] == "get") {
        // palette get <option>
        if (args.size() < 3) {
            bml->SendIngameMessage("Usage: palette get <theme|cube|gray|mix_strength|mix_space|toning|tone_brightness|tone_saturation>\n");
            return;
        }
        std::string key = utils::ToLower(args[2]);
        if (key == "grey") key = "gray";
        if (key == "linear") key = "mix_space";
        AnsiPalette pal;
        pal.ReloadFromFile();
        char buf[192]; buf[0] = 0;
        if (key == "theme" || key == "base") {
            std::string theme = pal.GetActiveThemeName();
            snprintf(buf, sizeof(buf), "[palette] theme = %s\n", theme.empty() ? "none" : theme.c_str());
        } else if (key == "cube") {
            snprintf(buf, sizeof(buf), "[palette] cube = %s\n", pal.GetCubeMixFromTheme() ? "theme" : "standard");
        } else if (key == "gray") {
            snprintf(buf, sizeof(buf), "[palette] gray = %s\n", pal.GetGrayMixFromTheme() ? "theme" : "standard");
        } else if (key == "mix_strength" || key == "mix") {
            snprintf(buf, sizeof(buf), "[palette] mix_strength = %.2f\n", pal.GetMixStrength());
        } else if (key == "mix_space") {
            snprintf(buf, sizeof(buf), "[palette] mix_space = %s\n", pal.GetLinearMix() ? "linear" : "srgb");
        } else if (key == "toning") {
            snprintf(buf, sizeof(buf), "[palette] toning = %s\n", pal.GetToningEnabled() ? "on" : "off");
        } else if (key == "tone_brightness") {
            snprintf(buf, sizeof(buf), "[palette] tone_brightness = %.2f\n", pal.GetToneBrightness());
        } else if (key == "tone_saturation") {
            snprintf(buf, sizeof(buf), "[palette] tone_saturation = %.2f\n", pal.GetToneSaturation());
        } else {
            bml->SendIngameMessage("[palette] unknown option.\n");
            return;
        }
        if (buf[0]) bml->SendIngameMessage(buf);
    } else if (args[1] == "reset") {
        // Remove [theme] section and reload -> default behavior
        AnsiPalette pal;
        pal.SaveSampleIfMissing();
        bool ok = pal.ResetThemeOptions();
        const bool loaded = palette.ReloadFromFile();
        if (ok && loaded) bml->SendIngameMessage("[palette] theme reset. Using defaults.\n");
        else if (!ok) bml->SendIngameMessage("[palette] failed to update config.\n");
        else bml->SendIngameMessage("[palette] no config found, using default.\n");
    }
}

const std::vector<std::string> CommandPalette::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
    if (args.size() == 2) return {"reload", "sample", "list", "theme", "show", "info", "set", "get", "reset"};
    if (args.size() == 3 && args[1] == std::string("theme")) {
        // Dynamic theme names + 'none' using palette API
        std::vector<std::string> out;
        out.emplace_back("none");
        AnsiPalette pal;
        auto names = pal.GetAvailableThemes();
        const std::string &prefix = args[2];
        std::string prefixLower = utils::ToLower(prefix);
        for (auto &name : names) {
            std::string low = utils::ToLower(name);
            if (prefix.empty() || utils::StartsWith(low, prefixLower)) out.push_back(name);
        }
        return out;
    }
    if (args.size() == 3 && args[1] == std::string("set")) {
        return {"cube", "gray", "mix_strength", "mix_space", "toning", "tone_brightness", "tone_saturation"};
    }
    if (args.size() == 3 && args[1] == std::string("get")) {
        return {"theme", "cube", "gray", "mix_strength", "mix_space", "toning", "tone_brightness", "tone_saturation"};
    }
    if (args.size() == 4 && args[1] == std::string("set")) {
        std::string opt = utils::ToLower(args[2]);
        if (opt == "cube" || opt == "gray") return {"standard", "theme", "on", "off"};
        if (opt == "mix_space") return {"linear", "srgb", "on", "off"};
        if (opt == "toning") return {"on", "off"};
        // For numeric options, no static suggestions
    }
    return {};
}
