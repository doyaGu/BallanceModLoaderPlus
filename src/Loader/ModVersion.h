#ifndef BML_MODVERSION_H
#define BML_MODVERSION_H

#include <cstdio>
#include <cstring>
#include <string>
#include "BML/BML.h"

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

// One entry of a Mod's dependency list, which the dependency registry reports a
// copy of. The loader builds entries from native or script declarations. The
// comparison operators look at id alone and ignore minVersion and optional.
struct ModDependency {
    char *id;
    BMLVersion minVersion;
    int optional; // (0 = required, 1 = optional)

    ModDependency() : id(nullptr), optional(0) {}

    ModDependency(const char *modId, const BMLVersion &version, int isOptional = 0)
        : minVersion(version), optional(isOptional) {
        if (modId && strlen(modId) > 0) {
            size_t len = strlen(modId);
            id = static_cast<char *>(BML_Malloc(len + 1));
            if (id) {
                memcpy(id, modId, len);
                id[len] = '\0';
            }
        } else {
            id = nullptr;
        }
    }

    ModDependency(const ModDependency &other)
        : minVersion(other.minVersion), optional(other.optional) {
        if (other.id && strlen(other.id) > 0) {
            size_t len = strlen(other.id);
            id = static_cast<char *>(BML_Malloc(len + 1));
            if (id) {
                memcpy(id, other.id, len);
                id[len] = '\0';
            }
        } else {
            id = nullptr;
        }
    }

    ModDependency &operator=(const ModDependency &other) {
        if (this != &other) {
            if (id) {
                BML_Free(id);
                id = nullptr;
            }

            if (other.id && strlen(other.id) > 0) {
                size_t len = strlen(other.id);
                id = static_cast<char *>(BML_Malloc(len + 1));
                if (id) {
                    memcpy(id, other.id, len);
                    id[len] = '\0';
                }
            }

            minVersion = other.minVersion;
            optional = other.optional;
        }
        return *this;
    }

    ~ModDependency() {
        if (id) {
            BML_Free(id);
            id = nullptr;
        }
    }

    int operator==(const ModDependency &other) const {
        if (id == nullptr || other.id == nullptr)
            return id == other.id;
        return strcmp(id, other.id) == 0;
    }

    int operator!=(const ModDependency &other) const { return !(*this == other); }
};

#endif
