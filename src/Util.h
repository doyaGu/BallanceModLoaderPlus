#ifndef BML_UTIL_H
#define BML_UTIL_H

#include <string>
#include <vector>

std::vector<std::string> SplitString(const std::string &str, const std::string &de);

bool StartWith(const std::string &str, const std::string &start);

bool IsFileExist(const char *file);

int CompareFileExtension(const char *file, const char *ext);
bool IsVirtoolsFile(const char *file);
bool IsTextureFile(const char *file);
bool IsSoundFile(const char *file);

std::string Text2Pinyin(const std::string &text);

#endif // BML_UTIL_H