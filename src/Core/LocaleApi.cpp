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

    BML_Result BML_API_LocaleLoad(BML_Mod owner, const char *locale_code) {
        auto *kernel = Context::KernelFromMod(owner);
        auto *context = Context::ContextFromMod(owner);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        auto caller = ResolveCallerInfo(*context, owner);
        if (!caller.valid)
            return BML_RESULT_INVALID_CONTEXT;

        // If no locale_code provided, use the current global language
        std::string code;
        if (locale_code && locale_code[0] != '\0') {
            code = locale_code;
        } else {
            const char *current = nullptr;
            kernel->locale->GetLanguage(&current);
            code = current ? current : "en";
        }

        return kernel->locale->Load(caller.id, caller.directory, code);
    }

    const char *BML_API_LocaleGet(BML_Mod owner, const char *key) {
        if (!key)
            return nullptr;

        auto *kernel = Context::KernelFromMod(owner);
        auto *context = Context::ContextFromMod(owner);
        if (!kernel || !context) {
            return nullptr;
        }
        auto caller = ResolveCallerInfo(*context, owner);
        if (!caller.valid)
            return nullptr;

        return kernel->locale->Get(caller.id, key);
    }

    BML_Result BML_API_LocaleBindTable(BML_Mod owner, BML_LocaleTable *out_table) {
        if (!out_table)
            return BML_RESULT_INVALID_ARGUMENT;

        auto *kernel = Context::KernelFromMod(owner);
        auto *context = Context::ContextFromMod(owner);
        if (!kernel || !context) {
            *out_table = nullptr;
            return BML_RESULT_INVALID_CONTEXT;
        }
        auto caller = ResolveCallerInfo(*context, owner);
        if (!caller.valid) {
            *out_table = nullptr;
            return BML_RESULT_INVALID_CONTEXT;
        }

        *out_table = kernel->locale->BindTable(caller.id);
        return BML_RESULT_OK;
    }

    void RegisterLocaleApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        BML_REGISTER_API_GUARDED(bmlLocaleLoad, "locale", BML_API_LocaleLoad);
        BML_REGISTER_API(bmlLocaleGet, BML_API_LocaleGet);
        BML_REGISTER_API_GUARDED(bmlLocaleBindTable, "locale", BML_API_LocaleBindTable);
    }
} // namespace BML::Core
