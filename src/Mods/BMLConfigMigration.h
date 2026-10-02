#ifndef BML_MODS_CONFIG_MIGRATION_H
#define BML_MODS_CONFIG_MIGRATION_H

class ConfigData;

// Upgrade settings written by BML+ 0.3.13 before the built-in Mod binds its
// current properties. Returns false if an old font path cannot be translated.
// Repeated calls leave already upgraded values unchanged.
bool MigrateBMLConfig(ConfigData &values);

#endif // BML_MODS_CONFIG_MIGRATION_H
