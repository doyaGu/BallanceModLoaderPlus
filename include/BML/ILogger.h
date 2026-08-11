// Where a Mod writes to the loader's log. Get the one instance from
// IMod::GetLogger; the Mod owns it and never deletes it.
//
// Each call appends one line to ModLoader.log in the loader directory, and to
// stdout as well in a debug build of the loader, prefixed with the local time and
// with the Mod's id and the level, as in
//
//     [08/11/2026 21:04:07.512] [MyMod/INFO]: loaded 3 maps
//
// The three functions differ in that word alone. Nothing is filtered out and there
// is no level to turn off, so everything a Mod logs reaches the file.
//
// fmt is a printf format string, handed to the loader's C runtime with no argument
// checking: %s wants a const char *, so pass str.c_str() rather than a std::string,
// and a mismatched format crashes the game rather than logging anything. A null fmt
// writes an empty line. The whole line is written and flushed before the call
// returns, which is a disk write, so do not log every frame.
//
// The line is written as several separate writes, so a Mod logging from a thread of
// its own can interleave with the loader's own lines. Log from the game thread.
#ifndef BML_ILOGGER_H
#define BML_ILOGGER_H

#include "BML/Defines.h"

class BML_EXPORT ILogger {
public:
	virtual void Info(const char* fmt, ...) = 0;
	virtual void Warn(const char* fmt, ...) = 0;
	virtual void Error(const char* fmt, ...) = 0;

    virtual ~ILogger() = default;
};

#endif // BML_ILOGGER_H