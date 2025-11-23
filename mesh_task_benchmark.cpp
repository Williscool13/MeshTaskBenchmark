//
// Created by William on 2025-11-22.
//

#include "mesh_task_benchmark.h"

#include <array>
#include <fmt/format.h>

#include "render/vk_context.h"
#include "input/input.h"
#include "core/time.h"
#include "render/render_targets.h"
#include "render/render_utils.h"
#include "render/vk_helpers.h"
#include "render/vk_swapchain.h"
#include "render/vk_synchronization.h"

MeshTaskBenchmark::MeshTaskBenchmark() = default;

MeshTaskBenchmark::~MeshTaskBenchmark() = default;

void MeshTaskBenchmark::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        fmt::println("SDL_Init failed: {}", SDL_GetError());
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow(
        "Mesh Task Benchmark",
        DEFAULT_WINDOW_WIDTH,
        DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);

    context = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(context.get(), w, h);
    renderTargets = std::make_unique<Renderer::RenderTargets>(context.get(), w, h);

    constexpr int32_t tripleBuffering = 3;
    frameSynchronization.reserve(tripleBuffering);

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::SceneData);

    for (int32_t i = 0; i < tripleBuffering; ++i) {
        frameSynchronization.emplace_back(context.get());
        frameSynchronization[i].Initialize();

        sceneDataBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo));
    }

    basicMeshShaderPipeline = Renderer::BasicMeshShaderPipeline(context.get());
}
void MeshTaskBenchmark::Run()
{
    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    SDL_Event e;
    bool exit = false;
    while (true) {
        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        if (exit) {
            bShouldExit = true;
            break;
        }



        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();

        const float deltaTime = Time::Get().GetDeltaTime();
        freeCamera.Update(deltaTime);

        const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
        Renderer::FrameSynchronization& currentFrameSync = frameSynchronization[currentFrameInFlight];
        Render(currentFrameInFlight, currentFrameSync);
        fmt::println("Loop {}", frameNumber);
        frameNumber++;
    }
}

void MeshTaskBenchmark::Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync)
{
    VK_CHECK(vkWaitForFences(context->device, 1, &frameSync.renderFence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(context->device, 1, &frameSync.renderFence));

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(context->device, swapchain->handle, UINT64_MAX, frameSync.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        fmt::println("Swapchain out of date or suboptimal (Acquire)");
        exit(1);
        return;
    }

    std::array extents = {swapchain->extent.width, swapchain->extent.height};

    const Input& input = Input::Input::Get();
    const float deltaTime = Time::Get().GetDeltaTime();
    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

    //
    {

        const glm::vec3 cameraPos = freeCamera.GetPosition();
        const glm::vec3 forward = freeCamera.GetForward();
        const glm::vec3 up = freeCamera.GetUp();

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, up);

        glm::mat4 proj = glm::perspective(
            freeCamera.GetFov(),
            static_cast<float>(extents[0]) / static_cast<float>(extents[1]),
            freeCamera.GetFarPlane(),
            freeCamera.GetNearPlane()
        );

        sceneData.view = view;
        sceneData.proj = proj;
        sceneData.viewProj = proj * view;
        sceneData.renderTargetSize.x = extents[0];
        sceneData.renderTargetSize.y = extents[1];
        sceneData.deltaTime = deltaTime;

        Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        auto* currentSceneData = static_cast<Renderer::SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
        *currentSceneData = sceneData;
    }

    VkCommandBuffer cmd = frameSync.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));


    //
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    //
    {
        constexpr VkClearValue colorClear = {.color = {0.0f, 0.1f, 0.2f, 1.0f}};
        const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
        const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({extents[0], extents[1]}, &colorAttachment, &depthAttachment);


        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, basicMeshShaderPipeline.pipeline.handle);

        VkViewport viewport = Renderer::VkHelpers::GenerateViewport(extents[0], extents[1]);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(extents[0], extents[1]);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        Renderer::BasicMeshShaderPushConstants pushData{
            glm::mat4(1.0f),
            currentSceneDataBuffer.address,

        };

        vkCmdPushConstants(cmd, basicMeshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::BasicMeshShaderPushConstants), &pushData);
        vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1);
        vkCmdEndRendering(cmd);
    }

    // Prepare for copy
    {
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        barriers[1] = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Blit
    {
        VkOffset3D renderOffset = {static_cast<int32_t>(extents[0]), static_cast<int32_t>(extents[1]), 1};
        VkOffset3D swapchainOffset = {static_cast<int32_t>(swapchain->extent.width), static_cast<int32_t>(swapchain->extent.height), 1};
        VkImageBlit2 blitRegion{};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.srcOffsets[0] = {0, 0, 0};
        blitRegion.srcOffsets[1] = renderOffset;
        blitRegion.dstOffsets[0] = {0, 0, 0};
        blitRegion.dstOffsets[1] = swapchainOffset;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = renderTargets->drawImage.handle;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = currentSwapchainImage;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    //
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(frameSync.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.swapchainSemaphore, VK_PIPELINE_STAGE_2_BLIT_BIT);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(context->graphicsQueue, 1, &submitInfo, frameSync.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = Renderer::VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &frameSync.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(context->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {

        fmt::println("Swapchain out of date or suboptimal (Present)\n");
        exit(1);
        return;
    }
}

void MeshTaskBenchmark::Cleanup()
{

    vkDeviceWaitIdle(context->device);

    SDL_DestroyWindow(window);
}
