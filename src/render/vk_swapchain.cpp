//
// Created by William on 2025-10-10.
//

#include "vk_swapchain.h"

#include "VkBootstrap.h"

#include "vk_context.h"
#include "render_constants.h"
#include "render_utils.h"

namespace Renderer
{
Swapchain::Swapchain(const VulkanContext* context, uint32_t width, uint32_t height): context(context)
{
    Create(width, height);
    Dump();
}

Swapchain::~Swapchain()
{
    vkDestroySwapchainKHR(context->device, handle, nullptr);
    for (VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }
}

void Swapchain::Create(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

    uint32_t formatCount{0};
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, nullptr));
    VkSurfaceFormatKHR surfaceFormats[32]{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, surfaceFormats));

    VkFormat targetSwapchainFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR targetColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (int32_t i = 0; i < formatCount; ++i) {
        if (ENABLE_HDR) {
            if (surfaceFormats[i].format == SWAPCHAIN_HDR_IMAGE_FORMAT && surfaceFormats[i].colorSpace == SWAPCHAIN_HDR_COLOR_SPACE) {
                targetSwapchainFormat = SWAPCHAIN_HDR_IMAGE_FORMAT;
                targetColorSpace = SWAPCHAIN_HDR_COLOR_SPACE;
                break;
            }
        }

        if (surfaceFormats[i].format == SWAPCHAIN_SDR_IMAGE_FORMAT && surfaceFormats[i].colorSpace == SWAPCHAIN_SDR_COLOR_SPACE) {
            targetSwapchainFormat = SWAPCHAIN_SDR_IMAGE_FORMAT;
            targetColorSpace = SWAPCHAIN_SDR_COLOR_SPACE;
        }
    }

    if (targetSwapchainFormat == VK_FORMAT_UNDEFINED) {
        fmt::println("Failed to get valid swapchains for this surface. Crashing.");
        exit(1);
    }

    auto swapchainResult = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = targetSwapchainFormat, .colorSpace = targetColorSpace})
            //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_desired_min_image_count(vkb::SwapchainBuilder::TRIPLE_BUFFERING)
            //.set_desired_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
            .build();

    if (!swapchainResult) {
        fmt::println("Failed to create swapchain: {}", swapchainResult.error().message());
        exit(1);
    }

    vkb::Swapchain vkbSwapchain = swapchainResult.value();

    auto imagesResult = vkbSwapchain.get_images();
    auto viewsResult = vkbSwapchain.get_image_views();

    if (!imagesResult || !viewsResult) {
        fmt::println("Failed to get swapchain images/views");
        exit(1);
    }

    handle = vkbSwapchain.swapchain;
    imageCount = vkbSwapchain.image_count;
    format = vkbSwapchain.image_format;
    colorSpace = vkbSwapchain.color_space;
    extent = {vkbSwapchain.extent.width, vkbSwapchain.extent.height};
    swapchainImages = imagesResult.value();
    swapchainImageViews = viewsResult.value();
}

void Swapchain::Recreate(uint32_t width, uint32_t height)
{
    vkDestroySwapchainKHR(context->device, handle, nullptr);
    for (const auto swapchainImage : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImage, nullptr);
    }

    Create(width, height);
    Dump();
}

void Swapchain::Dump()
{
    fmt::println("=== Swapchain Info ===");
    fmt::println("Image Count: {}", imageCount);
    fmt::println("Format: {}", string_VkFormat(format));
    fmt::println("Color Space: {}", string_VkColorSpaceKHR(colorSpace));
    fmt::println("Extent: {}x{}", extent.width, extent.height);
    fmt::println("Images: {}", swapchainImages.size());
    fmt::println("Image Views: {}", swapchainImageViews.size());
    fmt::println("======================");
}
} // Renderer
