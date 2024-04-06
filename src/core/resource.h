#pragma once

#include <stdint.h>
#include <random>

struct Resource {
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    uint32_t id;

    static uint32_t GetId() {
        return rand();
    }
};