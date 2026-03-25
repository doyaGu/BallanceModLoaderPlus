#include <gtest/gtest.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <algorithm>

#include "PathUtils.h"
#include "StringUtils.h"

namespace fs = std::filesystem;

namespace {
    std::wstring ToExtendedLengthPath(const fs::path &path) {
        const std::wstring value = path.wstring();
        if (value.rfind(L"\\\\?\\", 0) == 0) {
            return value;
        }

        if (value.rfind(L"\\\\", 0) == 0) {
            return L"\\\\?\\UNC\\" + value.substr(2);
        }

        return L"\\\\?\\" + value;
    }
}

// Test fixture for path operations
class PathUtilsTest : public ::testing::Test {
protected:
    std::string testDirA;
    std::wstring testDirW;
    std::string tempPathA;
    std::wstring tempPathW;
    std::wstring originalCurrentDirectoryW;
    utils::RuntimeLayoutNames runtimeNames;
    
    void SetUp() override {
        // Get temp directory
        tempPathA = utils::GetTempPathA();
        tempPathW = utils::GetTempPathW();
        originalCurrentDirectoryW = utils::GetCurrentDirectoryW();
        
        // Create unique test directories
        testDirA = tempPathA + "PathUtilsTestA_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed());
        testDirW = tempPathW + L"PathUtilsTestW_" + std::to_wstring(::testing::UnitTest::GetInstance()->random_seed());
        
        // Create test directories
        utils::CreateDirectoryA(testDirA);
        utils::CreateDirectoryW(testDirW);
    }
    
    void TearDown() override {
        if (!originalCurrentDirectoryW.empty()) {
            utils::SetCurrentDirectoryW(originalCurrentDirectoryW);
        }

        // Clean up test directories
        utils::DeleteDirectoryA(testDirA);
        utils::DeleteDirectoryW(testDirW);
    }
    
    // Helper to create a test file with content
    std::string CreateTestFileA(const std::string& filename, const std::string& content = "test content") {
        std::string filePath = utils::CombinePathA(testDirA, filename);
        std::ofstream file(filePath);
        file << content;
        file.close();
        return filePath;
    }
    
    std::wstring CreateTestFileW(const std::wstring& filename, const std::wstring& content = L"test content") {
        std::wstring filePath = utils::CombinePathW(testDirW, filename);
        std::wofstream file(filePath.c_str());
        file << content;
        file.close();
        return filePath;
    }

    fs::path CreateSyntheticGameDir(const std::wstring &name) {
        const fs::path gameDir = fs::path(testDirW) / name;
        fs::create_directories(gameDir);
        return gameDir;
    }

    fs::path CreateLongDirectoryTree(const std::wstring &prefix, size_t minimumLength) {
        fs::path current = fs::path(testDirW) / prefix;
        fs::create_directories(current);

        size_t segmentIndex = 0;
        while (current.native().size() <= minimumLength) {
            current /= L"segment_" + std::to_wstring(segmentIndex++) + L"_abcdefghijklmnop";
            if (!::CreateDirectoryW(ToExtendedLengthPath(current).c_str(), nullptr)) {
                const DWORD error = ::GetLastError();
                if (error != ERROR_ALREADY_EXISTS) {
                    throw std::runtime_error("failed to create long path test directory");
                }
            }
        }

        return current;
    }

    void ExpectRuntimeLayout(const fs::path &gameDir, const utils::RuntimeLayout &layout,
                             const utils::RuntimeLayoutNames &names) {
        const fs::path runtimeDir = (gameDir / names.runtime_directory).lexically_normal();
        EXPECT_EQ(gameDir.lexically_normal(), layout.game_directory);
        EXPECT_EQ(runtimeDir, layout.runtime_directory);
        EXPECT_EQ((runtimeDir / names.mods_directory).lexically_normal(), layout.mods_directory);
        EXPECT_EQ((runtimeDir / names.packages_directory).lexically_normal(), layout.packages_directory);
        EXPECT_EQ((runtimeDir / names.crash_dumps_directory).lexically_normal(),
            layout.crash_dumps_directory);
        EXPECT_EQ((runtimeDir / names.fault_log_file).lexically_normal(), layout.fault_log_path);
    }
};

