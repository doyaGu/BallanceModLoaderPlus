#pragma once
#if BML_RUNTIME_SDL
#include "ScriptFileWatcherSdl.h"
namespace BML { using ScriptFileWatcher = ScriptFileWatcherSdl; }
#else
#include "ScriptFileWatcherWin32.h"
namespace BML { using ScriptFileWatcher = ScriptFileWatcherWin32; }
#endif

namespace BML {
// Values match ReadDirectoryChangesW; the win32 backend retains its event ABI.
enum ScriptFileAction : uint32_t {
    ScriptFileAdded = 1, ScriptFileRemoved = 2, ScriptFileModified = 3,
    ScriptFileRenamedOld = 4, ScriptFileRenamedNew = 5
};
}
