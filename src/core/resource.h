#pragma once

#include <stdint.h>

struct Resource {
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    uint32_t id;
};