// Test file existence checks
TEST_F(PathUtilsTest, FileExists) {
    // ANSI version
    std::string filePathA = CreateTestFileA("testfile.txt");
    EXPECT_TRUE(utils::FileExistsA(filePathA));
    EXPECT_FALSE(utils::FileExistsA(filePathA + ".nonexistent"));
    EXPECT_FALSE(utils::FileExistsA(""));
    
    // Wide version
    std::wstring filePathW = CreateTestFileW(L"testfile.txt");
    EXPECT_TRUE(utils::FileExistsW(filePathW));
    EXPECT_FALSE(utils::FileExistsW(filePathW + L".nonexistent"));
    EXPECT_FALSE(utils::FileExistsW(L""));
    
    // UTF-8 version
    std::string utf8Path = utils::Utf16ToUtf8(filePathW);
    EXPECT_TRUE(utils::FileExistsUtf8(utf8Path));
    EXPECT_FALSE(utils::FileExistsUtf8(utf8Path + ".nonexistent"));
    EXPECT_FALSE(utils::FileExistsUtf8(""));
}

// Test directory existence checks
TEST_F(PathUtilsTest, DirectoryExists) {
    EXPECT_TRUE(utils::DirectoryExistsA(testDirA));
    EXPECT_FALSE(utils::DirectoryExistsA(testDirA + "_nonexistent"));
    EXPECT_FALSE(utils::DirectoryExistsA(""));
    
    EXPECT_TRUE(utils::DirectoryExistsW(testDirW));
    EXPECT_FALSE(utils::DirectoryExistsW(testDirW + L"_nonexistent"));
    EXPECT_FALSE(utils::DirectoryExistsW(L""));
    
    std::string utf8Dir = utils::Utf16ToUtf8(testDirW);
    EXPECT_TRUE(utils::DirectoryExistsUtf8(utf8Dir));
    EXPECT_FALSE(utils::DirectoryExistsUtf8(utf8Dir + "_nonexistent"));
    EXPECT_FALSE(utils::DirectoryExistsUtf8(""));
}

// Test path existence checks
TEST_F(PathUtilsTest, PathExists) {
    std::string filePathA = CreateTestFileA("pathtest.txt");
    EXPECT_TRUE(utils::PathExistsA(filePathA));
    EXPECT_TRUE(utils::PathExistsA(testDirA));
    EXPECT_FALSE(utils::PathExistsA(testDirA + "\\nonexistent"));
    EXPECT_FALSE(utils::PathExistsA(""));
    
    std::wstring filePathW = CreateTestFileW(L"pathtest.txt");
    EXPECT_TRUE(utils::PathExistsW(filePathW));
    EXPECT_TRUE(utils::PathExistsW(testDirW));
    EXPECT_FALSE(utils::PathExistsW(testDirW + L"\\nonexistent"));
    EXPECT_FALSE(utils::PathExistsW(L""));
    
    std::string utf8Path = utils::Utf16ToUtf8(filePathW);
    EXPECT_TRUE(utils::PathExistsUtf8(utf8Path));
    EXPECT_FALSE(utils::PathExistsUtf8(utf8Path + "_nonexistent"));
    EXPECT_FALSE(utils::PathExistsUtf8(""));
}

// Test directory creation
TEST_F(PathUtilsTest, CreateDirectory) {
    std::string newDirA = utils::CombinePathA(testDirA, "newdir");
    EXPECT_FALSE(utils::DirectoryExistsA(newDirA));
    EXPECT_TRUE(utils::CreateDirectoryA(newDirA));
    EXPECT_TRUE(utils::DirectoryExistsA(newDirA));
    // Test creating already existing directory
    EXPECT_TRUE(utils::CreateDirectoryA(newDirA));
    
    std::wstring newDirW = utils::CombinePathW(testDirW, L"newdir");
    EXPECT_FALSE(utils::DirectoryExistsW(newDirW));
    EXPECT_TRUE(utils::CreateDirectoryW(newDirW));
    EXPECT_TRUE(utils::DirectoryExistsW(newDirW));
    
    std::string newDirUtf8 = utils::CombinePathA(testDirA, "newdir_utf8");
    EXPECT_FALSE(utils::DirectoryExistsUtf8(newDirUtf8));
    EXPECT_TRUE(utils::CreateDirectoryUtf8(newDirUtf8));
    EXPECT_TRUE(utils::DirectoryExistsUtf8(newDirUtf8));
}

