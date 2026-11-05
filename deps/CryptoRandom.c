/* OS entropy adapter required by p256-m. Fail closed on every error. */
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
int p256_generate_random(uint8_t *output, unsigned size) {
    return BCryptGenRandom(NULL, output, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0 ? -1 : 0;
}
#elif defined(__APPLE__)
#include <Security/SecRandom.h>
int p256_generate_random(uint8_t *output, unsigned size) {
    return SecRandomCopyBytes(kSecRandomDefault, size, output) == errSecSuccess ? 0 : -1;
}
#else
#include <sys/random.h>
#include <errno.h>
int p256_generate_random(uint8_t *output, unsigned size) {
    while (size) {
        const ssize_t count = getrandom(output, size, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        output += count;
        size -= (unsigned)count;
    }
    return 0;
}
#endif
