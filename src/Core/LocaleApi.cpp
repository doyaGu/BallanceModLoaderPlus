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

        CallerInfo ResolveCallerInfo() {
            CallerInfo info;
            auto *consumer = GetKernelOrNull()->context->ResolveCurrentConsumer();
            if (!consumer)
                return info;

            info.id = consumer->id;
            if (consumer->manifest) {
                info.directory = consumer->manifest->directory;
            }
            info.valid = true;
            return info;
        }

        std::string GetCallerModuleId() {
            auto *consumer = GetKernelOrNull()->context->ResolveCurrentConsumer();
            return consumer ? consumer->id : std::string{};
        }
    } // namespace

    BML_Result BML_API_LocaleLoad(const char *locale_code) {
        auto caller = ResolveCallerInfo();
        if (!caller.valid)
            return BML_RESULT_INVALID_CONTEXT;

        // If no locale_code provided, use the current global language
        std::string code;
        if (locale_code && locale_code[0] != '\0') {
            code = locale_code;
        } else {
            const char *current = nullptr;
            GetKernelOrNull()->locale->GetLanguage(&current);
            code = current ? current : "en";
        }

        return GetKernelOrNull()->locale->Load(caller.id, caller.directory, code);
    }

    const char *BML_API_LocaleGet(const char *key) {
        if (!key)
            return nullptr;

        std::string module_id = GetCallerModuleId();
        if (module_id.empty())
            return key;

        return GetKernelOrNull()->locale->Get(module_id, key);
    }

    BML_Result BML_API_LocaleSetLanguage(const char *language_code) {
        BML_Result result = GetKernelOrNull()->locale->SetLanguage(language_code);
        if (result == BML_RESULT_OK) {
            // Reload all modules with the new language
            GetKernelOrNull()->locale->ReloadAll();
        }
        return result;
    }

    BML_Result BML_API_LocaleGetLanguage(const char **out_code) {
        return GetKernelOrNull()->locale->GetLanguage(out_code);
    }

    BML_Result BML_API_LocaleBindTable(BML_LocaleTable *out_table) {
        if (!out_table)
            return BML_RESULT_INVALID_ARGUMENT;

        std::string module_id = GetCallerModuleId();
        if (module_id.empty()) {
            *out_table = nullptr;
            return BML_RESULT_INVALID_CONTEXT;
        }

        *out_table = GetKernelOrNull()->locale->BindTable(module_id);
        return BML_RESULT_OK;
    }

    const char *BML_API_LocaleLookup(BML_LocaleTable table, const char *key) {
        return GetKernelOrNull()->locale->Lookup(table, key);
    }

    void RegisterLocaleApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlLocaleLoad, "locale", BML_API_LocaleLoad);
        BML_REGISTER_API(bmlLocaleGet, BML_API_LocaleGet);
        BML_REGISTER_API_GUARDED(bmlLocaleSetLanguage, "locale", BML_API_LocaleSetLanguage);
        BML_REGISTER_API_GUARDED(bmlLocaleGetLanguage, "locale", BML_API_LocaleGetLanguage);
        BML_REGISTER_API_GUARDED(bmlLocaleBindTable, "locale", BML_API_LocaleBindTable);
        BML_REGISTER_API(bmlLocaleLookup, BML_API_LocaleLookup);
    }
} // namespace BML::Core
