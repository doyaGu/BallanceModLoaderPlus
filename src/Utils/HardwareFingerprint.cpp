#include "HardwareFingerprint.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

namespace utils {
    HardwareFingerprint GetHardwareFingerprint() {
        HardwareFingerprint fp = {};

        // Read MachineGuid from registry
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\Microsoft\\Cryptography",
                          0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
            return fp;
        }

        char guid[128] = {};
        DWORD guidSize = sizeof(guid);
        DWORD type = 0;
        LSTATUS status = RegQueryValueExA(hKey, "MachineGuid", nullptr, &type,
                                           reinterpret_cast<LPBYTE>(guid), &guidSize);
        RegCloseKey(hKey);

        if (status != ERROR_SUCCESS || type != REG_SZ || guidSize == 0) {
            return fp;
        }

        // SHA-256 via BCrypt
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        uint8_t digest[32] = {};

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
            return fp;

        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return fp;
        }

        size_t guidLen = strlen(guid);
        NTSTATUS s1 = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(guid), static_cast<ULONG>(guidLen), 0);
        NTSTATUS s2 = BCryptFinishHash(hHash, digest, sizeof(digest), 0);

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        if (s1 != 0 || s2 != 0)
            return fp;

        memcpy(fp.hash, digest, 6);
        return fp;
    }
} // namespace utils
