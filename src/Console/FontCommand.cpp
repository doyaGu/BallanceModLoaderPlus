#include "Console/FontCommand.h"

#include "Console/Shell/ShellIo.h"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

#include "BML/IBML.h"
#include "BML/IConfig.h"

#include "StringUtils.h"

namespace Ui = BML::UI;

namespace {
constexpr const char *SampleText =
    "Latin AaZz 0123 | "
    "\xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC | "
    "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9 | "
    "\xE4\xB8\xAD\xE6\x96\x87 | "
    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E | "
    "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4 | "
    "\xE2\x86\x90\xE2\x86\x91\xE2\x86\x92\xE2\x86\x93 \xE2\x98\x85 \xE2\x9C\x93 | "
    "\xF0\x9F\x98\x80 \xF0\x9F\x91\x8D \xF0\x9F\x9A\x80";

const char *RuntimeStateName(Ui::FontRuntimeState state) {
    using Ui::FontRuntimeState;
    switch (state) {
    case FontRuntimeState::Unconfigured: return "unconfigured";
    case FontRuntimeState::Pending: return "pending";
    case FontRuntimeState::Ready: return "ready";
    case FontRuntimeState::Degraded: return "degraded";
    }
    return "unknown";
}

const char *SourceOriginName(Ui::FontSourceOrigin origin) {
    using Ui::FontSourceOrigin;
    switch (origin) {
    case FontSourceOrigin::Configured: return "configured";
    case FontSourceOrigin::LoaderCatalog: return "loader";
    case FontSourceOrigin::WindowsCatalog: return "windows";
    case FontSourceOrigin::Embedded: return "embedded";
    }
    return "unknown";
}

bool EqualArgument(const std::string &left, const char *right) {
    return utils::CompareString(left, right) == 0;
}

bool EqualFace(const std::string &left, const std::string &right) {
    return utils::CompareString(left, right) == 0;
}

bool ContainsFace(const std::vector<std::string> &faces, const std::string &candidate) {
    for (const std::string &face : faces) {
        if (EqualFace(face, candidate))
            return true;
    }
    return false;
}

std::string ReadFaceArgument(const std::vector<std::string> &args, std::size_t first) {
    // The shell has already removed quotes, so a face name typed as one quoted
    // word or as several bare words arrives here the same way.
    std::string value = utils::JoinString(args, ' ', first);
    utils::TrimString(value);
    return value;
}

bool ParseReferenceSize(const std::string &text, float &value) {
    if (text.empty())
        return false;

    char *end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(parsed) ||
        parsed < Ui::MinimumFontReferenceSize || parsed > Ui::MaximumFontReferenceSize) {
        return false;
    }

    value = parsed;
    return true;
}

bool ParsePositiveIndex(const std::string &text, std::size_t &index) {
    unsigned long long parsed = 0;
    const char *first = text.data();
    const char *last = first + text.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (first == last || result.ec != std::errc() || result.ptr != last ||
        parsed == 0 || parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    index = static_cast<std::size_t>(parsed - 1);
    return true;
}

bool ParseSwitch(const std::string &text, bool &value) {
    if (EqualArgument(text, "on") || EqualArgument(text, "true") ||
        text == "1") {
        value = true;
        return true;
    }
    if (EqualArgument(text, "off") || EqualArgument(text, "false") ||
        text == "0") {
        value = false;
        return true;
    }
    return false;
}

std::string FormatCodepoint(std::uint32_t codepoint) {
    std::ostringstream line;
    line << "U+" << std::uppercase << std::hex << std::setfill('0')
         << std::setw(4) << codepoint;
    return line.str();
}

void SendLine(IBML &bml, const std::string &line) {
    const std::string terminated = line + '\n';
    bml.SendIngameMessage(terminated.c_str());
}

void AddUniqueFace(std::vector<std::string> &faces, std::string face) {
    utils::TrimString(face);
    if (!face.empty() && !ContainsFace(faces, face))
        faces.push_back(std::move(face));
}
}

bool FontCommandContext::IsComplete() const noexcept {
    return Runtime && PrimaryFace && ReferenceSize && FallbackFaces &&
           UseWindowsFallbacks;
}

