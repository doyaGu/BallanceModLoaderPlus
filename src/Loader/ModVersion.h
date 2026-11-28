#ifndef BML_MODVERSION_H
#define BML_MODVERSION_H

#include <cstdio>
#include <string>
#include "BML/Version.h"

struct BMLVersion {
    int major, minor, patch;

    BMLVersion() : major(BML_MAJOR_VERSION), minor(BML_MINOR_VERSION), patch(BML_PATCH_VERSION) {}

    BMLVersion(int mj, int mn, int bd) : major(mj), minor(mn), patch(bd) {}

    bool operator<(const BMLVersion &o) const {
        if (major == o.major) {
            if (minor == o.minor)
                return patch < o.patch;
            return minor < o.minor;
        }
        return major < o.major;
    }

    bool operator>=(const BMLVersion &o) const { return !(*this < o); }

    bool operator==(const BMLVersion &o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }

    std::string ToString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
        return buf;
    }
};

// Host-owned declarations are values. Only NativeMod projects them into borrowed
// C strings at the bml.mod protocol boundary.
struct ModDependency {
    std::string Id;
    BMLVersion MinVersion{0, 0, 0};
    bool Optional = false;
};

#endif
