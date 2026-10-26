#include "window_save_manager.h"

#include <utility>

namespace wininfo {

WindowSaveManager::WindowSaveManager(ClosePrompt closePrompt)
    : closePrompt_(std::move(closePrompt)) {}

bool WindowSaveManager::RegisterWindow(HWND window, SaveHandler saveHandler) {
    if (window == nullptr || !IsWindow(window) || !saveHandler) {
        return false;
    }

    windows_.insert_or_assign(
        window,
        Entry{std::move(saveHandler), WindowSaveState::Saved, ++nextGeneration_});
    return true;
}

bool WindowSaveManager::UnregisterWindow(HWND window) {
    return windows_.erase(window) != 0;
}

bool WindowSaveManager::SetUnsaved(HWND window, bool unsaved) {
    const auto it = windows_.find(window);
    if (it == windows_.end()) {
        return false;
    }

    it->second.state = unsaved ? WindowSaveState::Unsaved : WindowSaveState::Saved;
    return true;
}

std::optional<WindowSaveState> WindowSaveManager::GetState(HWND window) const {
    const auto it = windows_.find(window);
    if (it == windows_.end()) {
        return std::nullopt;
    }
    return it->second.state;
}

std::vector<WindowState> WindowSaveManager::GetAllWindowStates() const {
    std::vector<WindowState> states;
    states.reserve(windows_.size());
    for (const auto& [window, entry] : windows_) {
        states.push_back({window, entry.state});
    }
    return states;
}

std::vector<HWND> WindowSaveManager::GetUnsavedWindows() const {
    std::vector<HWND> unsavedWindows;
    for (const auto& [window, entry] : windows_) {
        if (entry.state == WindowSaveState::Unsaved) {
            unsavedWindows.push_back(window);
        }
    }
    return unsavedWindows;
}

std::vector<HWND> WindowSaveManager::GetUnknownWindows() const {
    std::vector<HWND> unknownWindows;
    for (const auto& [window, entry] : windows_) {
        if (entry.state == WindowSaveState::Unknown) {
            unknownWindows.push_back(window);
        }
    }
    return unknownWindows;
}

bool WindowSaveManager::DiscoverOpenWindows() {
    std::vector<HWND> discoveredWindows;
    const BOOL enumerated = EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto* discovered = reinterpret_cast<std::vector<HWND>*>(parameter);
            if (IsWindowVisible(window)) {
                discovered->push_back(window);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&discoveredWindows));

    if (enumerated == FALSE) {
        return false;
    }

    for (auto it = windows_.begin(); it != windows_.end();) {
        if (!IsWindow(it->first)) {
            it = windows_.erase(it);
        } else {
            ++it;
        }
    }

    for (const HWND window : discoveredWindows) {
        if (windows_.find(window) == windows_.end()) {
            windows_.emplace(
                window,
                Entry{SaveHandler{}, WindowSaveState::Unknown, ++nextGeneration_});
        }
    }
    return true;
}

bool WindowSaveManager::AreAllWindowsSaved() const {
    for (const auto& [window, entry] : windows_) {
        (void)window;
        if (entry.state != WindowSaveState::Saved) {
            return false;
        }
    }
    return true;
}

bool WindowSaveManager::SaveWindow(HWND window) {
    const auto it = windows_.find(window);
    if (it == windows_.end()) {
        return false;
    }

    const auto generation = it->second.generation;
    const auto saveHandler = it->second.saveHandler;
    if (!saveHandler || !saveHandler()) {
        return false;
    }

    const auto current = windows_.find(window);
    if (current != windows_.end() && current->second.generation == generation) {
        current->second.state = WindowSaveState::Saved;
    }
    return true;
}

bool WindowSaveManager::CloseWindow(HWND window, bool force) {
    const auto it = windows_.find(window);
    if (it == windows_.end() || !IsWindow(window)) {
        return false;
    }

    if (!force && it->second.state != WindowSaveState::Saved) {
        if (!closePrompt_) {
            return false;
        }

        switch (closePrompt_(window)) {
        case CloseDecision::Save:
            if (!SaveWindow(window)) {
                return false;
            }
            break;
        case CloseDecision::Discard:
            break;
        case CloseDecision::Cancel:
            return false;
        }
    }

    return PostMessageW(window, WM_CLOSE, 0, 0) != FALSE;
}

} // namespace wininfo
