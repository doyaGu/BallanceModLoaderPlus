#ifndef BML_MAPCATALOG_H
#define BML_MAPCATALOG_H

#include <memory>
#include <string>
#include <vector>

class ILogger;

enum MapEntryType {
    MAP_ENTRY_DIR,
    MAP_ENTRY_FILE,
};

struct MapEntry {
    MapEntry *parent = nullptr;
    MapEntryType type;
    std::string name;
    std::wstring path;
    std::vector<MapEntry *> children;
    bool m_BeingDeleted = false;

    explicit MapEntry(MapEntry *parent, MapEntryType entryType) : parent(parent), type(entryType) {}

    ~MapEntry();

    MapEntry(const MapEntry &) = delete;
    MapEntry &operator=(const MapEntry &) = delete;
    MapEntry(MapEntry &&) = delete;
    MapEntry &operator=(MapEntry &&) = delete;

    bool operator<(const MapEntry &rhs) const;
    bool operator>(const MapEntry &rhs) const { return rhs < *this; }
    bool operator<=(const MapEntry &rhs) const { return !(rhs < *this); }
    bool operator>=(const MapEntry &rhs) const { return !(*this < rhs); }
};

class MapCatalog {
public:
    MapCatalog();

    MapEntry *GetRoot() const { return m_Root.get(); }
    bool Refresh(const std::wstring &path, int maxDepth, ILogger *logger);

private:
    enum class ScanResult {
        Success,
        Failure,
    };

    static ScanResult ExploreMaps(MapEntry *maps, int depth, ILogger *logger);
    static bool IsSupportedFileType(const std::wstring &path);

    std::unique_ptr<MapEntry> m_Root;
};

#endif // BML_MAPCATALOG_H