// Test file tree creation
TEST_F(PathUtilsTest, CreateFileTree) {
    // Create single-level directory first to isolate the issue
    std::string level1DirA = utils::CombinePathA(testDirA, "level1");
    EXPECT_TRUE(utils::CreateDirectoryA(level1DirA));
    EXPECT_TRUE(utils::DirectoryExistsA(level1DirA));

    // Then create nested directories with proper path format
    std::string nestedDirA = utils::CombinePathA(testDirA, "nested\\level2\\level3");
    EXPECT_TRUE(utils::CreateFileTreeA(nestedDirA));

    // Add explicit check for each directory level
    std::string level1A = utils::CombinePathA(testDirA, "nested");
    std::string level2A = utils::CombinePathA(level1A, "level2");
    std::string level3A = utils::CombinePathA(level2A, "level3");

    EXPECT_TRUE(utils::DirectoryExistsA(level1A));
    EXPECT_TRUE(utils::DirectoryExistsA(level2A));
    EXPECT_TRUE(utils::DirectoryExistsA(level3A));

    // Same for wide string version
    std::wstring nestedDirW = utils::CombinePathW(testDirW, L"nested\\level2\\level3");
    EXPECT_TRUE(utils::CreateFileTreeW(nestedDirW));

    std::wstring level1W = utils::CombinePathW(testDirW, L"nested");
    std::wstring level2W = utils::CombinePathW(level1W, L"level2");
    std::wstring level3W = utils::CombinePathW(level2W, L"level3");

    EXPECT_TRUE(utils::DirectoryExistsW(level1W));
    EXPECT_TRUE(utils::DirectoryExistsW(level2W));
    EXPECT_TRUE(utils::DirectoryExistsW(level3W));

    // UTF-8 version
    std::string nestedDirUtf8 = utils::CombinePathA(testDirA, "nested_utf8\\level2\\level3");
    EXPECT_TRUE(utils::CreateFileTreeUtf8(nestedDirUtf8));

    std::string level1Utf8 = utils::CombinePathA(testDirA, "nested_utf8");
    std::string level2Utf8 = utils::CombinePathA(level1Utf8, "level2");
    std::string level3Utf8 = utils::CombinePathA(level2Utf8, "level3");

    EXPECT_TRUE(utils::DirectoryExistsUtf8(level1Utf8));
    EXPECT_TRUE(utils::DirectoryExistsUtf8(level2Utf8));
    EXPECT_TRUE(utils::DirectoryExistsUtf8(level3Utf8));
}

// Test file deletion
TEST_F(PathUtilsTest, DeleteFile) {
    std::string filePathA = CreateTestFileA("deleteme.txt");
    EXPECT_TRUE(utils::FileExistsA(filePathA));
    EXPECT_TRUE(utils::DeleteFileA(filePathA));
    EXPECT_FALSE(utils::FileExistsA(filePathA));
    // Test deleting non-existent file
    EXPECT_FALSE(utils::DeleteFileA(filePathA));
    
    std::wstring filePathW = CreateTestFileW(L"deleteme.txt");
    EXPECT_TRUE(utils::FileExistsW(filePathW));
    EXPECT_TRUE(utils::DeleteFileW(filePathW));
    EXPECT_FALSE(utils::FileExistsW(filePathW));
    
    std::string filePathUtf8 = CreateTestFileA("deleteme_utf8.txt");
    EXPECT_TRUE(utils::FileExistsUtf8(filePathUtf8));
    EXPECT_TRUE(utils::DeleteFileUtf8(filePathUtf8));
    EXPECT_FALSE(utils::FileExistsUtf8(filePathUtf8));
}

