#pragma once

#include <string>
#include "core/resource.h"

namespace gfx {
    struct ID {
        size_t id = 0;
        inline ID() = default;
        ID(size_t _id) : id(_id) {}
    };
#define DEFINE_ID(m_name)                                                                    \
    struct m_name##ID : public ID {                                                          \
        inline operator bool() const { return id != 0; }                                     \
        inline m_name##ID &operator=(m_name##ID p_other) {                                   \
            id = p_other.id;                                                                 \
            return *this;                                                                    \
        }                                                                                    \
        inline bool operator<(const m_name##ID &p_other) const { return id < p_other.id; }   \
        inline bool operator==(const m_name##ID &p_other) const { return id == p_other.id; } \
        inline bool operator!=(const m_name##ID &p_other) const { return id != p_other.id; } \
        inline m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {}                     \
        inline explicit m_name##ID(uint64_t p_int) : ID(p_int) {}                            \
        inline explicit m_name##ID(void *p_ptr) : ID((size_t)p_ptr) {}                       \
        inline m_name##ID() = default;                                                       \
    };                                                                                       \
    /* Ensure type-punnable to pointer. Makes some things easier.*/                          \
    static_assert(sizeof(m_name##ID) == sizeof(void *));

    DEFINE_ID(Buffer)
    DEFINE_ID(Texture)
} // namespace gfx

class RenderingDevice : public Resource {
  public:
    enum class DeviceType {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    struct Device {
        std::string name;
        uint32_t vendor;
        DeviceType deviceType;
    };

    struct WindowPlatformData {
        void *windowPtr;
    };

    virtual void
    Initialize() = 0;

    virtual void CreateSurface(void *platformData) = 0;

    virtual void SetValidationMode(bool state) = 0;

    virtual void Shutdown() = 0;

    virtual uint32_t GetDeviceCount() = 0;
    virtual Device *GetDevice(int index) = 0;

    static RenderingDevice *&GetInstance() {
#ifdef VULKAN_ENABLED
        static RenderingDevice *device = nullptr;
        return device;
#else
#error No Supported Device Selected
#endif
    }
}; // namespace gfx
