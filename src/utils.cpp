#include "pch.h"
#include "utils.h"

namespace utils {
    std::optional<std::string> ReadFile(const std::string &filename, std::ios::openmode mode) {
        std::ifstream inFile(filename, mode);
        if (!inFile)
            return {};
        return std::string{
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>()};
    }
} // namespace utils