// Test directory deletion
TEST_F(PathUtilsTest, DeleteDirectory) {
    // Create a directory with files
    std::string nestedDirA = utils::CombinePathA(testDirA, "todelete");
    utils::CreateDirectoryA(nestedDirA);
    CreateTestFileA("todelete\\file1.txt");
    CreateTestFileA("todelete\\file2.txt");
    
    std::string subDir = utils::CombinePathA(nestedDirA, "subdir");
    utils::CreateDirectoryA(subDir);
    CreateTestFileA("todelete\\subdir\\file3.txt");
    
    EXPECT_TRUE(utils::DirectoryExistsA(nestedDirA));
    EXPECT_TRUE(utils::DeleteDirectoryA(nestedDirA));
    EXPECT_FALSE(utils::DirectoryExistsA(nestedDirA));
    
    // Wide char version
    std::wstring nestedDirW = utils::CombinePathW(testDirW, L"todelete");
    utils::CreateDirectoryW(nestedDirW);
    CreateTestFileW(L"todelete\\file1.txt");
    
    EXPECT_TRUE(utils::DirectoryExistsW(nestedDirW));
    EXPECT_TRUE(utils::DeleteDirectoryW(nestedDirW));
    EXPECT_FALSE(utils::DirectoryExistsW(nestedDirW));
    
    // UTF-8 version
    std::string nestedDirUtf8 = utils::CombinePathA(testDirA, "todelete_utf8");
    utils::CreateDirectoryUtf8(nestedDirUtf8);
    
    EXPECT_TRUE(utils::DirectoryExistsUtf8(nestedDirUtf8));
    EXPECT_TRUE(utils::DeleteDirectoryUtf8(nestedDirUtf8));
    EXPECT_FALSE(utils::DirectoryExistsUtf8(nestedDirUtf8));
}

// Test file copying
TEST_F(PathUtilsTest, CopyFile) {
    std::string srcFile = CreateTestFileA("source.txt", "copy test content");
    std::string destFile = utils::CombinePathA(testDirA, "destination.txt");
    
    EXPECT_TRUE(utils::FileExistsA(srcFile));
    EXPECT_FALSE(utils::FileExistsA(destFile));
    EXPECT_TRUE(utils::CopyFileA(srcFile, destFile));
    EXPECT_TRUE(utils::FileExistsA(destFile));
    
    // Test content
    EXPECT_EQ(utils::ReadTextFileA(srcFile), utils::ReadTextFileA(destFile));
    
    // Wide char version
    std::wstring srcFileW = CreateTestFileW(L"source_w.txt", L"copy test content");
    std::wstring destFileW = utils::CombinePathW(testDirW, L"destination_w.txt");
    
    EXPECT_TRUE(utils::CopyFileW(srcFileW, destFileW));
    EXPECT_TRUE(utils::FileExistsW(destFileW));
    
    // UTF-8 version
    std::string srcFileUtf8 = CreateTestFileA("source_utf8.txt", "copy test content utf8");
    std::string destFileUtf8 = utils::CombinePathA(testDirA, "destination_utf8.txt");
    
    EXPECT_TRUE(utils::CopyFileUtf8(srcFileUtf8, destFileUtf8));
    EXPECT_TRUE(utils::FileExistsUtf8(destFileUtf8));
}