Ui::FontProfile FontCommandContext::ReadProfile() const {
    Ui::FontProfile profile;
    if (!IsComplete())
        return profile;

    profile.PrimaryFace = PrimaryFace->GetString();
    profile.ReferenceSize = ReferenceSize->GetFloat();
    profile.FallbackFaces = ReadFallbackFaces();
    profile.UseWindowsFallbacks = UseWindowsFallbacks->GetBoolean();
    return profile;
}

std::vector<std::string> FontCommandContext::ReadFallbackFaces() const {
    std::vector<std::string> faces;
    if (!FallbackFaces)
        return faces;

    const std::vector<std::string> configured = utils::SplitString(
        std::string(FallbackFaces->GetString()), std::string(";"));
    for (const std::string &face : configured)
        AddUniqueFace(faces, face);
    return faces;
}

void FontCommandContext::WriteFallbackFaces(const std::vector<std::string> &faces) const {
    if (!FallbackFaces)
        return;

    const std::string value = utils::JoinString(faces, std::string(";"));
    FallbackFaces->SetString(value.c_str());
}

CommandFont::CommandFont(FontCommandContext context) : m_Context(context) {
    if (m_Context.Runtime)
        m_KnownFaces = m_Context.Runtime->ListLoaderFaces();
}

bool CommandFont::EnsureAvailable(IBML &bml) const {
    if (m_Context.IsComplete())
        return true;
    BML::Shell::Fail(&bml, "Font runtime is unavailable.\n");
    return false;
}

void CommandFont::ShowStatus(IBML &bml, bool includeHint) const {
    if (!EnsureAvailable(bml))
        return;

    const Ui::FontProfile profile = m_Context.ReadProfile();
    const Ui::FontRuntimeSnapshot &snapshot = m_Context.Runtime->Inspect();

    std::ostringstream heading;
    heading << "Font runtime: " << RuntimeStateName(snapshot.State)
            << " (generation " << snapshot.Generation << ')';
    SendLine(bml, heading.str());
    SendLine(bml, "  Primary: " + profile.PrimaryFace);

    std::ostringstream size;
    size << "  Reference size: " << profile.ReferenceSize
         << " px at 1200 px viewport height";
    SendLine(bml, size.str());

    const std::vector<std::string> fallbacks = profile.FallbackFaces;
    SendLine(bml, "  Configured fallbacks: " +
                  (fallbacks.empty() ? std::string("none") :
                   utils::JoinString(fallbacks, std::string(", "))));
    SendLine(bml, std::string("  Windows symbol/emoji fallbacks: ") +
                  (profile.UseWindowsFallbacks ? "on" : "off"));

    std::ostringstream coverage;
    coverage << "  Coverage: Unicode scalars "
             << (snapshot.SupportsUnicodeScalars ? "yes" : "no")
             << ", common emoji "
             << (snapshot.SupportsCommonEmoji ? "yes" : "no")
             << ", color emoji "
             << (snapshot.SupportsColorEmoji ? "yes" : "no");
    SendLine(bml, coverage.str());

    if (!snapshot.Diagnostics.empty()) {
        SendLine(bml, "  Warnings: " +
                      std::to_string(snapshot.Diagnostics.size()) +
                      " (use 'font sources')");
    }
    if (includeHint)
        SendLine(bml, "Use 'font help' for configuration and diagnostics.");
}

void CommandFont::ShowSources(IBML &bml) const {
    if (!EnsureAvailable(bml))
        return;

    const Ui::FontRuntimeSnapshot &snapshot = m_Context.Runtime->Inspect();
    SendLine(bml, std::to_string(snapshot.Sources.size()) +
                  " active font sources:");
    for (const Ui::FontSourceStatus &source : snapshot.Sources) {
        std::string line = "  [";
        line += SourceOriginName(source.Origin);
        line += "] ";
        line += source.RequestedFace;
        line += source.Loaded ? ": loaded" : ": unavailable";
        if (!source.ResolvedPath.empty())
            line += " -> " + source.ResolvedPath;
        SendLine(bml, line);
    }
    for (const std::string &diagnostic : snapshot.Diagnostics)
        SendLine(bml, "  Warning: " + diagnostic);
}

