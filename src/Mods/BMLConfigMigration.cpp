#include "Mods/BMLConfigMigration.h"

#include <algorithm>
#include <cmath>

#include "Config/Config.h"
#include "UI/FontRuntime.h"

#include "PathUtils.h"
#include "StringUtils.h"

namespace {
    constexpr const char *OldFontKeys[] = {
        "FontRanges", "EnableSecondaryFont", "SecondaryFontFilename",
        "SecondaryFontSize", "SecondaryFontRanges",
    };

    const std::string *FindString(const ConfigData &values, const char *category, const char *key) {
        const ConfigData::Entry *entry = values.Find(category, key);
        return entry && entry->Type == IProperty::STRING ? &std::get<std::string>(entry->Value) : nullptr;
    }

    bool ReadFloat(const ConfigData &values, const char *category, const char *key, float &value) {
        const ConfigData::Entry *entry = values.Find(category, key);
        if (!entry || entry->Type != IProperty::FLOAT)
            return false;
        value = std::get<float>(entry->Value);
        return std::isfinite(value);
    }

    bool ResolveOldFontPath(const std::string &requested, std::string &resolved) {
        resolved = requested;
        if (requested.empty() || utils::IsAbsolutePathUtf8(requested))
            return true;

        const std::string workingPath = utils::ResolvePathUtf8(
            utils::CombinePathUtf8(utils::GetCurrentDirectoryUtf8(), requested));
        if (utils::FileExistsUtf8(workingPath)) {
            resolved = workingPath;
            return true;
        }

        // The current font catalog accepts names but not paths relative to its
        // Fonts directory. Do not mistake a missing old path for a catalog name.
        return utils::GetFileNameUtf8(requested) == requested;
    }
}

bool MigrateBMLConfig(ConfigData &values) {
    bool hasOldFontSettings = false;
    for (const char *key : OldFontKeys)
        hasOldFontSettings |= values.HasKey("GUI", key);

    const bool hasFallbacks = FindString(values, "GUI", "FontFallbacks") != nullptr;
    float fallbackSize = 0.0f;
    const bool hasFallbackSize = ReadFloat(values, "GUI", "FontFallbackSize", fallbackSize);
    bool needsManualSelection = false;
    bool keepOldSecondarySettings = false;

    std::string primaryFace = BML::UI::FontProfile{}.PrimaryFace;
    if (const std::string *primary = FindString(values, "GUI", "FontFilename")) {
        primaryFace = *primary;
        if (hasOldFontSettings && !hasFallbacks) {
            std::string resolved;
            if (!ResolveOldFontPath(primaryFace, resolved)) {
                needsManualSelection = true;
            } else if (resolved != primaryFace) {
                values.Set("GUI", "FontFilename", IProperty::STRING, resolved);
                primaryFace = resolved;
            }
        }
    }

    if (hasOldFontSettings && !hasFallbacks) {
        float oldPrimarySize = 0.0f;
        if (ReadFloat(values, "GUI", "FontSize", oldPrimarySize) && oldPrimarySize <= 0.0f)
            values.Set("GUI", "FontSize", IProperty::FLOAT, BML::UI::FontProfile{}.ReferenceSize);
    }

    const ConfigData::Entry *secondary = values.Find("GUI", "EnableSecondaryFont");
    const bool secondaryEnabled = secondary && secondary->Type == IProperty::BOOLEAN && std::get<bool>(secondary->Value);
    const std::string *secondaryFace = FindString(values, "GUI", "SecondaryFontFilename");
    if (!hasFallbacks && secondaryEnabled && secondaryFace) {
        const std::string requested = *secondaryFace;
        std::string resolved;
        // 0.3.13 could load one face twice with different glyph ranges. Dynamic fonts
        // no longer need a second copy of the same face.
        if (!ResolveOldFontPath(requested, resolved) || resolved.find(';') != std::string::npos) {
            needsManualSelection = true;
            keepOldSecondarySettings = true;
        } else if (!resolved.empty() && utils::CompareString(resolved, primaryFace) != 0) {
            values.Set("GUI", "FontFallbacks", IProperty::STRING, resolved);
        }
    }

    if (hasOldFontSettings && !hasFallbackSize) {
        float size = BML::UI::FontProfile{}.ReferenceSize;
        if (!ReadFloat(values, "GUI", "SecondaryFontSize", size))
            ReadFloat(values, "GUI", "FontSize", size);
        if (size <= 0.0f)
            size = BML::UI::FontProfile{}.ReferenceSize;
        size = std::clamp(size, BML::UI::MinimumFontReferenceSize,
                          BML::UI::MaximumFontReferenceSize);
        values.Set("GUI", "FontFallbackSize", IProperty::FLOAT, size);
    }

    for (const char *key : OldFontKeys) {
        if (keepOldSecondarySettings &&
            (utils::CompareString(key, "EnableSecondaryFont") == 0 ||
             utils::CompareString(key, "SecondaryFontFilename") == 0 ||
             utils::CompareString(key, "SecondaryFontSize") == 0)) {
            continue;
        }
        values.Remove("GUI", key);
    }
    values.Remove("CommandBar", "WindowBackgroundAlpha");
    return !needsManualSelection;
}