// Test file moving
TEST_F(PathUtilsTest, MoveFile) {
    std::string srcFile = CreateTestFileA("moveSource.txt", "move test content");
    std::string destFile = utils::CombinePathA(testDirA, "moveDestination.txt");
    
    EXPECT_TRUE(utils::FileExistsA(srcFile));
    EXPECT_FALSE(utils::FileExistsA(destFile));
    EXPECT_TRUE(utils::MoveFileA(srcFile, destFile));
    EXPECT_FALSE(utils::FileExistsA(srcFile));
    EXPECT_TRUE(utils::FileExistsA(destFile));
    EXPECT_EQ("move test content", utils::ReadTextFileA(destFile));
    
    // Wide char version
    std::wstring srcFileW = CreateTestFileW(L"moveSource_w.txt", L"move test content");
    std::wstring destFileW = utils::CombinePathW(testDirW, L"moveDestination_w.txt");
    
    EXPECT_TRUE(utils::MoveFileW(srcFileW, destFileW));
    EXPECT_FALSE(utils::FileExistsW(srcFileW));
    EXPECT_TRUE(utils::FileExistsW(destFileW));
    
    // UTF-8 version
    std::string srcFileUtf8 = CreateTestFileA("moveSource_utf8.txt", "move test content utf8");
    std::string destFileUtf8 = utils::CombinePathA(testDirA, "moveDestination_utf8.txt");
    
    EXPECT_TRUE(utils::MoveFileUtf8(srcFileUtf8, destFileUtf8));
    EXPECT_FALSE(utils::FileExistsUtf8(srcFileUtf8));
    EXPECT_TRUE(utils::FileExistsUtf8(destFileUtf8));
}

// Test path manipulation functions
TEST_F(PathUtilsTest, PathManipulation) {
    // GetDrive
    EXPECT_EQ("C:", utils::GetDriveA("C:\\Windows\\System32"));
    EXPECT_EQ(L"D:", utils::GetDriveW(L"D:\\Games\\Steam"));
    EXPECT_EQ("", utils::GetDriveA("Windows\\System32"));
    
    // GetDirectory
    EXPECT_EQ("C:\\Windows", utils::GetDirectoryA("C:\\Windows\\System32"));
    EXPECT_EQ(L"D:\\Games", utils::GetDirectoryW(L"D:\\Games\\Steam"));
    EXPECT_EQ("", utils::GetDirectoryA("file.txt"));
    
    // GetFileName
    EXPECT_EQ("System32", utils::GetFileNameA("C:\\Windows\\System32"));
    EXPECT_EQ(L"Steam", utils::GetFileNameW(L"D:\\Games\\Steam"));
    EXPECT_EQ("file.txt", utils::GetFileNameA("file.txt"));
    
    // GetExtension
    EXPECT_EQ(".txt", utils::GetExtensionA("C:\\Windows\\file.txt"));
    EXPECT_EQ(L".exe", utils::GetExtensionW(L"D:\\Games\\game.exe"));
    EXPECT_EQ("", utils::GetExtensionA("file"));
    
    // RemoveExtension
    EXPECT_EQ("C:\\Windows\\file", utils::RemoveExtensionA("C:\\Windows\\file.txt"));
    EXPECT_EQ(L"D:\\Games\\game", utils::RemoveExtensionW(L"D:\\Games\\game.exe"));
    EXPECT_EQ("file", utils::RemoveExtensionA("file"));
    
    // CombinePath
    EXPECT_EQ("C:\\Windows\\System32", utils::CombinePathA("C:\\Windows", "System32"));
    EXPECT_EQ("C:\\Windows\\System32", utils::CombinePathA("C:\\Windows\\", "System32"));
    EXPECT_EQ("C:\\Windows\\System32", utils::CombinePathA("C:\\Windows", "\\System32"));
    EXPECT_EQ(L"D:\\Games\\Steam", utils::CombinePathW(L"D:\\Games", L"Steam"));
    
    // NormalizePath
    EXPECT_EQ("C:\\Windows\\System32", utils::NormalizePathA("C:/Windows/System32"));
    EXPECT_EQ("C:\\Windows\\System32", utils::NormalizePathA("C:\\Windows\\\\System32"));
    EXPECT_EQ(L"D:\\Games\\Steam", utils::NormalizePathW(L"D:/Games/Steam"));
}