void CommandFont::ShowCatalog(IBML &bml) {
    if (!EnsureAvailable(bml))
        return;

    m_KnownFaces = m_Context.Runtime->ListLoaderFaces();
    SendLine(bml, std::to_string(m_KnownFaces.size()) +
                  " fonts in ModLoader\\Fonts:");
    if (m_KnownFaces.empty()) {
        SendLine(bml, "  none");
        return;
    }
    for (const std::string &face : m_KnownFaces)
        SendLine(bml, "  " + face);
}

void CommandFont::CheckText(IBML &bml, const std::string &text) const {
    if (!EnsureAvailable(bml))
        return;

    const Ui::FontCoverage coverage = m_Context.Runtime->InspectText(text);
    if (!coverage.ValidUtf8) {
        BML::Shell::Fail(&bml, "The sample is not valid UTF-8.\n");
        return;
    }
    if (!coverage.FontAvailable) {
        BML::Shell::Fail(&bml, "No applied runtime font is available yet.\n");
        return;
    }

    if (coverage.MissingCodepoints.empty()) {
        SendLine(bml, "Font covers all " +
                      std::to_string(coverage.CodepointCount) +
                      " codepoints in the sample.");
        return;
    }

    std::vector<std::string> missing;
    missing.reserve(coverage.MissingCodepoints.size());
    for (std::uint32_t codepoint : coverage.MissingCodepoints)
        missing.push_back(FormatCodepoint(codepoint));
    SendLine(bml, "Missing glyphs for " + std::to_string(missing.size()) +
                  " distinct codepoints (" +
                  std::to_string(coverage.CodepointCount) +
                  " codepoints checked): " +
                  utils::JoinString(missing, std::string(", ")));
}

void CommandFont::ShowSample(IBML &bml) const {
    SendLine(bml, std::string("Font sample: ") + SampleText);
    CheckText(bml, SampleText);
}

void CommandFont::ShowFallbacks(IBML &bml) const {
    if (!EnsureAvailable(bml))
        return;

    const std::vector<std::string> faces = m_Context.ReadFallbackFaces();
    if (faces.empty()) {
        bml.SendIngameMessage("Configured fallback fonts: none.\n");
        return;
    }

    SendLine(bml, "Configured fallback fonts:");
    for (std::size_t index = 0; index < faces.size(); ++index)
        SendLine(bml, "  " + std::to_string(index + 1) + ". " + faces[index]);
}

void CommandFont::ShowHelp(IBML &bml) const {
    bml.SendIngameMessage(
        "Font commands:\n"
        "  font status                         Show the configured profile and runtime health.\n"
        "  font sources                        Show resolved active sources and warnings.\n"
        "  font list                           List fonts in ModLoader\\Fonts.\n"
        "  font sample                         Show a multilingual and emoji probe.\n"
        "  font check <text>                   Check applied-font coverage for UTF-8 text.\n"
        "  font primary [<file>|default]       Show or set the primary font.\n"
        "  font size [<8..96>|default]         Show or set the reference size.\n"
        "  font fallback                       List configured fallback fonts.\n"
        "  font fallback add <file>            Append a fallback font.\n"
        "  font fallback remove <index|file>   Remove a fallback font.\n"
        "  font fallback clear                 Remove all configured fallbacks.\n"
        "  font system [on|off]                Show or set Windows symbol/emoji fallbacks.\n"
        "  font reload                         Rebuild the configured font on the next frame.\n"
        "  font reset                          Restore the complete default profile.\n");
}

void CommandFont::ScheduleConfiguredProfile(IBML &bml, const std::string &message) {
    if (!EnsureAvailable(bml))
        return;
    m_Context.Runtime->Configure(m_Context.ReadProfile());
    SendLine(bml, message + " Font rebuild is pending.");
}

