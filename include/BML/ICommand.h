// One command for the loader's command bar, which the player opens with the / key.
// A Mod derives from this, allocates one instance, and hands it to
// IBML::RegisterCommand from OnLoad. The loader keeps the pointer, never deletes
// it, and has no way to take it back, so allocate it once and let it live for the
// whole process; this class has no virtual destructor either.
//
// The loader asks the three name functions again on every listing and every Tab
// press, so return the same text each time and build it cheaply. GetName, GetAlias,
// and GetDescription return std::string by value and Execute takes a
// std::vector<std::string>, so a Mod implementing this is tied to the standard
// library the loader was built with, which is why the loader and its Mods have to
// use the same MSVC runtime.
//
// Everything here is called on the game thread, from the command bar or from
// IBML::ExecuteCommand.
#ifndef BML_ICOMMAND_H
#define BML_ICOMMAND_H

#include <cfloat>
#include <climits>
#include <vector>
#include <string>
#include <algorithm>

#include "BML/Defines.h"

class IBML;

class BML_EXPORT ICommand {
public:
    // What the player types. It has to be at most 255 bytes and valid UTF-8, start
    // with an ASCII letter or an underscore, and hold nothing but ASCII letters,
    // digits, underscore, hyphen, and dot, except that codepoints above 127 are
    // allowed anywhere. Registration fails and logs an error otherwise.
    //
    // The loader looks names up case-insensitively, folding ASCII and UTF-8 alike,
    // so a name differing from another command's only in case counts as already
    // taken and registration fails. Prefix the name of anything Mod-specific with
    // the Mod's own word, since the table is shared with every other Mod.
    virtual std::string GetName() = 0;

    // A second word for the same command, or an empty string for none. Only the
    // length, the UTF-8 validity, and the absence of spaces and control characters
    // are checked here, so an alias may hold characters a name may not. An invalid
    // alias fails the whole registration; an alias already taken by another command
    // only logs a warning and leaves this command registered under its name alone.
    virtual std::string GetAlias() = 0;

    // The one-line explanation the help command prints after the name. Free text.
    virtual std::string GetDescription() = 0;

    // Whether this command needs cheats on. The loader checks it before running
    // anything: with cheats off it writes "Can not execute cheat command" as an
    // ingame message and neither Execute nor the Mods' OnPreCommandExecute and
    // OnPostCommandExecute run. Do not check IBML::IsCheatEnabled again in Execute.
    virtual bool IsCheat() = 0;

    // Runs the command. args[0] is the word the player typed, so it is the alias
    // when that is what was used, and the arguments start at args[1]. The line is
    // split on ASCII whitespace with no quoting and no escapes, so an argument
    // cannot contain a space and a quoted string arrives as several args; join them
    // back if the command wants one text. Write output with
    // IBML::SendIngameMessage. An exception thrown here is caught by the loader,
    // logged, and shown to the player, and it skips the OnPostCommandExecute
    // broadcast.
    virtual void Execute(IBML *bml, const std::vector<std::string> &args) = 0;

    // Asked when the player presses Tab. args is the line up to the caret split the
    // same way, with args[0] the command word, and a caret sitting after a space
    // adds an empty last element, so args.size() is 2 while the first argument is
    // being completed, 3 for the second, and so on. Return every candidate for that
    // position, in any order: the command bar keeps the ones that start with what
    // has been typed, ignoring case, and drops duplicates. Return an empty vector
    // for a position with nothing to offer. This runs inside the loader's ImGui
    // frame while the player waits, so do not read files or block here.
    virtual const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) = 0;

    // Argument helpers, all of them silent about a bad argument. ParseInteger and
    // ParseFloat go through atoi and atof, which yield 0 for text that is not a
    // number, and then clamp into mn to mx, so "abc" and "0" are indistinguishable
    // and so are "999999" and mx. Validate the string first when the difference
    // matters. ParseBoolean is true for exactly "true", "on", and "1", lowercase
    // only, and false for everything else including "TRUE" and "yes".
    static int ParseInteger(const std::string &str, int mn = INT_MIN, int mx = INT_MAX) {
        return (std::max)(mn, (std::min)(mx, atoi(str.c_str())));
    }
    static float ParseFloat(const std::string &str, float mn = -FLT_MAX, float mx = FLT_MAX) {
        return (std::max)(mn, (std::min)(mx, (float) atof(str.c_str())));
    }
    static bool ParseBoolean(const std::string &str) {
        return str == "true" || str == "on" || str == "1";
    }
};

#endif // BML_ICOMMAND_H