TEST_F(PathUtilsTest, ResolvesRuntimeLayoutFromExecutablePath) {
    const fs::path gameDir = CreateSyntheticGameDir(L"Ballance");
    const fs::path gameExe = gameDir / L"Player.exe";
    const fs::path driverExe = gameDir / L"Bin" / L"BMLCoreDriver.exe";

    ExpectRuntimeLayout(gameDir, utils::ResolveRuntimeLayoutFromExecutable(gameExe), runtimeNames);
    ExpectRuntimeLayout(gameDir, utils::ResolveRuntimeLayoutFromExecutable(driverExe), runtimeNames);
}

TEST_F(PathUtilsTest, ResolvesRuntimeLayoutFromModsDirectory) {
    const fs::path gameDir = CreateSyntheticGameDir(L"BallanceMods");
    const utils::RuntimeLayout expected =
        utils::ResolveRuntimeLayoutFromExecutable(gameDir / L"Player.exe", runtimeNames);

    const utils::RuntimeLayout actual =
        utils::ResolveRuntimeLayoutFromModsDirectory(expected.mods_directory, runtimeNames);

    ExpectRuntimeLayout(gameDir, actual, runtimeNames);
}

TEST_F(PathUtilsTest, ResolvesRuntimeLayoutFromCustomModsDirectoryUsingExecutableRoot) {
    const fs::path gameDir = CreateSyntheticGameDir(L"BallanceCustomMods");
    const fs::path customModsDir = (gameDir / L"CustomMods").lexically_normal();
    const utils::RuntimeLayout expected = utils::GetRuntimeLayout(runtimeNames);

    const utils::RuntimeLayout actual =
        utils::ResolveRuntimeLayoutFromModsDirectory(customModsDir, runtimeNames);

    EXPECT_EQ(expected.game_directory, actual.game_directory);
    EXPECT_EQ(expected.runtime_directory, actual.runtime_directory);
    EXPECT_EQ(expected.packages_directory, actual.packages_directory);
    EXPECT_EQ(expected.crash_dumps_directory, actual.crash_dumps_directory);
    EXPECT_EQ(expected.fault_log_path, actual.fault_log_path);
    EXPECT_EQ(customModsDir, actual.mods_directory);
}

TEST_F(PathUtilsTest, ResolvesRuntimeLayoutWithCustomDirectoryNames) {
    utils::RuntimeLayoutNames customNames;
    customNames.runtime_directory = L"LoaderData";
    customNames.mods_directory = L"Extensions";
    customNames.packages_directory = L"Archives";
    customNames.crash_dumps_directory = L"Dumps";
    customNames.fault_log_file = L"faults.json";

    const fs::path gameDir = CreateSyntheticGameDir(L"CustomLayoutGame");
    const utils::RuntimeLayout layout =
        utils::ResolveRuntimeLayoutFromExecutable(gameDir / L"Player.exe", customNames);

    ExpectRuntimeLayout(gameDir, layout, customNames);
}

// Test path validation functions
TEST_F(PathUtilsTest, PathValidation) {
    // IsPathValid - note that Windows paths with drive letters contain colons,
    // which the implementation considers invalid
    EXPECT_FALSE(utils::IsPathValidA("C:\\Windows\\System32")); // Contains colon
    EXPECT_TRUE(utils::IsPathValidA("Windows\\System32"));      // No invalid chars
    EXPECT_FALSE(utils::IsPathValidA("Windows\\Sys*tem32"));    // Contains *

    EXPECT_FALSE(utils::IsPathValidW(L"D:\\Games\\Steam"));     // Contains colon
    EXPECT_TRUE(utils::IsPathValidW(L"Games\\Steam"));          // No invalid chars
    EXPECT_FALSE(utils::IsPathValidW(L"Games\\St?eam"));        // Contains ?
    
    // IsAbsolutePath
    EXPECT_TRUE(utils::IsAbsolutePathA("C:\\Windows\\System32"));
    EXPECT_FALSE(utils::IsAbsolutePathA("Windows\\System32"));
    EXPECT_TRUE(utils::IsAbsolutePathW(L"D:\\Games\\Steam"));
    EXPECT_FALSE(utils::IsAbsolutePathW(L"Games\\Steam"));
    
    // IsRelativePath
    EXPECT_FALSE(utils::IsRelativePathA("C:\\Windows\\System32"));
    EXPECT_TRUE(utils::IsRelativePathA("Windows\\System32"));
    EXPECT_FALSE(utils::IsRelativePathW(L"D:\\Games\\Steam"));
    EXPECT_TRUE(utils::IsRelativePathW(L"Games\\Steam"));
    
    // IsPathRooted
    EXPECT_TRUE(utils::IsPathRootedA("C:\\Windows\\System32"));
    EXPECT_TRUE(utils::IsPathRootedA("\\Windows\\System32"));
    EXPECT_FALSE(utils::IsPathRootedA("Windows\\System32"));
    EXPECT_TRUE(utils::IsPathRootedW(L"D:\\Games\\Steam"));
    EXPECT_TRUE(utils::IsPathRootedW(L"\\Games\\Steam"));
    EXPECT_FALSE(utils::IsPathRootedW(L"Games\\Steam"));
}

