#pragma once

#include <string>
#include <Windows.h>

namespace UI {
    void ShowDialogBox(HWND hwnd, const std::string &message, const std::string &caption);
}; // namespace UI