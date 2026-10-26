#pragma once

#include <windows.h>

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
    bool AreAllWindowsSaved() const;

    bool SaveWindow(HWND window);
    bool CloseWindow(HWND window, bool force = false);

private:
    struct Entry {
        SaveHandler saveHandler;
        WindowSaveState state;
        unsigned long long generation;
    };

    std::unordered_map<HWND, Entry> windows_;
    ClosePrompt closePrompt_;
    unsigned long long nextGeneration_ = 0;
};

} // namespace wininfo
