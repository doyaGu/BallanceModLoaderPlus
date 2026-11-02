#include "PathUtils.h"
#include "StringUtils.h"
#include "CryptoUtils.h"
#include "Platform/FileError.h"
#include "Platform/Module.h"
#include "Platform/ModAbi.h"
#include <SDL3/SDL.h>
#include <zip.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

static void Require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

int main(int argc, char **argv) {
    std::wstring root;
    try {
        Require(argc == 3, "fixture path");
        const auto marker = utils::CreateTempFileW(L"bml-platform-");
        Require(!marker.empty(), "exclusive temporary file");
        root = marker + L".dir";
        Require(utils::CreateFileTreeW(root), "create test root");
        Require(utils::DeleteFileW(marker), "remove marker");
        const auto directory = utils::CombinePathW(root, L"\u4e2d\u6587-\U0001f3ae");
        Require(utils::CreateFileTreeW(directory), "Unicode directory");
        const auto path = utils::CombinePathW(directory, L"config.txt");
        std::uint32_t error = 0;
        Require(utils::WriteFileBytesAtomicW(path, "before", error, false), "publish new config");
        Require(!utils::WriteFileBytesAtomicW(path, "clobber", error, false), "must not replace existing config");
        Require(error == BML::Platform::FileError::FileExists || error == BML::Platform::FileError::AlreadyExists, "existing-file error");
        std::string bytes;
        Require(utils::ReadFileBytesW(path, bytes) && bytes == "before", "failed publication preserves config");
        Require(utils::WriteFileBytesAtomicW(path, "after", error), "replace config");
        Require(utils::ReadFileBytesW(path, bytes) && bytes == "after", "replacement data");
        Require(utils::WriteFileBytesAtomicW(path, {}, error), "empty config");
        Require(utils::ReadFileBytesW(path, bytes) && bytes.empty(), "read empty config");
        Require(utils::ListFilesW(directory).size() == 1, "temporary files cleaned");
        Require(!utils::ReadFileBytesW(path + L".missing", bytes, error) && error == BML::Platform::FileError::NotFound, "missing-file classification");
        Require(!utils::WriteFileBytesAtomicW(utils::CombinePathW(root, L"absent/file"), "x", error), "missing parent write fails");
        std::wstring resolved;
        Require(utils::TryGetFinalPathInsideRootW(path, root, resolved), "physical containment");
        Require(!utils::IsPathInsideRootW(root + L"-sibling", root), "root prefix is not containment");
        Require(utils::MakeRelativePathW(path, directory) == L"config.txt", "relative file");
        Require(utils::MakeRelativePathW(directory, utils::CombinePathW(directory, L"child")) == L"..", "relative ancestor");
        Require(utils::Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 known vector");
        Require(utils::WriteFileBytesAtomicW(path, "abc", error), "hash fixture");
        Require(utils::Sha256FileHex(path) == utils::Sha256Hex("abc"), "SDL streaming hash");
        const auto archive = utils::CombinePathW(root, L"good.zip");
        zip_t *zip = zip_open(utils::Utf16ToUtf8(archive).c_str(), 0, 'w');
        Require(zip != nullptr, "create zip fixture");
        Require(zip_entry_open(zip, "nested/resource.txt") == 0 && zip_entry_write(zip, "hello", 5) == 0, "zip resource");
        zip_entry_close(zip);
        Require(zip_entry_open(zip, "empty/") == 0, "empty ZIP directory");
        zip_entry_close(zip); zip_close(zip);
        const auto unpack = utils::CombinePathW(root, L"unpacked");
        Require(utils::ExtractZipW(archive, unpack), "SDL zip extraction");
        Require(utils::DirectoryExistsW(utils::CombinePathW(unpack, L"empty")), "preserve empty ZIP directory");
        Require(utils::ReadFileBytesW(utils::CombinePathW(unpack, L"nested/resource.txt"), bytes) && bytes == "hello", "extracted data");
        zip = zip_open(utils::Utf16ToUtf8(archive).c_str(), 0, 'w');
        Require(zip != nullptr, "create traversal fixture");
        zip_entry_open(zip, "../escaped.txt"); zip_entry_write(zip, "bad", 3); zip_entry_close(zip); zip_close(zip);
        Require(!utils::ExtractZipW(archive, unpack), "reject traversal");
        Require(!utils::PathExistsW(utils::CombinePathW(root, L"escaped.txt")), "no traversal output");
#ifndef _WIN32
        const auto alias = utils::CombinePathW(directory, L"outside");
        Require(::symlink("/tmp", utils::Utf16ToUtf8(alias).c_str()) == 0, "symlink fixture");
        Require(!utils::TryGetFinalPathInsideRootW(alias, root, resolved), "reject symlink escape");
#endif
        std::string descriptor;
        Require(utils::ReadFileBytesUtf8(argv[2], descriptor), "host ABI descriptor");
        const auto modPath = utils::CombinePathW(root, L"example.bmodp");
        std::string abiError;
        Require(!BML::Platform::ValidateModAbi(modPath, abiError), "missing ABI rejected");
        Require(utils::WriteFileBytesAtomicW(modPath + L".abi.json", descriptor, error), "write ABI");
        Require(BML::Platform::ValidateModAbi(modPath, abiError), "matching ABI accepted");
        const auto runtimeAt = descriptor.find("\"runtime\":\"sdl\"");
        Require(runtimeAt != std::string::npos, "SDL descriptor");
        descriptor.replace(runtimeAt, 15, "\"runtime\":\"win32\"");
        Require(utils::WriteFileBytesAtomicW(modPath + L".abi.json", descriptor, error), "write wrong ABI");
        Require(!BML::Platform::ValidateModAbi(modPath, abiError) && abiError.find("runtime") != std::string::npos, "wrong runtime rejected");
        Require(utils::WriteFileBytesAtomicW(modPath + L".abi.json", "{", error), "write malformed ABI");
        Require(!BML::Platform::ValidateModAbi(modPath, abiError), "malformed ABI rejected");
        std::string loadError;
        const auto first = BML::Platform::Module::Open(argv[1], loadError);
        const auto second = BML::Platform::Module::Open(argv[1], loadError);
        Require(first && second, loadError.c_str());
        auto entry = first->Symbol("BMLEntry");
        Require(entry && first->Identity() == second->Identity(), "duplicate loads share identity");
        Require(first->Identity() == BML::Platform::ModuleFromAddress(entry), "address ownership");
        Require(reinterpret_cast<int (*)()>(entry)() == 42, "native call");
        Require(!first->Symbol("missing_symbol"), "missing symbol");
        auto reference = BML::Platform::RetainModule(first);
        Require(reference.get() == first->Identity(), "registry uses image identity");
        Require(BML::Platform::ModuleSymbol(reference, "BMLEntry") == entry, "symbol lookup uses retained load handle");
        Require(utils::DeleteDirectoryW(root), "cleanup without following symlinks");
        std::cout << "SDL filesystem, module identity and crypto contracts passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        if (!root.empty()) utils::DeleteDirectoryW(root);
        return 1;
    }
}
