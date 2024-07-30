#include "pch.h"

#include "win32-ui.h"

void UI::ShowDialogBox(HWND hwnd, const std::string &message, const std::string &caption) {
    MessageBoxA(hwnd, message.c_str(), caption.c_str(), MB_OK);
}