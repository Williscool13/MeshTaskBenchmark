//
// Created by William on 2025-10-16.
//

#ifndef WILLENGINETESTBED_RENDER_UTILS_H
#define WILLENGINETESTBED_RENDER_UTILS_H

#include <fmt/format.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>

namespace Renderer
{
inline void SetObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.pNext = nullptr;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}

#define VK_CHECK(x)                                                             \
    do {                                                                        \
        VkResult err = x;                                                       \
        if (err) {                                                              \
            fmt::println("Detected Vulkan error: {}\n", string_VkResult(err));  \
            exit(1);                                                            \
        }                                                                       \
} while (0)
} // Renderer

#endif //WILLENGINETESTBED_RENDER_UTILS_H