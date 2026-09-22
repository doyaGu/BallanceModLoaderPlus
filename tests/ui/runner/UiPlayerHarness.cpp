#include "UiPlayerHarness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <imm.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace UiTest {
namespace {

class UniqueHandle {
  public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : m_Handle(handle) {}
    ~UniqueHandle() {
        if (m_Handle && m_Handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_Handle);
    }
    UniqueHandle(const UniqueHandle &) = delete;
    UniqueHandle &operator=(const UniqueHandle &) = delete;
    UniqueHandle(UniqueHandle &&other) noexcept
        : m_Handle(std::exchange(other.m_Handle, nullptr)) {}
    UniqueHandle &operator=(UniqueHandle &&other) noexcept {
        if (this != &other) {
            if (m_Handle && m_Handle != INVALID_HANDLE_VALUE)
                CloseHandle(m_Handle);
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }
    HANDLE Get() const { return m_Handle; }
    explicit operator bool() const { return m_Handle && m_Handle != INVALID_HANDLE_VALUE; }

  private:
    HANDLE m_Handle = nullptr;
};

std::wstring NormalizedPath(const fs::path &path) {
    std::wstring value = fs::absolute(path).lexically_normal().wstring();
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::string ReadSharedTextFile(const fs::path &path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file)
        return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.Get(), &size) || size.QuadPart <= 0)
        return {};
    std::string text(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &read, nullptr))
        return {};
    text.resize(read);
    return text;
}

void WriteTextFile(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot write " + path.string());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output)
        throw std::runtime_error("Cannot finish writing " + path.string());
}

void Retry(const std::function<void()> &operation, const std::string &description) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        try {
            operation();
            return;
        } catch (const fs::filesystem_error &) {
            if (attempt == 19)
                throw std::runtime_error(description);
            std::this_thread::sleep_for(500ms);
        }
    }
}

struct FileSnapshot {
    fs::path Path;
    fs::path Backup;
    bool Existed = false;
    std::uint64_t OriginalFingerprint = 0;
};

