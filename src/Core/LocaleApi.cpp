#include "ApiRegistrationMacros.h"
#include "LocaleManager.h"

#include "Context.h"

#include "bml_locale.h"

namespace BML::Core {
    namespace {
        struct CallerInfo {
            std::string id;
            std::wstring directory;
            bool valid{false};
        };

        CallerInfo ResolveCallerInfo(Context &context, BML_Mod owner) {
            CallerInfo info;
            auto *consumer = context.ResolveModHandle(owner);
            if (!consumer)
                return info;

            info.id = consumer->id;
            if (consumer->manifest) {
                info.directory = consumer->manifest->directory;
            }
            info.valid = true;
            return info;
        }
    } // namespace

    BML_Result BML_API_LocaleLoadOwned(BML_Mod owner, const char *locale_code) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto caller = ResolveCallerInfo(context, owner);
        if (!caller.valid)
            return BML_RESULT_INVALID_CONTEXT;

        // If no locale_code provided, use the current global language
        std::string code;
        if (locale_code && locale_code[0] != '\0') {
            code = locale_code;
        } else {
            const char *current = nullptr;
            kernel.locale->GetLanguage(&current);
            code = current ? current : "en";
        }

        return kernel.locale->Load(caller.id, caller.directory, code);
    }

    BML_Result BML_API_LocaleLoad(const char *locale_code) {
        return BML_API_LocaleLoadOwned(Context::GetCurrentModule(), locale_code);
    }

    const char *BML_API_LocaleGetOwned(BML_Mod owner, const char *key) {
        if (!key)
            return nullptr;

        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto caller = ResolveCallerInfo(context, owner);
        if (!caller.valid)
            return key;

        return kernel.locale->Get(caller.id, key);
    }

    const char *BML_API_LocaleGet(const char *key) {
        return BML_API_LocaleGetOwned(Context::GetCurrentModule(), key);
    }

    BML_Result BML_API_LocaleSetLanguage(const char *language_code) {
        auto &locale = *Kernel().locale;
        BML_Result result = locale.SetLanguage(language_code);
        if (result == BML_RESULT_OK) {
            // Reload all modules with the new language
            locale.ReloadAll();
        }
        return result;
    }

    BML_Result BML_API_LocaleGetLanguage(const char **out_code) {
        auto &locale = *Kernel().locale;
        return locale.GetLanguage(out_code);
    }

    BML_Result BML_API_LocaleBindTableOwned(BML_Mod owner, BML_LocaleTable *out_table) {
        if (!out_table)
            return BML_RESULT_INVALID_ARGUMENT;

        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto caller = ResolveCallerInfo(context, owner);
        if (!caller.valid) {
            *out_table = nullptr;
            return BML_RESULT_INVALID_CONTEXT;
        }

        *out_table = kernel.locale->BindTable(caller.id);
        return BML_RESULT_OK;
    }

    BML_Result BML_API_LocaleBindTable(BML_LocaleTable *out_table) {
        return BML_API_LocaleBindTableOwned(Context::GetCurrentModule(), out_table);
    }

    const char *BML_API_LocaleLookup(BML_LocaleTable table, const char *key) {
        auto &locale = *Kernel().locale;
        return locale.Lookup(table, key);
    }

    void RegisterLocaleApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlLocaleLoad, "locale", BML_API_LocaleLoad);
        BML_REGISTER_API_GUARDED(bmlLocaleLoadOwned, "locale", BML_API_LocaleLoadOwned);
        BML_REGISTER_API(bmlLocaleGet, BML_API_LocaleGet);
        BML_REGISTER_API(bmlLocaleGetOwned, BML_API_LocaleGetOwned);
        BML_REGISTER_API_GUARDED(bmlLocaleSetLanguage, "locale", BML_API_LocaleSetLanguage);
        BML_REGISTER_API_GUARDED(bmlLocaleGetLanguage, "locale", BML_API_LocaleGetLanguage);
        BML_REGISTER_API_GUARDED(bmlLocaleBindTable, "locale", BML_API_LocaleBindTable);
        BML_REGISTER_API_GUARDED(bmlLocaleBindTableOwned, "locale", BML_API_LocaleBindTableOwned);
        BML_REGISTER_API(bmlLocaleLookup, BML_API_LocaleLookup);
    }
} // namespace BML::Core
