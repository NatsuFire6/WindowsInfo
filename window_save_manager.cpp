#include "window_save_manager.h"

#include <algorithm>
#include <cwctype>
#include <string>
#include <utility>

namespace wininfo {

namespace {

bool ContainsFileName(const std::wstring& title, const std::wstring& fileName) {
    std::wstring lowerTitle = title;
    std::wstring lowerFileName = fileName;
    const auto toLower = [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    };
    std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), toLower);
    std::transform(
        lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), toLower);

    std::size_t position = lowerTitle.find(lowerFileName);
    while (position != std::wstring::npos) {
        const auto isFileNameCharacter = [](wchar_t character) {
            return std::iswalnum(character) != 0 || character == L'_' ||
                character == L'-' || character == L'.';
        };
        const bool startsAtBoundary = position == 0 ||
            !isFileNameCharacter(lowerTitle[position - 1]);
        const auto end = position + lowerFileName.size();
        const bool endsAtBoundary = end == lowerTitle.size() ||
            !isFileNameCharacter(lowerTitle[end]);
        if (startsAtBoundary && endsAtBoundary) {
            return true;
        }
        position = lowerTitle.find(lowerFileName, position + 1);
    }
    return false;
}

std::wstring GetWindowTitle(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }

    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, title.data(), length + 1);
    if (copied <= 0) {
        return {};
    }
    title.resize(static_cast<std::size_t>(copied));
    return title;
}

} // namespace

WindowSaveManager::WindowSaveManager(ClosePrompt closePrompt)
    : closePrompt_(std::move(closePrompt)) {}

bool WindowSaveManager::RegisterWindow(HWND window, SaveHandler saveHandler) {
    if (window == nullptr || !IsWindow(window) || !saveHandler) {
        return false;
    }

    windows_.insert_or_assign(
        window,
        Entry{std::move(saveHandler), WindowSaveState::Saved, ++nextGeneration_, true});
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
    it->second.stateReported = true;
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
                Entry{SaveHandler{}, WindowSaveState::Unknown, ++nextGeneration_, false});
        }
    }
    return true;
}

bool WindowSaveManager::UpdateForegroundWindowState() {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || !IsWindow(foreground)) {
        foregroundWindow_ = nullptr;
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (foreground != foregroundWindow_) {
        foregroundWindow_ = foreground;
        foregroundSince_ = now;
    }

    auto it = windows_.find(foreground);
    if (it == windows_.end()) {
        it = windows_.emplace(
            foreground,
            Entry{SaveHandler{}, WindowSaveState::Unknown, ++nextGeneration_, false})
                 .first;
    }

    if (now - foregroundSince_ >= std::chrono::minutes(1)) {
        if (it->second.state != WindowSaveState::Saved || !it->second.stateReported) {
            it->second.state = WindowSaveState::Unknown;
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
    if (saveHandler) {
        if (!saveHandler()) {
            return false;
        }
    } else {
        if (it->second.state != WindowSaveState::Unknown || !SendSaveShortcut(window)) {
            return false;
        }
        return true;
    }

    const auto current = windows_.find(window);
    if (current != windows_.end() && current->second.generation == generation) {
        current->second.state = WindowSaveState::Saved;
    }
    return true;
}

std::vector<HWND> WindowSaveManager::FindWindowsForFile(
    const std::filesystem::path& filePath) {
    std::vector<HWND> matchingWindows;
    std::error_code error;
    if (filePath.empty() || filePath.filename().empty() ||
        filePath.extension().empty() || !std::filesystem::is_regular_file(filePath, error) ||
        error || !DiscoverOpenWindows()) {
        return matchingWindows;
    }

    const std::wstring fileName = filePath.filename().wstring();
    for (const auto& [window, entry] : windows_) {
        (void)entry;
        if (ContainsFileName(GetWindowTitle(window), fileName)) {
            matchingWindows.push_back(window);
        }
    }
    return matchingWindows;
}

bool WindowSaveManager::SaveFile(const std::filesystem::path& filePath) {
    const auto matchingWindows = FindWindowsForFile(filePath);
    if (matchingWindows.empty()) {
        return false;
    }

    bool savedAtLeastOne = false;
    bool allSaved = true;
    for (const HWND window : matchingWindows) {
        if (SaveWindow(window)) {
            savedAtLeastOne = true;
        } else {
            allSaved = false;
        }
    }
    return savedAtLeastOne && allSaved;
}

bool WindowSaveManager::SendSaveShortcut(HWND window) {
    if (!IsWindow(window)) {
        return false;
    }

    SetForegroundWindow(window);
    if (GetForegroundWindow() != window) {
        return false;
    }

    INPUT inputs[4]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'S';
    inputs[2] = inputs[1];
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3] = inputs[0];
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    const UINT sent = SendInput(4, inputs, sizeof(INPUT));
    if (sent == 4) {
        return true;
    }

    INPUT releases[2]{};
    releases[0].type = INPUT_KEYBOARD;
    releases[0].ki.wVk = 'S';
    releases[0].ki.dwFlags = KEYEVENTF_KEYUP;
    releases[1].type = INPUT_KEYBOARD;
    releases[1].ki.wVk = VK_CONTROL;
    releases[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, releases, sizeof(INPUT));
    return false;
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
            {
                const auto current = windows_.find(window);
                if (current == windows_.end() ||
                    current->second.state != WindowSaveState::Saved) {
                    return false;
                }
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