std::optional<std::uint64_t> FingerprintFile(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        for (std::streamsize index = 0; index < input.gcount(); ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    if (!input.eof())
        return std::nullopt;
    return hash;
}

class InstallTransaction {
  public:
    explicit InstallTransaction(fs::path root)
        : m_Root(fs::absolute(std::move(root)).lexically_normal()),
          m_ModLoader(m_Root / "ModLoader"), m_Mods(m_ModLoader / "Mods"),
          m_ModsBackup(m_ModLoader /
                       ("Mods.bml-ui-backup-" + std::to_string(GetCurrentProcessId()))) {}

    ~InstallTransaction() {
        if (!m_Restored) {
            try {
                Restore();
            } catch (...) {
            }
        }
    }

    void Prepare(const PlayerRunRequest &request) {
        if (!fs::is_regular_file(request.BuildDll))
            throw std::runtime_error("Built BML DLL does not exist: " + request.BuildDll.string());
        if (!fs::is_regular_file(m_Root / "Bin" / "Player.exe"))
            throw std::runtime_error("Ballance Player does not exist under " + m_Root.string());
        fs::create_directories(m_ModLoader);
        if (fs::exists(m_ModsBackup))
            throw std::runtime_error("Stale UI automation backup exists: " + m_ModsBackup.string());

        m_ModsExisted = fs::exists(m_Mods);
        if (m_ModsExisted)
            fs::rename(m_Mods, m_ModsBackup);
        fs::create_directories(m_Mods);

        const fs::path installedLoader = m_Root / "BuildingBlocks" / "BMLPlus.dll";
        Snapshot(installedLoader);
        fs::create_directories(installedLoader.parent_path());
        Retry(
            [&] {
                fs::copy_file(request.BuildDll, installedLoader,
                              fs::copy_options::overwrite_existing);
            },
            "Cannot install BMLPlus.dll");

        const fs::path modLog = m_Root / "ModLoader" / "ModLoader.log";
        const fs::path playerLog = m_Root / "Bin" / "Player.log";
        Snapshot(modLog);
        Snapshot(playerLog);
        Snapshot(m_Root / "Bin" / "Player.ini");
        Snapshot(m_Root / "ModLoader" / "Configs" / "BML.cfg");
        fs::remove(modLog);
        fs::remove(playerLog);

        if (std::find(request.SelectedScenario.Fixtures.begin(),
                      request.SelectedScenario.Fixtures.end(),
                      "custom-map") != request.SelectedScenario.Fixtures.end()) {
            const fs::path map = m_Root / "ModLoader" / "Maps" / "BMLUiAutomation.nmo";
            const fs::path source = m_Root / "3D Entities" / "Level" / "Level_01.NMO";
            if (!fs::is_regular_file(source))
                throw std::runtime_error("Custom-map fixture source does not exist: " +
                                         source.string());
            Snapshot(map);
            fs::create_directories(map.parent_path());
            fs::copy_file(source, map, fs::copy_options::overwrite_existing);
        }

        if (std::find(request.SelectedScenario.Fixtures.begin(),
                      request.SelectedScenario.Fixtures.end(),
                      "public-authoring") != request.SelectedScenario.Fixtures.end()) {
            if (!fs::is_regular_file(request.PublicAuthoringMod)) {
                throw std::runtime_error(
                    "Public authoring fixture does not exist: " +
                    request.PublicAuthoringMod.string());
            }
            fs::copy_file(request.PublicAuthoringMod,
                          m_Mods / "PublicAuthoringTest.bmodp",
                          fs::copy_options::overwrite_existing);
        }
    }

    bool Restore() {
        if (m_Restored)
            return true;
        bool restored = true;
        for (auto found = m_Files.rbegin(); found != m_Files.rend(); ++found) {
            try {
                const FileSnapshot &snapshot = *found;
                if (snapshot.Existed) {
                    Retry(
                        [&] {
                            fs::copy_file(snapshot.Backup, snapshot.Path,
                                          fs::copy_options::overwrite_existing);
                        },
                        "Cannot restore " + snapshot.Path.string());
                    Retry([&] { fs::remove(snapshot.Backup); },
                          "Cannot remove backup " + snapshot.Backup.string());
                } else {
                    Retry([&] { fs::remove(snapshot.Path); },
                          "Cannot remove test file " + snapshot.Path.string());
                }
            } catch (...) {
                restored = false;
            }
        }
        try {
            if (fs::exists(m_Mods)) {
                if (m_Mods.parent_path() != m_ModLoader)
                    throw std::runtime_error("Unsafe Mods cleanup target");
                fs::remove_all(m_Mods);
            }
            if (m_ModsExisted)
                fs::rename(m_ModsBackup, m_Mods);
        } catch (...) {
            restored = false;
        }
        m_Restored = true;
        return restored && VerifyRestored();
    }

  private:
    void Snapshot(const fs::path &path) {
        FileSnapshot snapshot;
        snapshot.Path = path;
        snapshot.Existed = fs::is_regular_file(path);
        if (snapshot.Existed) {
            const auto fingerprint = FingerprintFile(path);
            if (!fingerprint.has_value())
                throw std::runtime_error("Cannot fingerprint " + path.string());
            snapshot.OriginalFingerprint = *fingerprint;
        }
        snapshot.Backup = path;
        snapshot.Backup += ".bml-ui-backup-" + std::to_string(GetCurrentProcessId());
        if (fs::exists(snapshot.Backup))
            throw std::runtime_error("Stale file backup exists: " + snapshot.Backup.string());
        if (snapshot.Existed) {
            Retry(
                [&] {
                    fs::copy_file(snapshot.Path, snapshot.Backup,
                                  fs::copy_options::overwrite_existing);
                },
                "Cannot back up " + path.string());
        }
        m_Files.push_back(std::move(snapshot));
    }

    bool VerifyRestored() const {
        if (m_ModsExisted != fs::is_directory(m_Mods) || fs::exists(m_ModsBackup))
            return false;
        for (const auto &snapshot : m_Files) {
            const auto fingerprint = FingerprintFile(snapshot.Path);
            if (fs::exists(snapshot.Backup) || snapshot.Existed != fingerprint.has_value() ||
                (snapshot.Existed && *fingerprint != snapshot.OriginalFingerprint))
                return false;
        }
        return true;
    }

    fs::path m_Root;
    fs::path m_ModLoader;
    fs::path m_Mods;
    fs::path m_ModsBackup;
    std::vector<FileSnapshot> m_Files;
    bool m_ModsExisted = false;
    bool m_Restored = false;
};

std::vector<DWORD> FindTargetPlayerProcesses(const fs::path &playerPath) {
    std::vector<DWORD> matches;
    UniqueHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot)
        return matches;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.Get(), &entry))
        return matches;
    const std::wstring target = NormalizedPath(playerPath);
    do {
        if (_wcsicmp(entry.szExeFile, L"Player.exe") != 0)
            continue;
        UniqueHandle process(
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (!process)
            continue;
        std::wstring image(32768, L'\0');
        DWORD length = static_cast<DWORD>(image.size());
        if (QueryFullProcessImageNameW(process.Get(), 0, image.data(), &length)) {
            image.resize(length);
            if (NormalizedPath(image) == target)
                matches.push_back(entry.th32ProcessID);
        }
    } while (Process32NextW(snapshot.Get(), &entry));
    return matches;
}

