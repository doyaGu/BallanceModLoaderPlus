#pragma once

#include <string>

namespace BML::Core {
    using ExtensionProviderCleanupHook = void (*)(const std::string &provider_id);

    void SetExtensionProviderCleanupHook(ExtensionProviderCleanupHook hook);
    void RunExtensionProviderCleanupHook(const std::string &provider_id);
} // namespace BML::Core
