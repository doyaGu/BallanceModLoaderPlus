#include "CryptoUtils.h"
#include "StringUtils.h"
#include "Sha256State.h"
#include <SDL3/SDL_iostream.h>
#include <array>
#include <cstring>
#include <memory>

namespace utils {
namespace {
constexpr size_t kSha256Length = 32;
std::string BytesToLowerHex(const uint8_t *bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '\0');
    for (size_t i = 0; i < size; ++i) {
        result[i * 2] = digits[bytes[i] >> 4];
        result[i * 2 + 1] = digits[bytes[i] & 15];
    }
    return result;
}
}
bool Sha256(const uint8_t *data, size_t size, uint8_t out[32]) {
    if (!out || (!data && size)) return false;
    detail::Sha256State state;
    state.Update(data, size);
    state.Finish(out);
    return true;
}
bool HmacSha256(const uint8_t *key, size_t keySize, const uint8_t *data, size_t size, uint8_t out[32]) {
    if (!out || (!key && keySize) || (!data && size)) return false;
    std::array<uint8_t, 64> pad{};
    if (keySize > pad.size()) {
        if (!Sha256(key, keySize, pad.data())) return false;
    } else if (keySize) std::memcpy(pad.data(), key, keySize);
    for (auto &byte : pad) byte ^= 0x36;
    detail::Sha256State inner;
    inner.Update(pad.data(), pad.size());
    inner.Update(data, size);
    std::array<uint8_t, 32> digest{};
    inner.Finish(digest.data());
    for (auto &byte : pad) byte ^= 0x36 ^ 0x5c;
    detail::Sha256State outer;
    outer.Update(pad.data(), pad.size());
    outer.Update(digest.data(), digest.size());
    outer.Finish(out);
    return true;
}
bool Sha256File(const std::wstring &path, uint8_t out[32]) {
    if (!out) return false;
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(
        SDL_IOFromFile(Utf16ToUtf8(path).c_str(), "rb"), SDL_CloseIO);
    if (!file) return false;
    detail::Sha256State state;
    std::array<uint8_t, 16384> buffer{};
    for (;;) {
        const size_t count = SDL_ReadIO(file.get(), buffer.data(), buffer.size());
        if (count == 0)
            break;
        state.Update(buffer.data(), count);
    }
    if (SDL_GetIOStatus(file.get()) != SDL_IO_STATUS_EOF) return false;
    state.Finish(out);
    return SDL_CloseIO(file.release());
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

    std::string Sha256Hex(const uint8_t *data, size_t dataLength) {
        std::array<uint8_t, kSha256Length> digest{};
        if (!Sha256(data, dataLength, digest.data()))
            return {};
        return BytesToLowerHex(digest.data(), digest.size());
    }

    std::string Sha256Hex(std::string_view data) {
        return Sha256Hex(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    }

    std::string Sha256FileHex(const std::wstring &path) {
        std::array<uint8_t, kSha256Length> digest{};
        if (!Sha256File(path, digest.data()))
            return {};
        return BytesToLowerHex(digest.data(), digest.size());
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