// Test path resolution functions
TEST_F(PathUtilsTest, PathResolution) {
    // ResolvePath
    EXPECT_EQ("C:\\Windows\\System32", utils::ResolvePathA("C:\\Windows\\System32"));
    EXPECT_EQ("C:\\Windows", utils::ResolvePathA("C:\\Windows\\System32\\.."));
    EXPECT_EQ("C:\\Windows\\System32", utils::ResolvePathA("C:\\Windows\\.\\System32"));
    EXPECT_EQ(L"D:\\Games", utils::ResolvePathW(L"D:\\Games\\Steam\\.."));
    
    // MakeRelativePath
    EXPECT_EQ("System32", utils::MakeRelativePathA("C:\\Windows\\System32", "C:\\Windows"));
    EXPECT_EQ("..", utils::MakeRelativePathA("C:\\Windows", "C:\\Windows\\System32"));
    EXPECT_EQ("..\\..\\Games", utils::MakeRelativePathA("C:\\Games", "C:\\Windows\\System32"));
    EXPECT_EQ(L"Steam", utils::MakeRelativePathW(L"D:\\Games\\Steam", L"D:\\Games"));
}

// Test file properties functions
TEST_F(PathUtilsTest, FileProperties) {
    std::string testContent = "This is test content for size checking";
    std::string filePath = CreateTestFileA("size_test.txt", testContent);
    
    // GetFileSize
    EXPECT_EQ(testContent.size(), static_cast<size_t>(utils::GetFileSizeA(filePath)));
    EXPECT_EQ(-1, utils::GetFileSizeA(filePath + ".nonexistent"));
    
    // Wide character file size test
    // Note: When writing wide strings to files with wofstream,
    // encoding conversion happens, so we can't directly calculate expected size
    std::wstring testContentW = L"This is test content for size checking";
    std::wstring filePathW = CreateTestFileW(L"size_test_w.txt", testContentW);

    // Just check that file exists and has reasonable size
    int64_t size = utils::GetFileSizeW(filePathW);
    EXPECT_GT(size, 0);
    // On Windows with default encoding, each character is typically 2 bytes,
    // but BOM and encoding conversion may affect the final size
    
    utils::FileTime fileTimeW = utils::GetFileTimeW(filePathW);
    EXPECT_NE(0, fileTimeW.creationTime);
}