void CommandFont::ExecutePrimary(IBML &bml, const std::vector<std::string> &args) {
    if (!EnsureAvailable(bml))
        return;
    if (args.size() == 2) {
        SendLine(bml, "Primary font: " + m_Context.ReadProfile().PrimaryFace);
        return;
    }

    std::string face = ReadFaceArgument(args, 2);
    if (EqualArgument(face, "default"))
        face = Ui::FontProfile().PrimaryFace;
    if (face.empty()) {
        BML::Shell::Fail(&bml, "Usage: font primary <file|default>\n");
        return;
    }

    m_Context.PrimaryFace->SetString(face.c_str());
    ScheduleConfiguredProfile(bml, "Primary font set to " + face + '.');
}

void CommandFont::ExecuteSize(IBML &bml, const std::vector<std::string> &args) {
    if (!EnsureAvailable(bml))
        return;
    if (args.size() == 2) {
        std::ostringstream line;
        line << "Reference size: " << m_Context.ReferenceSize->GetFloat();
        SendLine(bml, line.str());
        return;
    }

    float size = Ui::FontProfile().ReferenceSize;
    if (!EqualArgument(args[2], "default") &&
        !ParseReferenceSize(args[2], size)) {
        BML::Shell::Fail(&bml, "Font size must be a number from 8 through 96.\n");
        return;
    }

    m_Context.ReferenceSize->SetFloat(size);
    std::ostringstream message;
    message << "Reference size set to " << size << '.';
    ScheduleConfiguredProfile(bml, message.str());
}

