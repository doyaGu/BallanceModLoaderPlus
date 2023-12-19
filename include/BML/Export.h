#ifndef BML_EXPORT_H
#define BML_EXPORT_H

#ifndef BML_EXPORT
#ifdef BML_EXPORTS
#define BML_EXPORT __declspec(dllexport)
#else
#define BML_EXPORT __declspec(dllimport)
#endif
#endif

#define MOD_EXPORT extern "C" __declspec(dllexport)

#endif // BML_EXPORT_H