#include "Util.h"

#include <string.h>
#include <sys/stat.h>
#include <sstream>
#include <iomanip>

std::vector<std::string> SplitString(const std::string &str, const std::string &de) {
    size_t lpos, pos = 0;
    std::vector<std::string> res;

    lpos = str.find_first_not_of(de, pos);
    while (lpos != std::string::npos) {
        pos = str.find_first_of(de, lpos);
        res.push_back(str.substr(lpos, pos - lpos));
        if (pos == std::string::npos)
            break;

        lpos = str.find_first_not_of(de, pos);
    }

    if (pos != std::string::npos)
        res.emplace_back("");

    return res;
}

bool StartWith(const std::string &str, const std::string &start) {
    return str.substr(0, start.size()) == start;
}

bool IsFileExist(const char *file) {
    static struct stat buf = {0};
    return stat(file, &buf) == 0;
}

int CompareFileExtension(const char *file, const char *ext) {
    if (!file || strlen(file) < 2) return false;
    if (!ext || strlen(ext) < 2 || ext[0] != '.') return false;
    const char *p = strrchr(file, '.');
    if (!p) return false;
    return stricmp(p, ext);
}

bool IsVirtoolsFile(const char *file) {
    static const char *extensions[] = {
        ".cmo",
        ".nmo",
        ".vmo",
    };
    for (auto ext : extensions) {
        if (CompareFileExtension(file, ext) == 0) return true;
    }
    return false;
}

bool IsTextureFile(const char *file) {
    static const char *extensions[] = {
        ".bmp",
        ".jpg",
        ".jpeg",
        ".gif",
        ".tif",
        ".png",
        ".tga",
        ".dds",
        ".svg",
        ".raw",
    };
    for (auto ext : extensions) {
        if (CompareFileExtension(file, ext) == 0) return true;
    }
    return false;
}

bool IsSoundFile(const char *file) {
    static const char *extensions[] = {
        ".wav",
        ".mp3",
        ".wma",
        ".midi",
        ".ogg",
        ".ape",
        ".flac",
        ".acc",
    };
    for (auto ext : extensions) {
        if (CompareFileExtension(file, ext) == 0) return true;
    }
    return false;
}