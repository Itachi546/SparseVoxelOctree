#include "voxel-app.h"
#include "utils.h"

#include "voxels/voxel-data.h"

#ifdef VULKAN_ENABLED
#include "rendering/vulkan-rendering-device.h"
#endif

int main() {
#if VULKAN_ENABLED
    RenderingDevice::GetInstance() = new VulkanRenderingDevice();
#endif
    RenderingDevice *device = RenderingDevice::GetInstance();
    device->SetValidationMode(true);
    device->Initialize();

    while (true) {
    }

    device->Shutdown();
}