void StopTargetPlayers(const fs::path &playerPath) {
    for (DWORD processId : FindTargetPlayerProcesses(playerPath)) {
        UniqueHandle process(OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, processId));
        if (process) {
            TerminateProcess(process.Get(), 3);
            WaitForSingleObject(process.Get(), 5000);
        }
    }
}

struct WindowSearch {
    DWORD ProcessId = 0;
    std::wstring Title;
    HWND Found = nullptr;
    long long SmallestArea = LLONG_MAX;
};

void ConsiderWindow(HWND window, WindowSearch &search) {
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner != search.ProcessId || !IsWindowVisible(window))
        return;
    wchar_t title[256]{};
    if (GetWindowTextW(window, title, 256) <= 0 || search.Title != title)
        return;
    RECT rect{};
    if (!GetWindowRect(window, &rect))
        return;
    const long long area =
        static_cast<long long>(rect.right - rect.left) * (rect.bottom - rect.top);
    if (area > 0 && area < search.SmallestArea) {
        search.Found = window;
        search.SmallestArea = area;
    }
}

BOOL CALLBACK FindChildWindow(HWND window, LPARAM parameter) {
    ConsiderWindow(window, *reinterpret_cast<WindowSearch *>(parameter));
    return TRUE;
}

BOOL CALLBACK FindTopWindow(HWND window, LPARAM parameter) {
    auto &search = *reinterpret_cast<WindowSearch *>(parameter);
    ConsiderWindow(window, search);
    EnumChildWindows(window, FindChildWindow, parameter);
    return TRUE;
}

HWND FindVisibleWindow(DWORD processId, const wchar_t *title) {
    WindowSearch search{processId, title};
    EnumWindows(FindTopWindow, reinterpret_cast<LPARAM>(&search));
    return search.Found;
}

