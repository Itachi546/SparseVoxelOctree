#pragma once

#include <optional>
#include <string>

namespace utils {

    std::optional<std::string> ReadFile(const char *filename);

    template <typename T>
    inline bool HasFlag(T val, T flag) {
        return (val & flag) == flag;
    }
} // namespace utils