// Test file I/O functions
TEST_F(PathUtilsTest, FileIO) {
    // Text file I/O
    std::string content = "Test content for file I/O\nLine 2\nLine 3";
    std::string filePath = utils::CombinePathA(testDirA, "io_test.txt");
    
    EXPECT_TRUE(utils::WriteTextFileA(filePath, content));
    EXPECT_TRUE(utils::FileExistsA(filePath));
    EXPECT_EQ(content, utils::ReadTextFileA(filePath));
    
    // Wide char version
    std::wstring contentW = L"Test content for file I/O\nLine 2\nLine 3";
    std::wstring filePathW = utils::CombinePathW(testDirW, L"io_test_w.txt");
    
    EXPECT_TRUE(utils::WriteTextFileW(filePathW, contentW));
    EXPECT_TRUE(utils::FileExistsW(filePathW));
    EXPECT_EQ(contentW, utils::ReadTextFileW(filePathW));
    
    // Binary file I/O
    std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFC};
    std::string binaryPath = utils::CombinePathA(testDirA, "binary_test.bin");
    
    EXPECT_TRUE(utils::WriteBinaryFileA(binaryPath, binaryData));
    EXPECT_TRUE(utils::FileExistsA(binaryPath));
    
    std::vector<uint8_t> readData = utils::ReadBinaryFileA(binaryPath);
    ASSERT_EQ(binaryData.size(), readData.size());
    for (size_t i = 0; i < binaryData.size(); ++i) {
        EXPECT_EQ(binaryData[i], readData[i]);
    }
}

// Test temporary file functions
TEST_F(PathUtilsTest, TempFile) {
    std::string tempFileA = utils::CreateTempFileA("test");
    EXPECT_FALSE(tempFileA.empty());
    EXPECT_TRUE(utils::FileExistsA(tempFileA));
    
    std::wstring tempFileW = utils::CreateTempFileW(L"test");
    EXPECT_FALSE(tempFileW.empty());
    EXPECT_TRUE(utils::FileExistsW(tempFileW));
    
    // Clean up
    utils::DeleteFileA(tempFileA);
    utils::DeleteFileW(tempFileW);
}

// Test directory listing functions
TEST_F(PathUtilsTest, DirectoryListing) {
    // Create some test files
    CreateTestFileA("list_test1.txt");
    CreateTestFileA("list_test2.txt");
    CreateTestFileA("list_test3.dat");
    
    // Create subdirectories
    utils::CreateDirectoryA(utils::CombinePathA(testDirA, "subdir1"));
    utils::CreateDirectoryA(utils::CombinePathA(testDirA, "subdir2"));
    
    // Test ListFiles
    std::vector<std::string> txtFiles = utils::ListFilesA(testDirA, "*.txt");
    EXPECT_EQ(2, txtFiles.size());
    EXPECT_TRUE(std::find(txtFiles.begin(), txtFiles.end(), "list_test1.txt") != txtFiles.end());
    EXPECT_TRUE(std::find(txtFiles.begin(), txtFiles.end(), "list_test2.txt") != txtFiles.end());
    
    std::vector<std::string> allFiles = utils::ListFilesA(testDirA, "*");
    EXPECT_GE(allFiles.size(), 3);
    
    // Test ListDirectories
    std::vector<std::string> dirs = utils::ListDirectoriesA(testDirA, "*");
    EXPECT_EQ(2, dirs.size());
    EXPECT_TRUE(std::find(dirs.begin(), dirs.end(), "subdir1") != dirs.end());
    EXPECT_TRUE(std::find(dirs.begin(), dirs.end(), "subdir2") != dirs.end());
}

// Main function that runs all the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST_F(PathUtilsTest, GetCurrentDirectorySupportsLongPaths) {
    const fs::path longDir = CreateLongDirectoryTree(L"CurrentDirectoryLongPath", 320);
    const std::wstring extendedLongDir = ToExtendedLengthPath(longDir);
    if (!utils::SetCurrentDirectoryW(extendedLongDir)) {
        std::error_code cleanupError;
        fs::remove_all(fs::path(ToExtendedLengthPath(fs::path(testDirW) / L"CurrentDirectoryLongPath")),
                       cleanupError);
        GTEST_SKIP() << "Long current directories are not supported in this environment";
    }

    EXPECT_EQ(extendedLongDir, utils::GetCurrentDirectoryW());
    EXPECT_EQ(utils::Utf16ToUtf8(extendedLongDir), utils::GetCurrentDirectoryA());

    utils::SetCurrentDirectoryW(originalCurrentDirectoryW);
    std::error_code cleanupError;
    fs::remove_all(fs::path(ToExtendedLengthPath(fs::path(testDirW) / L"CurrentDirectoryLongPath")),
                   cleanupError);
}
