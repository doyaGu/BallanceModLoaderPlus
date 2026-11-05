#include "UpdaterSigning.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <stdexcept>
#include <vector>

// This executable supplies a failed entropy provider instead of the OS adapter.
extern "C" int p256_generate_random(uint8_t *, unsigned) { return -1; }
TEST(SigningTest, EntropyFailureCannotProduceASignature) {
    const std::vector<uint8_t> prefix = {0x30,0x31,2,1,1,4,0x20};
    auto key = prefix;
    key.insert(key.end(), 31, 0); key.push_back(1);
    const uint8_t curve[] = {0xa0,0x0a,6,8,0x2a,0x86,0x48,0xce,0x3d,3,1,7};
    key.insert(key.end(), std::begin(curve), std::end(curve));
    try {
        bmlupdater::SigningKey parsed(key);
        FAIL() << "Entropy failure accepted";
    } catch (const std::runtime_error &error) {
        EXPECT_STREQ(error.what(), "P-256 signing failed");
    }
}
