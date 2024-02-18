#pragma once

#include <string>
namespace GpuTimer {
    void Initialize();

    void Begin(std::string name);

    void End();

    void AddUI();

    void Reset();

    void Shutdown();
}; // namespace GpuTimer