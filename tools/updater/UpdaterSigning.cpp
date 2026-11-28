#include "UpdaterSigning.h"
#include <p256-m.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace bmlupdater {
namespace {
using Bytes = std::span<const uint8_t>;
void Erase(std::span<uint8_t> bytes) {
    volatile uint8_t *data = bytes.data();
    for (size_t i = 0; i < bytes.size(); ++i) data[i] = 0;
}
struct EraseVector {
    std::vector<uint8_t> &Bytes;
    ~EraseVector() { Erase(Bytes); }
};
[[noreturn]] void Invalid() { throw std::runtime_error("Expected an unencrypted P-256 SEC1 or PKCS#8 private key"); }
class Der {
public:
    explicit Der(Bytes bytes) : m_Bytes(bytes) {}
    bool Empty() const { return m_Bytes.empty(); }
    uint8_t Tag() const { if (Empty()) Invalid(); return m_Bytes.front(); }
    Bytes Read(uint8_t tag) {
        if (m_Bytes.size() < 2 || m_Bytes[0] != tag) Invalid();
        size_t header = 2, size = m_Bytes[1];
        if (size & 128) {
            const size_t count = size & 127;
            if (!count || count > sizeof(size_t) || count > m_Bytes.size() - header || !m_Bytes[header]) Invalid();
            size = 0;
            for (size_t i = 0; i < count; ++i) size = (size << 8) | m_Bytes[header++];
            if (size < 128) Invalid();
        }
        if (size > m_Bytes.size() - header) Invalid();
        const auto value = m_Bytes.subspan(header, size);
        m_Bytes = m_Bytes.subspan(header + size);
        return value;
    }
    void End() const { if (!Empty()) Invalid(); }
private:
    Bytes m_Bytes;
};
bool Equal(Bytes a, Bytes b) { return std::equal(a.begin(), a.end(), b.begin(), b.end()); }
constexpr uint8_t CurveOid[] = {0x2a,0x86,0x48,0xce,0x3d,3,1,7};
constexpr uint8_t EcOid[] = {0x2a,0x86,0x48,0xce,0x3d,2,1};
std::vector<uint8_t> DecodePem(Bytes encoded) {
    std::string_view text(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    if (!text.starts_with("-----BEGIN ")) return {encoded.begin(), encoded.end()};
    const auto line = text.find("-----", 11);
    if (line == std::string_view::npos) Invalid();
    const auto label = text.substr(11, line - 11);
    if (label != "EC PRIVATE KEY" && label != "PRIVATE KEY") Invalid();
    const std::string footer = "-----END " + std::string(label) + "-----";
    const auto end = text.find(footer, line + 5);
    if (end == std::string_view::npos) Invalid();
    std::string base64;
    for (const char c : text.substr(line + 5, end - line - 5)) {
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        base64 += c;
    }
    for (const char c : text.substr(end + footer.size()))
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t') Invalid();
    if (base64.empty() || base64.size() % 4) Invalid();
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> result;
    for (size_t i = 0; i < base64.size(); i += 4) {
        uint32_t value = 0;
        unsigned padding = 0;
        for (size_t j = 0; j < 4; ++j) {
            const char c = base64[i + j];
            const auto digit = alphabet.find(c);
            if (c == '=') { if (j < 2 || i + 4 != base64.size()) Invalid(); ++padding; value <<= 6; }
            else { if (padding || digit == std::string_view::npos) Invalid(); value = (value << 6) | static_cast<uint32_t>(digit); }
        }
        if (padding > 2 || (padding == 1 && (value & 0xff)) || (padding == 2 && (value & 0xffff))) Invalid();
        result.push_back(static_cast<uint8_t>(value >> 16));
        if (padding < 2) result.push_back(static_cast<uint8_t>(value >> 8));
        if (!padding) result.push_back(static_cast<uint8_t>(value));
    }
    return result;
}
void ReadSec1(Der &key, bool knownCurve, std::array<uint8_t, 32> &scalar, std::vector<uint8_t> &point) {
    const auto secret = key.Read(4);
    if (secret.size() != scalar.size()) Invalid();
    std::copy(secret.begin(), secret.end(), scalar.begin());
    if (!key.Empty() && key.Tag() == 0xa0) {
        Der params(key.Read(0xa0));
        if (!Equal(params.Read(6), CurveOid)) Invalid();
        params.End(); knownCurve = true;
    }
    if (!knownCurve) Invalid();
    if (!key.Empty() && key.Tag() == 0xa1) {
        Der publicKey(key.Read(0xa1));
        const auto bits = publicKey.Read(3);
        if (bits.size() != 66 || bits[0] != 0 || bits[1] != 4) Invalid();
        point.assign(bits.begin() + 2, bits.end());
        publicKey.End();
    }
    key.End();
}
}
SigningKey::SigningKey(std::span<const uint8_t> encoded) {
    try {
        if (encoded.empty() || encoded.size() > 16384) Invalid();
        auto der = DecodePem(encoded);
        EraseVector erase{der};
        Der input(der), key(input.Read(0x30)); input.End();
        const auto version = key.Read(2);
        if (version.size() != 1 || version[0] > 1) Invalid();
        std::vector<uint8_t> point;
        if (version[0] == 0) {
            Der algorithm(key.Read(0x30));
            if (!Equal(algorithm.Read(6), EcOid) || !Equal(algorithm.Read(6), CurveOid)) Invalid();
            algorithm.End();
            Der privateKey(key.Read(4)); key.End();
            Der sec1(privateKey.Read(0x30)); privateKey.End();
            constexpr uint8_t one[] = {1};
            if (!Equal(sec1.Read(2), one)) Invalid();
            ReadSec1(sec1, true, m_Scalar, point);
        } else ReadSec1(key, false, m_Scalar, point);
        const uint8_t probe[32]{};
        const auto signature = SignDigest(probe);
        if (!point.empty() && p256_ecdsa_verify(signature.data(), point.data(), probe, sizeof(probe)) != P256_SUCCESS) Invalid();
    } catch (...) { Erase(m_Scalar); throw; }
}
SigningKey::~SigningKey() {
    Erase(m_Scalar);
}
std::array<uint8_t, 64> SigningKey::SignDigest(const uint8_t digest[32]) const {
    std::array<uint8_t, 64> signature{};
    if (!digest || p256_ecdsa_sign(signature.data(), m_Scalar.data(), digest, 32) != P256_SUCCESS)
        throw std::runtime_error("P-256 signing failed");
    return signature;
}
}