class TopMostGuard {
  public:
    void Hold(HWND window) {
        if (!window)
            return;
        if (m_Window && m_Window != window)
            Release();
        m_Window = window;
        SetWindowPos(m_Window, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    void Release() {
        if (!m_Window)
            return;
        if (IsWindow(m_Window)) {
            SetWindowPos(m_Window, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        m_Window = nullptr;
    }

    ~TopMostGuard() { Release(); }

  private:
    HWND m_Window = nullptr;
};

bool ActivateWindow(HWND window) {
    if (!window)
        return false;
    const HWND previousForeground = GetForegroundWindow();
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD targetThread = GetWindowThreadProcessId(window, nullptr);
    const DWORD foregroundThread =
        previousForeground ? GetWindowThreadProcessId(previousForeground, nullptr) : 0;
    const bool attachedToForeground = foregroundThread && foregroundThread != currentThread &&
                                      AttachThreadInput(currentThread, foregroundThread, TRUE);
    const bool attachedToTarget = targetThread && targetThread != currentThread &&
                                  targetThread != foregroundThread &&
                                  AttachThreadInput(currentThread, targetThread, TRUE);

    // Alt gives this process the same foreground transition right as a real
    // keyboard user. Ballance suspends native menu processing while unfocused.
    keybd_event(VK_MENU, 0, 0, 0);
    ShowWindowAsync(window, SW_RESTORE);
    BringWindowToTop(window);
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    if (attachedToTarget)
        AttachThreadInput(currentThread, targetThread, FALSE);
    if (attachedToForeground)
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    if (HIMC inputContext = ImmGetContext(window)) {
        ImmSetOpenStatus(inputContext, FALSE);
        ImmReleaseContext(window, inputContext);
    }
    return GetForegroundWindow() == window;
}

bool ClientIsUnobstructed(HWND window) {
    if (!window || !IsWindowVisible(window) || IsIconic(window))
        return false;

    RECT client{};
    POINT origin{};
    if (!GetClientRect(window, &client) || !ClientToScreen(window, &origin))
        return false;
    OffsetRect(&client, origin.x, origin.y);

    DWORD targetProcess = 0;
    GetWindowThreadProcessId(window, &targetProcess);
    for (HWND candidate = GetTopWindow(nullptr); candidate;
         candidate = GetWindow(candidate, GW_HWNDNEXT)) {
        if (candidate == window)
            return true;
        if (!IsWindowVisible(candidate) || IsIconic(candidate))
            continue;
        DWORD candidateProcess = 0;
        GetWindowThreadProcessId(candidate, &candidateProcess);
        if (candidateProcess == targetProcess)
            continue;
        RECT candidateRect{};
        RECT overlap{};
        if (GetWindowRect(candidate, &candidateRect) &&
            IntersectRect(&overlap, &client, &candidateRect))
            return false;
    }
    return true;
}

bool IsForegroundClientVisible(HWND window) {
    return GetForegroundWindow() == window && ClientIsUnobstructed(window);
}

bool EnsureForegroundClientVisible(HWND window, std::chrono::milliseconds timeout = 1500ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        ActivateWindow(window);
        std::this_thread::sleep_for(50ms);
        if (IsForegroundClientVisible(window))
            return true;
        std::this_thread::sleep_for(50ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

bool ConfirmSetupDialog(HWND dialog) {
    constexpr UINT wmCommand = 0x0111;
    constexpr UINT lbGetCurSel = 0x0188;
    constexpr UINT lbSetCurSel = 0x0186;
    constexpr int driverId = 1007;
    constexpr int screenModeId = 1008;
    HWND driver = GetDlgItem(dialog, driverId);
    HWND screenMode = GetDlgItem(dialog, screenModeId);
    if (!driver || !screenMode)
        return false;
    if (SendMessageW(driver, lbGetCurSel, 0, 0) < 0) {
        SendMessageW(driver, lbSetCurSel, 0, 0);
        SendMessageW(dialog, wmCommand, MAKEWPARAM(driverId, 1), reinterpret_cast<LPARAM>(driver));
    }
    if (SendMessageW(screenMode, lbGetCurSel, 0, 0) < 0)
        SendMessageW(screenMode, lbSetCurSel, 0, 0);
    if (SendMessageW(screenMode, lbGetCurSel, 0, 0) < 0)
        return false;
    SendMessageW(dialog, wmCommand, IDOK, 0);
    return true;
}

void SendKey(BYTE key) {
    DWORD flags = key >= VK_PRIOR && key <= VK_DOWN ? KEYEVENTF_EXTENDEDKEY : 0;
    const BYTE scan = static_cast<BYTE>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));
    keybd_event(key, scan, flags, 0);
    std::this_thread::sleep_for(35ms);
    keybd_event(key, scan, flags | KEYEVENTF_KEYUP, 0);
    std::this_thread::sleep_for(180ms);
}

bool ParkCursorForKeyboardInput(HWND window) {
    POINT position{8, 8};
    if (!window || !ClientToScreen(window, &position))
        return false;
    if (!SetCursorPos(position.x, position.y))
        return false;
    std::this_thread::sleep_for(50ms);
    return true;
}

enum class InputSequenceKind {
    Fixed,
    Menu,
};

struct InputSequence {
    std::string CheckpointName;
    InputSequenceKind Kind = InputSequenceKind::Fixed;
    std::vector<BYTE> Keys;
    bool Injected = false;
};

std::vector<InputSequence> MakeInputSequences(InputProfile profile) {
    std::vector<InputSequence> sequences = {
        {"input-main-to-options", InputSequenceKind::Menu, {}},
        {"input-options-to-imgui", InputSequenceKind::Menu, {}},
    };
    if (profile != InputProfile::ModList) {
        sequences.push_back({"input-options-to-main", InputSequenceKind::Menu, {}});
        sequences.push_back({"input-main-to-start", InputSequenceKind::Menu, {}});
        if (profile == InputProfile::LevelOne)
            sequences.push_back({"input-start-to-level-1", InputSequenceKind::Fixed, {VK_RETURN}});
        sequences.push_back({"input-dismiss-tutorial", InputSequenceKind::Fixed, {'Q'}});
        if (profile == InputProfile::CustomMap) {
            sequences.push_back({"input-level-to-pause", InputSequenceKind::Fixed, {VK_ESCAPE}});
            sequences.push_back({"input-pause-to-main", InputSequenceKind::Menu, {}});
            sequences.push_back({"input-confirm-exit-level", InputSequenceKind::Fixed, {VK_LEFT, VK_RETURN}});
            sequences.push_back({"input-return-to-start", InputSequenceKind::Menu, {}});
        }
    }
    return sequences;
}

bool IsDecimal(std::string_view value) {
    if (value.empty())
        return false;
    for (char character : value) {
        if (character < '0' || character > '9')
            return false;
    }
    return true;
}

bool IsMenuMove(std::string_view suffix, std::string_view direction) {
    if (suffix.substr(0, direction.size()) != direction)
        return false;
    suffix.remove_prefix(direction.size());
    constexpr std::string_view separatorText = "-to-";
    const std::size_t separator = suffix.find(separatorText);
    return separator != std::string_view::npos &&
        IsDecimal(suffix.substr(0, separator)) &&
        IsDecimal(suffix.substr(separator + separatorText.size()));
}

bool ResolveInput(const InputSequence &sequence, const std::string &checkpoint,
                  std::vector<BYTE> &keys, bool &completesSequence) {
    if (sequence.Kind == InputSequenceKind::Fixed) {
        if (checkpoint != sequence.CheckpointName)
            return false;
        keys = sequence.Keys;
        completesSequence = true;
        return true;
    }

    const std::string prefix = sequence.CheckpointName + "-";
    if (checkpoint.compare(0, prefix.size(), prefix) != 0)
        return false;
    const std::string_view suffix(checkpoint.data() + prefix.size(),
                                  checkpoint.size() - prefix.size());
    if (IsMenuMove(suffix, "down-from-")) {
        keys = {VK_DOWN};
        completesSequence = false;
        return true;
    }
    if (IsMenuMove(suffix, "up-from-")) {
        keys = {VK_UP};
        completesSequence = false;
        return true;
    }
    constexpr std::string_view activate = "activate-";
    if (suffix.substr(0, activate.size()) == activate &&
        IsDecimal(suffix.substr(activate.size()))) {
        keys = {VK_RETURN};
        completesSequence = true;
        return true;
    }
    return false;
}

bool ReadBitmapPixels(HDC memory, HBITMAP bitmap, int height,
                      std::vector<unsigned char> &pixels,
                      BITMAPINFO &info) {
    return GetDIBits(memory, bitmap, 0, height, pixels.data(), &info,
                     DIB_RGB_COLORS) == height;
}

bool SaveClientBitmap(HWND window, const fs::path &path, int &width, int &height) {
    // Captures only need an unobstructed client. The runner itself is often the
    // foreground process (ctest / Cursor), so requiring GetForegroundWindow()
    // == Ballance deadlocks the first checkpoint until the 180s test timeout.
    if (!ClientIsUnobstructed(window))
        return false;
    const DPI_AWARENESS_CONTEXT previous =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RECT client{};
    POINT origin{};
    if (!GetClientRect(window, &client) || !ClientToScreen(window, &origin)) {
        if (previous)
            SetThreadDpiAwarenessContext(previous);
        return false;
    }
    width = client.right;
    height = client.bottom;
    HDC screen = GetDC(nullptr);
    HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    HBITMAP bitmap = memory
        ? CreateCompatibleBitmap(screen, width, height)
        : nullptr;
    HGDIOBJ old = bitmap ? SelectObject(memory, bitmap) : nullptr;
    const bool blit = bitmap &&
        BitBlt(memory, 0, 0, width, height, screen, origin.x, origin.y,
               SRCCOPY | CAPTUREBLT);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    const DWORD rowBytes = (width * 3u + 3u) & ~3u;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(rowBytes) * height);
    bool read = blit && ReadBitmapPixels(
        memory, bitmap, height, pixels, info);
    if (!read && bitmap) {
        const bool printed =
            PrintWindow(window, memory, PW_RENDERFULLCONTENT) ||
            PrintWindow(window, memory, 0);
        read = printed && ReadBitmapPixels(
            memory, bitmap, height, pixels, info);
    }
    if (old)
        SelectObject(memory, old);
    if (bitmap)
        DeleteObject(bitmap);
    if (memory)
        DeleteDC(memory);
    if (screen)
        ReleaseDC(nullptr, screen);
    if (previous)
        SetThreadDpiAwarenessContext(previous);
    if (!read || !ClientIsUnobstructed(window))
        return false;

    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4d42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(pixels.size());
    output.write(reinterpret_cast<const char *>(&fileHeader), sizeof(fileHeader));
    output.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char *>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    return output.good();
}

class EnvironmentVariable {
  public:
    EnvironmentVariable(const wchar_t *name, const std::wstring &value) : m_Name(name) {
        DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
        if (size) {
            m_Previous.resize(size);
            GetEnvironmentVariableW(name, m_Previous.data(), size);
            m_Previous.resize(wcslen(m_Previous.c_str()));
            m_HadPrevious = true;
        }
        if (!SetEnvironmentVariableW(name, value.c_str()))
            throw std::runtime_error("Cannot set child environment variable");
    }
    ~EnvironmentVariable() {
        SetEnvironmentVariableW(m_Name.c_str(), m_HadPrevious ? m_Previous.c_str() : nullptr);
    }

  private:
    std::wstring m_Name;
    std::wstring m_Previous;
    bool m_HadPrevious = false;
};

} // namespace

PlayerRunResult RunPlayerScenario(const PlayerRunRequest &request) {
    PlayerRunResult result;
    const fs::path root = fs::absolute(request.BallanceRoot).lexically_normal();
    const fs::path artifacts = fs::absolute(request.ArtifactsDirectory).lexically_normal();
    const fs::path player = root / "Bin" / "Player.exe";
    const fs::path modLog = root / "ModLoader" / "ModLoader.log";
    const fs::path playerLog = root / "Bin" / "Player.log";
    fs::create_directories(artifacts);
    result.ResultPath = artifacts / (request.SelectedScenario.Name + ".result");
    result.TracePath = artifacts / "ModLoader-trace.log";
    result.PlayerTracePath = artifacts / "Player-trace.log";
    const auto sessionStamp = std::chrono::steady_clock::now().time_since_epoch().count();
    result.SessionDirectory = artifacts / ("session-" + std::to_string(GetCurrentProcessId()) +
                                           '-' + std::to_string(sessionStamp));
    fs::create_directories(result.SessionDirectory);
    fs::remove(result.ResultPath);

    if (!FindTargetPlayerProcesses(player).empty())
        throw std::runtime_error("The target Ballance Player is already running; close it first");

    InstallTransaction installation(root);
    PROCESS_INFORMATION processInfo{};
    bool processStarted = false;
    try {
        installation.Prepare(request);
        EnvironmentVariable resultEnvironment(L"BML_UI_AUTOMATION_RESULT",
                                              result.ResultPath.wstring());
        EnvironmentVariable scenarioEnvironment(L"BML_UI_AUTOMATION_SCENARIO",
                                                std::wstring(request.SelectedScenario.Name.begin(),
                                                             request.SelectedScenario.Name.end()));
        EnvironmentVariable sessionEnvironment(L"BML_UI_AUTOMATION_SESSION",
                                               result.SessionDirectory.wstring());
        UiAutomationSession::RunnerEndpoint session(result.SessionDirectory);

        std::wstring command = L"\"" + player.wstring() + L"\" --width " +
                               std::to_wstring(request.Width) + L" --height " +
                               std::to_wstring(request.Height) + L" --bpp 32";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        if (!CreateProcessW(player.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, player.parent_path().c_str(), &startup, &processInfo)) {
            throw std::runtime_error("Cannot launch Ballance Player");
        }
        processStarted = true;
        CloseHandle(processInfo.hThread);
        AllowSetForegroundWindow(ASFW_ANY);
        AllowSetForegroundWindow(processInfo.dwProcessId);

        TopMostGuard topMost;
        HWND ballanceWindow = nullptr;
        const auto startupDeadline = std::chrono::steady_clock::now() + 15s;
        while (std::chrono::steady_clock::now() < startupDeadline &&
               WaitForSingleObject(processInfo.hProcess, 0) == WAIT_TIMEOUT) {
            if (HWND setup = FindVisibleWindow(processInfo.dwProcessId, L"FullScreen Setup")) {
                if (!ConfirmSetupDialog(setup))
                    throw std::runtime_error("FullScreen Setup has no selectable render mode");
            }
            ballanceWindow = FindVisibleWindow(processInfo.dwProcessId, L"Ballance");
            if (ballanceWindow) {
                topMost.Hold(ballanceWindow);
                result.WindowActivated = EnsureForegroundClientVisible(ballanceWindow);
                break;
            }
            std::this_thread::sleep_for(100ms);
        }

        auto inputs = MakeInputSequences(request.SelectedScenario.Input);
        result.ExpectedInputSequences = static_cast<int>(inputs.size());
        struct CapturePlan {
            std::string CheckpointName;
            std::string ArtifactName;
        };
        const std::vector<CapturePlan> capturePlans = {
            {"capture-game-menu", "GameMenu"},
            {"capture-native-options", "NativeOptions"},
            {"capture-" + request.SelectedScenario.Surface, request.SelectedScenario.CaptureName},
        };
        for (const auto &capturePlan : capturePlans) {
            result.Captures[capturePlan.ArtifactName].Path =
                artifacts / (capturePlan.ArtifactName + ".bmp");
        }

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(request.TimeoutSeconds);
        constexpr auto checkpointAckTimeout = 20s;
        std::uint32_t checkpointAttemptSequence = 0;
        auto checkpointAttemptStarted = std::chrono::steady_clock::now();
        bool checkpointFailureLogged = false;
        auto nextForeground = std::chrono::steady_clock::now();
        while (WaitForSingleObject(processInfo.hProcess, 0) == WAIT_TIMEOUT &&
               std::chrono::steady_clock::now() < deadline) {
            if (std::chrono::steady_clock::now() >= nextForeground) {
                ballanceWindow = FindVisibleWindow(processInfo.dwProcessId, L"Ballance");
                if (ballanceWindow) {
                    topMost.Hold(ballanceWindow);
                    result.WindowActivated =
                        EnsureForegroundClientVisible(ballanceWindow, 500ms) ||
                        result.WindowActivated || ClientIsUnobstructed(ballanceWindow);
                }
                nextForeground = std::chrono::steady_clock::now() + 1s;
            }
            const std::optional<UiAutomationSession::Checkpoint> checkpoint = session.ReceiveNext();
            if (!session.LastError().empty())
                throw std::runtime_error("UI automation session: " + session.LastError());
            if (checkpoint) {
                if (checkpoint->Sequence != checkpointAttemptSequence) {
                    checkpointAttemptSequence = checkpoint->Sequence;
                    checkpointAttemptStarted = std::chrono::steady_clock::now();
                    checkpointFailureLogged = false;
                }
                bool recognized = false;
                bool completed = false;
                bool succeeded = false;
                std::string failureReason = "unsupported-checkpoint";

                ballanceWindow = FindVisibleWindow(processInfo.dwProcessId, L"Ballance");
                if (ballanceWindow)
                    topMost.Hold(ballanceWindow);

                if (checkpoint->Kind == UiAutomationSession::CheckpointKind::Input) {
                    std::vector<BYTE> keys;
                    bool completesSequence = false;
                    InputSequence *input = nullptr;
                    for (InputSequence &candidate : inputs) {
                        if (ResolveInput(candidate, checkpoint->Name, keys,
                                         completesSequence)) {
                            input = &candidate;
                            break;
                        }
                    }
                    if (input) {
                        recognized = true;
                        if (input->Injected) {
                            completed = true;
                            failureReason = "duplicate-checkpoint";
                        } else if (!ballanceWindow) {
                            failureReason = "window-not-found";
                        } else if (!EnsureForegroundClientVisible(ballanceWindow)) {
                            failureReason = "window-obstructed";
                        } else if (!ParkCursorForKeyboardInput(ballanceWindow)) {
                            failureReason = "cursor-position-failed";
                        } else {
                            succeeded = true;
                            for (BYTE key : keys) {
                                if (!EnsureForegroundClientVisible(ballanceWindow)) {
                                    succeeded = false;
                                    failureReason = "window-obstructed";
                                    break;
                                }
                                SendKey(key);
                            }
                            if (succeeded) {
                                if (completesSequence) {
                                    input->Injected = true;
                                    ++result.InjectedInputSequences;
                                }
                                completed = true;
                            }
                        }
                    }
                } else {
                    const auto capturePlan =
                        std::find_if(capturePlans.begin(), capturePlans.end(),
                                     [&](const CapturePlan &candidate) {
                                         return candidate.CheckpointName == checkpoint->Name;
                                     });
                    if (capturePlan != capturePlans.end()) {
                        recognized = true;
                        auto &capture = result.Captures[capturePlan->ArtifactName];
                        if (capture.Captured) {
                            completed = true;
                            failureReason = "duplicate-checkpoint";
                        } else if (!ballanceWindow) {
                            failureReason = "window-not-found";
                        } else {
                            ActivateWindow(ballanceWindow);
                            if (!ClientIsUnobstructed(ballanceWindow) &&
                                !EnsureForegroundClientVisible(ballanceWindow)) {
                                failureReason = "window-obstructed";
                            } else if (!SaveClientBitmap(ballanceWindow, capture.Path,
                                                         capture.Width, capture.Height)) {
                                failureReason = "capture-failed";
                            } else {
                                capture.Captured = true;
                                succeeded = true;
                                completed = true;
                            }
                        }
                    }
                }

                if (!recognized)
                    completed = true;
                if (!completed && std::chrono::steady_clock::now() - checkpointAttemptStarted >=
                                      checkpointAckTimeout) {
                    completed = true;
                }
                if (!completed && !checkpointFailureLogged) {
                    std::cerr << "UI automation: waiting for " << checkpoint->Name
                              << " reason=" << failureReason << '\n';
                    checkpointFailureLogged = true;
                }
                if (completed) {
                    if (!succeeded) {
                        std::cerr << "UI automation: " << checkpoint->Name
                                  << " failed reason=" << failureReason << '\n';
                    }
                    if (!session.Acknowledge(*checkpoint, succeeded,
                                             succeeded ? std::string_view{}
                                                       : std::string_view{failureReason})) {
                        throw std::runtime_error("UI automation session acknowledgement: " +
                                                 session.LastError());
                    }
                    result.HandledCheckpoints.push_back(*checkpoint);
                    if (!succeeded)
                        result.SessionFailures.push_back(checkpoint->Name + ':' + failureReason);
                }
            }
            std::this_thread::sleep_for(100ms);
        }

        if (WaitForSingleObject(processInfo.hProcess, 0) == WAIT_TIMEOUT) {
            result.TimedOut = true;
            TerminateProcess(processInfo.hProcess, 2);
            WaitForSingleObject(processInfo.hProcess, 5000);
        }
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode))
            result.ExitCode = static_cast<int>(exitCode);
        result.ModLoaderLog = ReadSharedTextFile(modLog);
        result.PlayerLog = ReadSharedTextFile(playerLog);
        WriteTextFile(result.TracePath, result.ModLoaderLog);
        WriteTextFile(result.PlayerTracePath, result.PlayerLog);
        StopTargetPlayers(player);
        CloseHandle(processInfo.hProcess);
        processInfo.hProcess = nullptr;
        result.InstallRestored = installation.Restore();
        return result;
    } catch (...) {
        if (processStarted) {
            StopTargetPlayers(player);
            if (processInfo.hProcess)
                CloseHandle(processInfo.hProcess);
        }
        installation.Restore();
        throw;
    }
}

} // namespace UiTest
