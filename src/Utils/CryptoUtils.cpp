#include "CryptoUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cstring>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace utils {
    namespace {
        constexpr size_t kSha256Length = 32;

        bool OpenHmacProvider(BCRYPT_ALG_HANDLE *provider) {
            return BCryptOpenAlgorithmProvider(
                provider,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                BCRYPT_ALG_HANDLE_HMAC_FLAG) == 0;
        }

        bool OpenSha256Provider(BCRYPT_ALG_HANDLE *provider) {
            return BCryptOpenAlgorithmProvider(
                provider,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                0) == 0;
        }
    } // namespace

    bool HmacSha256(const uint8_t *key, size_t keyLength,
                    const uint8_t *data, size_t dataLength,
                    uint8_t outDigest[32]) {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;

        if (!OpenHmacProvider(&algorithm)) {
            return false;
        }

        const PUCHAR keyBytes = keyLength == 0 ? nullptr : const_cast<PUCHAR>(key);
        const PUCHAR dataBytes = dataLength == 0 ? nullptr : const_cast<PUCHAR>(data);
        const ULONG keySize = static_cast<ULONG>(keyLength);
        const ULONG dataSize = static_cast<ULONG>(dataLength);

        NTSTATUS status = BCryptCreateHash(
            algorithm,
            &hash,
            nullptr,
            0,
            keyBytes,
            keySize,
            0);
        if (status != 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }

        status = BCryptHashData(hash, dataBytes, dataSize, 0);
        if (status == 0) {
            status = BCryptFinishHash(hash, outDigest, static_cast<ULONG>(kSha256Length), 0);
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return status == 0;
    }

    bool Sha256(const uint8_t *data, size_t dataLength, uint8_t outDigest[32]) {
        if (!outDigest || (!data && dataLength > 0)) {
            return false;
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;

        if (!OpenSha256Provider(&algorithm)) {
            return false;
        }

        NTSTATUS status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
        if (status != 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }

        status = BCryptHashData(
            hash,
            dataLength == 0 ? nullptr : const_cast<PUCHAR>(data),
            static_cast<ULONG>(dataLength),
            0);
        if (status == 0) {
            status = BCryptFinishHash(hash, outDigest, static_cast<ULONG>(kSha256Length), 0);
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return status == 0;
    }

    bool Sha256File(const std::wstring &path, uint8_t outDigest[32]) {
        if (!outDigest) {
            return false;
        }

        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (!OpenSha256Provider(&algorithm)) {
            CloseHandle(file);
            return false;
        }
        NTSTATUS status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
        if (status != 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            CloseHandle(file);
            return false;
        }

        std::array<uint8_t, 64 * 1024> buffer{};
        while (true) {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                status = -1;
                break;
            }
            if (read == 0) {
                break;
            }
            status = BCryptHashData(hash, buffer.data(), read, 0);
            if (status != 0) {
                break;
            }
        }

        if (status == 0) {
            status = BCryptFinishHash(hash, outDigest, static_cast<ULONG>(kSha256Length), 0);
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        CloseHandle(file);
        return status == 0;
    }

    std::vector<uint8_t> Sha256Bytes(const uint8_t *data, size_t dataLength) {
        std::vector<uint8_t> digest(kSha256Length);
        if (!Sha256(data, dataLength, digest.data())) {
            digest.clear();
        }
        return digest;
    }

    std::vector<uint8_t> Sha256FileBytes(const std::wstring &path) {
        std::vector<uint8_t> digest(kSha256Length);
        if (!Sha256File(path, digest.data())) {
            digest.clear();
        }
        return digest;
    }

    bool VerifyEcdsaP256Sha256RawSignature(const uint8_t publicKeyX[32],
                                           const uint8_t publicKeyY[32],
                                           const uint8_t digest[32],
                                           const uint8_t signature[64]) {
        if (!publicKeyX || !publicKeyY || !digest || !signature) {
            return false;
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) {
            return false;
        }

        struct PublicBlob {
            BCRYPT_ECCKEY_BLOB header;
            uint8_t x[32];
            uint8_t y[32];
        } blob{};
        blob.header.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
        blob.header.cbKey = 32;
        std::memcpy(blob.x, publicKeyX, 32);
        std::memcpy(blob.y, publicKeyY, 32);

        NTSTATUS status = BCryptImportKeyPair(
            algorithm,
            nullptr,
            BCRYPT_ECCPUBLIC_BLOB,
            &key,
            reinterpret_cast<PUCHAR>(&blob),
            sizeof(blob),
            0);
        if (status == 0) {
            status = BCryptVerifySignature(
                key,
                nullptr,
                const_cast<PUCHAR>(digest),
                static_cast<ULONG>(kSha256Length),
                const_cast<PUCHAR>(signature),
                64,
                0);
        }

        if (key) {
            BCryptDestroyKey(key);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return status == 0;
    }

    bool HkdfSha256(const uint8_t *ikm, size_t ikmLength,
                    const uint8_t *salt, size_t saltLength,
                    const uint8_t *info, size_t infoLength,
                    uint8_t *outBytes, size_t outLength) {
        if (!outBytes) {
            return false;
        }

        if (outLength == 0) {
            return true;
        }

        if (outLength > 255 * kSha256Length) {
            return false;
        }

        std::array<uint8_t, kSha256Length> zeroSalt = {};
        const uint8_t *effectiveSalt = saltLength == 0 ? zeroSalt.data() : salt;
        const size_t effectiveSaltLength = saltLength == 0 ? zeroSalt.size() : saltLength;

        std::array<uint8_t, kSha256Length> prk = {};
        if (!HmacSha256(effectiveSalt, effectiveSaltLength, ikm, ikmLength, prk.data())) {
            return false;
        }

        std::array<uint8_t, kSha256Length> previous = {};
        size_t produced = 0;
        uint8_t counter = 1;
        size_t previousLength = 0;

        while (produced < outLength) {
            std::vector<uint8_t> blockInput;
            blockInput.reserve(previousLength + infoLength + 1);
            blockInput.insert(blockInput.end(), previous.begin(), previous.begin() + previousLength);
            if (info && infoLength > 0) {
                blockInput.insert(blockInput.end(), info, info + infoLength);
            }
            blockInput.push_back(counter);

            if (!HmacSha256(prk.data(), prk.size(),
                            blockInput.data(), blockInput.size(),
                            previous.data())) {
                return false;
            }

            size_t copySize = outLength - produced;
            if (copySize > previous.size()) {
                copySize = previous.size();
            }
            std::memcpy(outBytes + produced, previous.data(), copySize);
            produced += copySize;
            previousLength = previous.size();
            ++counter;
        }

        return true;
    }

    uint64_t KeyToSeed(const uint8_t *key, size_t keyLength) {
        uint64_t seed = 0;
        size_t bytesToFold = keyLength;
        const size_t maxBytes = sizeof(uint64_t) * 2;
        if (bytesToFold > maxBytes) {
            bytesToFold = maxBytes;
        }
        for (size_t i = 0; i < bytesToFold; ++i) {
            const size_t shift = (i % sizeof(uint64_t)) * 8;
            seed ^= static_cast<uint64_t>(key[i]) << shift;
        }
        return seed;
    }
} // namespace utils
