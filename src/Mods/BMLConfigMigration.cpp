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

    bool HasString(Config &config, const char *category, const char *key) {
        return config.HasKey(category, key) &&
               config.GetProperty(category, key)->GetType() == IProperty::STRING;
    }

    bool ReadFloat(Config &config, const char *category, const char *key, float &value) {
        if (!config.HasKey(category, key))
            return false;

        IProperty *property = config.GetProperty(category, key);
        if (property->GetType() != IProperty::FLOAT || !std::isfinite(property->GetFloat()))
            return false;

        value = property->GetFloat();
        return true;
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

bool MigrateBMLConfig(Config &config) {
    bool hasOldFontSettings = false;
    for (const char *key : OldFontKeys)
        hasOldFontSettings |= config.HasKey("GUI", key);

    const bool hasFallbacks = HasString(config, "GUI", "FontFallbacks");
    const bool hasFallbackSize = config.HasKey("GUI", "FontFallbackSize") &&
        config.GetProperty("GUI", "FontFallbackSize")->GetType() == IProperty::FLOAT;
    bool needsManualSelection = false;
    bool keepOldSecondarySettings = false;

    std::string primaryFace = BML::UI::FontProfile{}.PrimaryFace;
    if (HasString(config, "GUI", "FontFilename")) {
        IProperty *primary = config.GetProperty("GUI", "FontFilename");
        primaryFace = primary->GetString();
        if (hasOldFontSettings && !hasFallbacks) {
            std::string resolved;
            if (!ResolveOldFontPath(primaryFace, resolved)) {
                needsManualSelection = true;
            } else if (resolved != primaryFace) {
                primary->SetString(resolved.c_str());
                primaryFace = resolved;
            }
        }
    }

    if (hasOldFontSettings && !hasFallbacks) {
        float oldPrimarySize = 0.0f;
        if (ReadFloat(config, "GUI", "FontSize", oldPrimarySize) && oldPrimarySize <= 0.0f)
            config.GetProperty("GUI", "FontSize")->SetFloat(BML::UI::FontProfile{}.ReferenceSize);
    }

    const bool secondaryEnabled = config.HasKey("GUI", "EnableSecondaryFont") &&
        config.GetProperty("GUI", "EnableSecondaryFont")->GetBoolean();
    if (!hasFallbacks && secondaryEnabled && HasString(config, "GUI", "SecondaryFontFilename")) {
        const std::string requested = config.GetProperty("GUI", "SecondaryFontFilename")->GetString();
        std::string resolved;
        // 0.3.13 could load one face twice with different glyph ranges. Dynamic fonts
        // no longer need a second copy of the same face.
        if (!ResolveOldFontPath(requested, resolved) || resolved.find(';') != std::string::npos) {
            needsManualSelection = true;
            keepOldSecondarySettings = true;
        } else if (!resolved.empty() && utils::CompareString(resolved, primaryFace) != 0) {
            IProperty *fallbacks = config.GetProperty("GUI", "FontFallbacks");
            fallbacks->SetDefaultString("");
            fallbacks->SetString(resolved.c_str());
        }
    }

    if (hasOldFontSettings && !hasFallbackSize) {
        float size = BML::UI::FontProfile{}.ReferenceSize;
        if (!ReadFloat(config, "GUI", "SecondaryFontSize", size))
            ReadFloat(config, "GUI", "FontSize", size);
        if (size <= 0.0f)
            size = BML::UI::FontProfile{}.ReferenceSize;
        size = std::clamp(size, BML::UI::MinimumFontReferenceSize,
                          BML::UI::MaximumFontReferenceSize);
        config.GetProperty("GUI", "FontFallbackSize")->SetDefaultFloat(size);
    }

    for (const char *key : OldFontKeys) {
        if (keepOldSecondarySettings &&
            (utils::CompareString(key, "EnableSecondaryFont") == 0 ||
             utils::CompareString(key, "SecondaryFontFilename") == 0 ||
             utils::CompareString(key, "SecondaryFontSize") == 0)) {
            continue;
        }
        config.RemoveProperty("GUI", key);
    }
    config.RemoveProperty("CommandBar", "WindowBackgroundAlpha");
    return !needsManualSelection;
}
