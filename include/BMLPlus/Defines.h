#ifndef BML_DEFINES_H
#define BML_DEFINES_H

#include "Vx2dVector.h"
#include "VxColor.h"
#include "CKDefines.h"

#include "Export.h"
#include "Version.h"
#include "Guids.h"

#define TOSTRING2(arg) #arg
#define TOSTRING(arg) TOSTRING2(arg)

#define TOCKSTRING(str) const_cast<CKSTRING>(str)

typedef const char *CSTRING;

struct BMLVersion {
    int major, minor, build;

    BMLVersion() : major(BML_MAJOR_VER), minor(BML_MINOR_VER), build(BML_PATCH_VER) {}
    BMLVersion(int mj, int mn, int bd) : major(mj), minor(mn), build(bd) {}

    bool operator<(const BMLVersion &o) const {
        if (major == o.major) {
            if (minor == o.minor)
                return build < o.build;
            return minor < o.minor;
        }
        return major < o.major;
    }

    bool operator>=(const BMLVersion &o) const {
        return !(*this < o);
    }
};

#endif // BML_DEFINES_H
