#include "UpdaterSigning.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"
#include "Utils/Sha256State.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
struct EraseText {
    std::string &Text;
    ~EraseText() {
        volatile char *bytes = Text.data();
        for (size_t i = 0; i < Text.size(); ++i) bytes[i] = 0;
    }
};
int Run(const std::vector<std::string> &args) {
    if (args.size() != 3 || (args[1] != "sign" && args[1] != "check")) {
        std::cerr << "Usage: BMLUpdateSigner sign|check <P-256 key.pem or key.der>\n";
        return 2;
    }
    try {
        std::string encoded;
        EraseText erase{encoded};
        if (!utils::ReadFileBytesW(utils::Utf8ToUtf16(args[2]), encoded)) throw std::runtime_error("Cannot read signing key");
        bmlupdater::SigningKey key({reinterpret_cast<const uint8_t *>(encoded.data()), encoded.size()});
        if (args[1] == "check") return 0;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        utils::detail::Sha256State state;
        std::array<uint8_t, 16384> buffer{};
        while (std::cin) {
            std::cin.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            state.Update(buffer.data(), static_cast<size_t>(std::cin.gcount()));
        }
        if (!std::cin.eof()) throw std::runtime_error("Cannot read payload");
        std::array<uint8_t, 32> digest{};
        state.Finish(digest.data());
        const auto signature = key.SignDigest(digest.data());
        std::cout.write(reinterpret_cast<const char *>(signature.data()), signature.size());
        return std::cout ? 0 : 1;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(utils::Utf16ToUtf8(argv[i]));
    return Run(args);
}
#else
int main(int argc, char **argv) { return Run({argv, argv + argc}); }
#endif
