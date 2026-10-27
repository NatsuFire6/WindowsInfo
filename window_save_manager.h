#pragma once

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace wininfo {

enum class WindowSaveState {
    Unknown,
    Saved,
    Unsaved
};

enum class CloseDecision {
    Save,
    Discard,
    Cancel
};

struct WindowState {
    HWND window;
    WindowSaveState state;
};

class WindowSaveManager {
public:
    using SaveHandler = std::function<bool()>;
    using ClosePrompt = std::function<CloseDecision(HWND)>;

    explicit WindowSaveManager(ClosePrompt closePrompt = {});

    bool RegisterWindow(HWND window, SaveHandler saveHandler);
    bool UnregisterWindow(HWND window);
    bool SetUnsaved(HWND window, bool unsaved = true);

    std::optional<WindowSaveState> GetState(HWND window) const;
    std::vector<WindowState> GetAllWindowStates() const;
    std::vector<HWND> GetUnsavedWindows() const;
    std::vector<HWND> GetUnknownWindows() const;
    bool DiscoverOpenWindows();
    bool UpdateForegroundWindowState();
    bool AreAllWindowsSaved() const;

    bool SaveWindow(HWND window);
    std::vector<HWND> FindWindowsForFile(const std::filesystem::path& filePath);
    bool SaveFile(const std::filesystem::path& filePath);
    bool CloseWindow(HWND window, bool force = false);

private:
    struct Entry {
        SaveHandler saveHandler;
        WindowSaveState state;
        unsigned long long generation;
        bool stateReported;
    };

    static bool SendSaveShortcut(HWND window);

    std::unordered_map<HWND, Entry> windows_;
    ClosePrompt closePrompt_;
    unsigned long long nextGeneration_ = 0;
    HWND foregroundWindow_ = nullptr;
    std::chrono::steady_clock::time_point foregroundSince_{};
};

} // namespace wininfo
