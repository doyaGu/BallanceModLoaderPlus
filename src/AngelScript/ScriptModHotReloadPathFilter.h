#ifndef BML_SCRIPTMODHOTRELOADPATHFILTER_H
#define BML_SCRIPTMODHOTRELOADPATHFILTER_H

#include <string>

#include "ScriptFileWatcher.h"
#include "ScriptModEntryScanner.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

bool ScriptHotReloadEventBelongsToEntry(const std::wstring &path, const ScriptModEntry &entry);
bool ScriptHotReloadEventLooksRelevant(const ScriptFileWatcher::Event &event, const ScriptModEntry &entry);
bool ScriptHotReloadOverflowCanAffectEntry(const ScriptFileWatcher::Event &event, const ScriptModEntry &entry);
bool ScriptHotReloadEventLooksRelevantToLibraryUse(const ScriptFileWatcher::Event &event,
                                                   const ScriptLibraryUse &library);

} // namespace BML

#endif