void CommandFont::ExecuteFallback(IBML &bml, const std::vector<std::string> &args) {
    if (!EnsureAvailable(bml))
        return;
    if (args.size() == 2) {
        ShowFallbacks(bml);
        return;
    }
    if (EqualArgument(args[2], "clear")) {
        m_Context.WriteFallbackFaces({});
        ScheduleConfiguredProfile(bml, "Configured fallback fonts cleared.");
        return;
    }
    if (EqualArgument(args[2], "add")) {
        const std::string face = ReadFaceArgument(args, 3);
        if (face.empty()) {
            BML::Shell::Fail(&bml, "Usage: font fallback add <file>\n");
            return;
        }
        if (face.find(';') != std::string::npos) {
            BML::Shell::Fail(
                &bml, "Fallback font names cannot contain ';'.\n");
            return;
        }

        std::vector<std::string> faces = m_Context.ReadFallbackFaces();
        if (ContainsFace(faces, face)) {
            SendLine(bml, "Fallback font is already configured: " + face);
            return;
        }
        if (EqualFace(m_Context.ReadProfile().PrimaryFace, face)) {
            SendLine(
                bml, "The primary font does not need to be added as a fallback.");
            return;
        }
        faces.push_back(face);
        m_Context.WriteFallbackFaces(faces);
        ScheduleConfiguredProfile(bml, "Fallback font added: " + face + '.');
        return;
    }
    if (EqualArgument(args[2], "remove")) {
        const std::string requested = ReadFaceArgument(args, 3);
        if (requested.empty()) {
            BML::Shell::Fail(&bml,
                "Usage: font fallback remove <index|file>\n");
            return;
        }

        std::vector<std::string> faces = m_Context.ReadFallbackFaces();
        std::size_t index = 0;
        bool found = ParsePositiveIndex(requested, index) &&
                     index < faces.size();
        if (!found) {
            for (std::size_t candidate = 0; candidate < faces.size(); ++candidate) {
                if (EqualFace(faces[candidate], requested)) {
                    index = candidate;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            BML::Shell::Fail(&bml, "Fallback font not found: " + requested);
            return;
        }

        const std::string removed = faces[index];
        faces.erase(faces.begin() + static_cast<std::ptrdiff_t>(index));
        m_Context.WriteFallbackFaces(faces);
        ScheduleConfiguredProfile(
            bml, "Fallback font removed: " + removed + '.');
        return;
    }

    BML::Shell::Fail(&bml, "Usage: font fallback [add|remove|clear]\n");
}

void CommandFont::ExecuteSystemFallbacks(IBML &bml, const std::vector<std::string> &args) {
    if (!EnsureAvailable(bml))
        return;
    if (args.size() == 2) {
        SendLine(
            bml, std::string("Windows symbol/emoji fallbacks: ") +
                     (m_Context.UseWindowsFallbacks->GetBoolean()
                          ? "on"
                          : "off"));
        return;
    }

    bool enabled = false;
    if (!ParseSwitch(args[2], enabled)) {
        BML::Shell::Fail(&bml, "Usage: font system <on|off>\n");
        return;
    }
    m_Context.UseWindowsFallbacks->SetBoolean(enabled);
    ScheduleConfiguredProfile(
        bml, std::string("Windows symbol/emoji fallbacks turned ") +
                 (enabled ? "on." : "off."));
}

void CommandFont::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (!bml)
        return;

    if (args.size() == 1 || EqualArgument(args[1], "status")) {
        ShowStatus(*bml, args.size() == 1);
        return;
    }
    if (EqualArgument(args[1], "help")) {
        ShowHelp(*bml);
        return;
    }
    if (EqualArgument(args[1], "sources")) {
        ShowSources(*bml);
        return;
    }
    if (EqualArgument(args[1], "list")) {
        ShowCatalog(*bml);
        return;
    }
    if (EqualArgument(args[1], "sample")) {
        ShowSample(*bml);
        return;
    }
    if (EqualArgument(args[1], "check")) {
        const std::string text = utils::JoinString(args, ' ', 2);
        if (text.empty())
            BML::Shell::Fail(bml, "Usage: font check <text>\n");
        else
            CheckText(*bml, text);
        return;
    }
    if (EqualArgument(args[1], "primary")) {
        ExecutePrimary(*bml, args);
        return;
    }
    if (EqualArgument(args[1], "size")) {
        ExecuteSize(*bml, args);
        return;
    }
    if (EqualArgument(args[1], "fallback")) {
        ExecuteFallback(*bml, args);
        return;
    }
    if (EqualArgument(args[1], "system")) {
        ExecuteSystemFallbacks(*bml, args);
        return;
    }
    if (EqualArgument(args[1], "reload")) {
        if (!EnsureAvailable(*bml))
            return;
        m_Context.Runtime->Configure(m_Context.ReadProfile());
        m_Context.Runtime->RequestReload();
        bml->SendIngameMessage("Font rebuild is pending.\n");
        return;
    }
    if (EqualArgument(args[1], "reset")) {
        if (!EnsureAvailable(*bml))
            return;
        const Ui::FontProfile defaults;
        m_Context.PrimaryFace->SetString(defaults.PrimaryFace.c_str());
        m_Context.ReferenceSize->SetFloat(defaults.ReferenceSize);
        m_Context.WriteFallbackFaces(defaults.FallbackFaces);
        m_Context.UseWindowsFallbacks->SetBoolean(defaults.UseWindowsFallbacks);
        ScheduleConfiguredProfile(*bml, "Default font profile restored.");
        return;
    }

    BML::Shell::Fail(bml, "Unknown font action: " + args[1] +
                          ". Use 'font help'.");
}

const std::vector<std::string> CommandFont::GetTabCompletion(IBML *, const std::vector<std::string> &args) {
    if (args.size() == 2) {
        return {"status", "sources", "list", "sample", "check", "primary",
                "size", "fallback", "system", "reload", "reset", "help"};
    }
    if (args.size() == 3 && EqualArgument(args[1], "primary")) {
        std::vector<std::string> values = m_KnownFaces;
        values.insert(values.begin(), "default");
        return values;
    }
    if (args.size() == 3 && EqualArgument(args[1], "size"))
        return {"default"};
    if (args.size() == 3 && EqualArgument(args[1], "fallback"))
        return {"add", "remove", "clear"};
    if (args.size() == 4 && EqualArgument(args[1], "fallback") &&
        EqualArgument(args[2], "add")) {
        std::vector<std::string> faces;
        faces.reserve(m_KnownFaces.size());
        for (const std::string &face : m_KnownFaces) {
            if (face.find(';') == std::string::npos)
                faces.push_back(face);
        }
        return faces;
    }
    if (args.size() == 4 && EqualArgument(args[1], "fallback") &&
        EqualArgument(args[2], "remove")) {
        return m_Context.ReadFallbackFaces();
    }
    if (args.size() == 3 && EqualArgument(args[1], "system"))
        return {"on", "off"};
    return {};
}
