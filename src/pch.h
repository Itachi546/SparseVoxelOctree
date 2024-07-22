#pragma once

// C++ HEADERS
#include <iostream>
#include <string>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <vector>
#include <map>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <list>
#include <fstream>
#include <queue>
#include <stack>

#include <chrono>
#include <sstream>
#include <numeric>
#include <random>
#include <mutex>
#include <thread>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#define ASSERT(condition, message) (assert(condition && message))
#define LOGE(err)                                   \
    {                                               \
        std::cerr << "ERROR::" << err << std::endl; \
        assert(0);                                  \
    }

#define LOG(msg)                                  \
    {                                             \
        std::cout << "LOG::" << msg << std::endl; \
    }

#define LOGW(msg)                                     \
    {                                                 \
        std::cout << "WARNING::" << msg << std::endl; \
    }