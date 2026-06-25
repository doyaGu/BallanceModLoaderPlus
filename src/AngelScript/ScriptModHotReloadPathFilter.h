#ifndef BML_SCRIPTMODHOTRELOADPATHFILTER_H
#define BML_SCRIPTMODHOTRELOADPATHFILTER_H

#include <string>

#include "ScriptFileWatcherWin32.h"
#include "ScriptModEntryScanner.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

bool ScriptHotReloadEventBelongsToEntry(const std::wstring &path, const ScriptModEntry &entry);
bool ScriptHotReloadEventLooksRelevant(const ScriptFileWatcherWin32::Event &event, const ScriptModEntry &entry);
bool ScriptHotReloadOverflowCanAffectEntry(const ScriptFileWatcherWin32::Event &event, const ScriptModEntry &entry);
bool ScriptHotReloadEventLooksRelevantToLibraryUse(const ScriptFileWatcherWin32::Event &event,
                                                   const ScriptLibraryUse &library);

} // namespace BML

#endif
