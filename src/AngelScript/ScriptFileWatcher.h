#pragma once
#include "ScriptFileWatcherSdl.h"
namespace BML { using ScriptFileWatcher = ScriptFileWatcherSdl; }

namespace BML {
// Normalized change kinds used by the SDL file watcher.
enum ScriptFileAction : uint32_t {
    ScriptFileAdded = 1, ScriptFileRemoved = 2, ScriptFileModified = 3,
    ScriptFileRenamedOld = 4, ScriptFileRenamedNew = 5
};
}
