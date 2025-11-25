//
// Created by William on 2025-11-22.
//

#include "mesh_task_benchmark.h"

#include <array>
#include <fmt/format.h>

#include "meshoptimizer.h"
#include "render/vk_context.h"
#include "input/input.h"
#include "core/time.h"

#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"

#include "render/render_targets.h"
#include "render/render_utils.h"
#include "render/vk_helpers.h"
#include "render/vk_swapchain.h"
#include "render/vk_synchronization.h"
#include "render/model/model_types.h"

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
    for (int32_t i = 0; i < tripleBuffering; ++i) {
        frameSynchronization.emplace_back(context.get());
        frameSynchronization[i].Initialize();
    }

    traditionalPipeline = Renderer::TraditionalPipeline(context.get());
    indirectTraditionalCompute = Renderer::TraditionalIndirectComputePipeline(context.get());
    indirectTraditionalGraphics = Renderer::TraditionalIndirectRenderPipeline(context.get());
    taskMeshPipeline = Renderer::TaskMeshPipeline(context.get());
    indirectTaskMeshCompute = Renderer::IndirectTaskMeshComputePipeline(context.get());
    indirectTaskMeshGraphics = Renderer::IndirectTaskMeshRenderPipeline(context.get());

    CreateBuffers();

    materialBufferAllocator.allocate(sizeof(Renderer::MaterialProperties));
    Renderer::MaterialProperties defaultMaterial{};
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData), &defaultMaterial, sizeof(Renderer::MaterialProperties));

    std::filesystem::path bunnyPath = "assets/stanford_bunny/stanford_bunny.gltf";
    bunnyModel = LoadModel(bunnyPath);

    InstanceGeneration();
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

        if (input.IsKeyPressed(Key::NUM_1)) {
            benchmarkType = BenchmarkType::Traditional;
        }
        if (input.IsKeyPressed(Key::NUM_2)) {
            benchmarkType = BenchmarkType::IndirectTraditional;
        }
        if (input.IsKeyPressed(Key::NUM_3)) {
            benchmarkType = BenchmarkType::Meshlet;
        }
        if (input.IsKeyPressed(Key::NUM_4)) {
            benchmarkType = BenchmarkType::IndirectMeshlet;
        }

        if (benchmarkType != lastBenchmarkType) {
            benchmarkTimer = 0.0f;
            benchmarkFrameCount = 0;
            averageFPS = 0.0f;
            fpsDisplayTimer = 0.0f;
            lastBenchmarkType = benchmarkType;
            const char* benchmarkNames[] = {
                "Traditional Pipeline",
                "Indirect Traditional Pipeline",
                "Meshlet Pipeline",
                "Indirect Meshlet Pipeline"
            };
            fmt::println("\n========================================");
            fmt::println("  {}", benchmarkNames[static_cast<int>(benchmarkType)]);
            fmt::println("========================================\n");
        }

        benchmarkFrameCount++;
        benchmarkTimer += deltaTime;
        averageFPS = static_cast<float>(benchmarkFrameCount) / benchmarkTimer;
        fpsDisplayTimer += deltaTime;
        if (fpsDisplayTimer >= 1.0f) {
            fmt::println("[{}] Average FPS: {:.1f} ({} frames over {:.2f}s)", static_cast<int>(benchmarkType), averageFPS, benchmarkFrameCount, benchmarkTimer);
            fpsDisplayTimer = 0.0f;
        }


        const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
        Renderer::FrameSynchronization& currentFrameSync = frameSynchronization[currentFrameInFlight];
        Render(currentFrameInFlight, currentFrameSync);
        frameNumber++;
    }
}

void MeshTaskBenchmark::Traditional(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd)
{
    constexpr VkClearValue colorClear = {.color = {0.0f, 0.1f, 0.2f, 1.0f}};
    const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({extents[0], extents[1]}, &colorAttachment, &depthAttachment);


    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, traditionalPipeline.pipeline.handle);

    VkViewport viewport = Renderer::VkHelpers::GenerateViewport(extents[0], extents[1]);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(extents[0], extents[1]);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];

    Renderer::TraditionalPipelinePushConstant pushData{
        .sceneData = currentSceneDataBuffer.address,
        .materialBuffer = materialBuffer.address,
        .primitiveBuffer = traditionalPrimitiveBuffer.address,
        .modelBuffer = modelBuffer.address,
        .instanceBuffer = instanceBuffer.address,
    };

    vkCmdPushConstants(cmd, traditionalPipeline.pipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::TraditionalPipelinePushConstant), &pushData);
    constexpr VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &megaVertexBuffer.handle, &vertexOffset);
    vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(
        cmd,
        bunnyModel.indexCount, // 144,046 triangles * 3
        125, // instanceCount - all instances
        bunnyModel.indexOffset,
        bunnyModel.vertexOffset,
        0
    );

    vkCmdEndRendering(cmd);
}

void MeshTaskBenchmark::IndirectTraditional(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd)
{
    VkBufferMemoryBarrier2 bufferBarriers[2];
    bufferBarriers[0] = Renderer::VkHelpers::BufferMemoryBarrier(
        traditionalIndirectBuffer.handle, 0, sizeof(uint32_t),
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    bufferBarriers[1] = Renderer::VkHelpers::BufferMemoryBarrier(
        traditionalIndirectBuffer.handle, sizeof(glm::vec4), sizeof(VkDrawIndexedIndirectCommand) * 125,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.bufferMemoryBarrierCount = 2;
    depInfo.pBufferMemoryBarriers = bufferBarriers;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkCmdFillBuffer(cmd, traditionalIndirectBuffer.handle, 0, sizeof(uint32_t), 0);

    VkBufferMemoryBarrier2 bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
        traditionalIndirectBuffer.handle, 0, sizeof(uint32_t),
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &bufferBarrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, indirectTraditionalCompute.drawCullPipeline.handle);
    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    Renderer::TraditionalIndirectComputePushConstant pushData{
        .sceneData = currentSceneDataBuffer.address,
        .primitiveBuffer = traditionalPrimitiveBuffer.address,
        .modelBuffer = modelBuffer.address,
        .instanceBuffer = instanceBuffer.address,
        .indirectBuffer = traditionalIndirectBuffer.address,
    };

    vkCmdPushConstants(cmd, indirectTraditionalCompute.drawCullPipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Renderer::TraditionalIndirectComputePushConstant), &pushData);
    constexpr uint32_t groupsX = (125 + 63) / 64;
    vkCmdDispatch(cmd, groupsX, 1, 1);

    bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
        traditionalIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    depInfo.pBufferMemoryBarriers = &bufferBarrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);


    constexpr VkClearValue colorClear = {.color = {0.2f, 0.1f, 0.0f, 1.0f}};
    const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({extents[0], extents[1]}, &colorAttachment, &depthAttachment);


    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, indirectTraditionalGraphics.pipeline.handle);

    VkViewport viewport = Renderer::VkHelpers::GenerateViewport(extents[0], extents[1]);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(extents[0], extents[1]);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    Renderer::TraditionalIndirectRenderPushConstant pushData2{
        .sceneData = currentSceneDataBuffer.address,
        .materialBuffer = materialBuffer.address,
        .primitiveBuffer = traditionalPrimitiveBuffer.address,
        .modelBuffer = modelBuffer.address,
        .instanceBuffer = instanceBuffer.address,
    };

    vkCmdPushConstants(cmd, indirectTraditionalGraphics.pipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::TraditionalIndirectRenderPushConstant),
                       &pushData2);

    constexpr VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &megaVertexBuffer.handle, &vertexOffset);
    vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexedIndirectCount(cmd, traditionalIndirectBuffer.handle, sizeof(glm::vec4), traditionalIndirectBuffer.handle, 0, 125, sizeof(VkDrawIndexedIndirectCommand));
    vkCmdEndRendering(cmd);
}

void MeshTaskBenchmark::Meshlet(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd)
{
    constexpr VkClearValue colorClear = {.color = {0.0f, 0.2f, 0.1f, 1.0f}};
    const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({extents[0], extents[1]}, &colorAttachment, &depthAttachment);


    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taskMeshPipeline.pipeline.handle);

    VkViewport viewport = Renderer::VkHelpers::GenerateViewport(extents[0], extents[1]);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(extents[0], extents[1]);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    Renderer::TaskMeshPushConstant pushConstants{
        .sceneData = currentSceneDataBuffer.address,
        .vertexBuffer = megaVertexBuffer.address,
        .primitiveBuffer = meshletPrimitiveBuffer.address,
        .meshletVerticesBuffer = megaMeshletVerticesBuffer.address,
        .meshletTrianglesBuffer = megaMeshletTrianglesBuffer.address,
        .meshletBuffer = megaMeshletBuffer.address,
        .materialBuffer = materialBuffer.address,
        .modelBuffer = modelBuffer.address,
        .instanceBuffer = instanceBuffer.address,
    };

    vkCmdPushConstants(cmd, taskMeshPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(Renderer::TaskMeshPushConstant), &pushConstants);
    //vkCmdPushConstants(cmd, basicMeshShaderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::BasicMeshShaderPushConstants), &pushData);

    uint32_t numChunks = (bunnyModel.meshletCount + (64 - 1)) / 64;
    vkCmdDrawMeshTasksEXT(cmd, numChunks, 125, 1);
    vkCmdEndRendering(cmd);
}

void MeshTaskBenchmark::IndirectMeshlet(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd)
{
    VkBufferMemoryBarrier2 bufferBarriers[2];
    bufferBarriers[0] = Renderer::VkHelpers::BufferMemoryBarrier(
        meshletIndirectBuffer.handle, 0, sizeof(uint32_t),
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    bufferBarriers[1] = Renderer::VkHelpers::BufferMemoryBarrier(
        meshletIndirectBuffer.handle, sizeof(glm::vec4), sizeof(Renderer::TaskIndirectDrawParameters) * Renderer::BINDLESS_INSTANCE_COUNT * 4,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.bufferMemoryBarrierCount = 2;
    depInfo.pBufferMemoryBarriers = bufferBarriers;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkCmdFillBuffer(cmd, meshletIndirectBuffer.handle, 0, sizeof(uint32_t), 0);

    VkBufferMemoryBarrier2 bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
        meshletIndirectBuffer.handle, 0, sizeof(uint32_t),
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers = &bufferBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, indirectTaskMeshCompute.pipeline.handle);

    Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    Renderer::IndirectTaskMeshComputePushConstant pushData{
        .sceneData = currentSceneDataBuffer.address,
        .primitiveBuffer = meshletPrimitiveBuffer.address,
        .instanceBuffer = instanceBuffer.address,
        .modelBuffer = modelBuffer.address,
        .taskIndirectParameterBuffer = meshletIndirectBuffer.address
    };

    vkCmdPushConstants(cmd, indirectTaskMeshCompute.pipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Renderer::IndirectTaskMeshComputePushConstant), &pushData);
    constexpr uint32_t groupsX = (125 + 63) / 64;
    vkCmdDispatch(cmd, groupsX, 1, 1);


    bufferBarrier = Renderer::VkHelpers::BufferMemoryBarrier(
        meshletIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    depInfo.pBufferMemoryBarriers = &bufferBarrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    constexpr VkClearValue colorClear = {.color = {0.1f, 0.2f, 0.0f, 1.0f}};
    const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({extents[0], extents[1]}, &colorAttachment, &depthAttachment);


    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, indirectTaskMeshGraphics.pipeline.handle);

    VkViewport viewport = Renderer::VkHelpers::GenerateViewport(extents[0], extents[1]);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(extents[0], extents[1]);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    Renderer::IndirectTaskMeshRenderPushConstant pushData2{
        .sceneData = currentSceneDataBuffer.address,
        .vertexBuffer = megaVertexBuffer.address,
        .meshletVerticesBuffer = megaMeshletVerticesBuffer.address,
        .meshletTrianglesBuffer = megaMeshletTrianglesBuffer.address,
        .meshletBuffer = megaMeshletBuffer.address,
        .meshIndirectParameterBuffer = meshletIndirectBuffer.address,
        .materialBuffer = materialBuffer.address,
        .modelBuffer = modelBuffer.address,
    };

    vkCmdPushConstants(cmd, indirectTaskMeshGraphics.pipelineLayout.handle, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(Renderer::IndirectTaskMeshRenderPushConstant), &pushData2);

    vkCmdDrawMeshTasksIndirectCountEXT(cmd, meshletIndirectBuffer.handle, sizeof(glm::vec4), meshletIndirectBuffer.handle, 0, Renderer::BINDLESS_INSTANCE_COUNT, sizeof(glm::vec4) * 2);
    vkCmdEndRendering(cmd);
}

void MeshTaskBenchmark::Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync)
{
    VK_CHECK(vkWaitForFences(context->device, 1, &frameSync.renderFence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(context->device, 1, &frameSync.renderFence));

    std::array extents = {swapchain->extent.width, swapchain->extent.height};

    const Input& input = Input::Input::Get();
    const float deltaTime = Time::Get().GetDeltaTime();

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
        sceneData.frustum = Renderer::Frustum(sceneData.viewProj);
        sceneData.cameraWorldPos = {freeCamera.transform.translation, 0.0f};
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

    switch (benchmarkType) {
        case BenchmarkType::Traditional:
            Traditional(currentFrameInFlight, extents, cmd);
            break;
        case BenchmarkType::IndirectTraditional:
            IndirectTraditional(currentFrameInFlight, extents, cmd);
            break;
        case BenchmarkType::Meshlet:
            Meshlet(currentFrameInFlight, extents, cmd);
            break;
        case BenchmarkType::IndirectMeshlet:
            IndirectMeshlet(currentFrameInFlight, extents, cmd);
            break;
        default:
            break;
    }

    uint32_t swapchainImageIndex;

    VkResult e = vkAcquireNextImageKHR(context->device, swapchain->handle, UINT64_MAX, frameSync.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        fmt::println("Swapchain out of date or suboptimal (Acquire)");
        exit(1);
        return;
    }
    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];

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

void MeshTaskBenchmark::CreateBuffers()
{
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
        sceneDataBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo));
    }


    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT;
    megaVertexBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT;
    materialBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaIndexBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.size = sizeof(Renderer::TraditionalPrimitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT;
    traditionalPrimitiveBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletVerticesBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.size = sizeof(uint8_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletTrianglesBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaMeshletBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.size = sizeof(Renderer::MeshletPrimitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT;
    meshletPrimitiveBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.size = sizeof(Renderer::Model) * Renderer::BINDLESS_MODEL_MATRIX_COUNT;
    modelBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.size = sizeof(Renderer::Instance) * Renderer::BINDLESS_INSTANCE_COUNT;
    instanceBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);

    vmaAllocInfo.flags = 0;
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bufferInfo.usage = VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    bufferInfo.size = sizeof(glm::vec4) + sizeof(VkDrawIndexedIndirectCommand) * Renderer::BINDLESS_INSTANCE_COUNT;
    traditionalIndirectBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
    // vec4 for indirect count + padding. Instance_count * 4 is an assumption that each instance is likely to have at most 4 * 32 meshlets
    bufferInfo.size = sizeof(glm::vec4) + sizeof(Renderer::TaskIndirectDrawParameters) * Renderer::BINDLESS_INSTANCE_COUNT * 4;
    meshletIndirectBuffer = Renderer::VkResources::CreateAllocatedBuffer(context.get(), bufferInfo, vmaAllocInfo);
}

void MeshTaskBenchmark::InstanceGeneration()
{
    constexpr int32_t gridSize = 5;
    constexpr float spacing = 3.0f;
    constexpr int32_t totalInstances = gridSize * gridSize * gridSize;

    // Hard coded bunny rotation-fix
    glm::quat bunnyRotation(0.7071068f, 0.7071068f, 0.0f, 0.0f);
    glm::mat4 bunnyCorrection = glm::mat4_cast(bunnyRotation);

    std::vector<Renderer::Model> models;
    models.reserve(totalInstances);

    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            for (int z = 0; z < gridSize; z++) {
                glm::mat4 mat{1.0f};
                mat = glm::translate(mat, glm::vec3(
                                         x * spacing - (gridSize * spacing) / 2.0f,
                                         y * spacing - (gridSize * spacing) / 2.0f,
                                         z * spacing - (gridSize * spacing) / 2.0f
                                     ));
                mat = mat * bunnyCorrection;

                models.push_back(Renderer::Model{mat});
            }
        }
    }

    memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData), models.data(), models.size() * sizeof(Renderer::Model));

    std::vector<Renderer::Instance> instances;
    instances.reserve(totalInstances);

    for (uint32_t i = 0; i < totalInstances; i++) {
        Renderer::Instance inst;
        inst.modelIndex = i;
        inst.primitiveIndex = 0;
        inst.bIsAllocated = 1;
        inst.jointMatrixOffset = 0;
        instances.push_back(inst);
    }

    memcpy(static_cast<char*>(instanceBuffer.allocationInfo.pMappedData), instances.data(), instances.size() * sizeof(Renderer::Instance));

    fmt::println("Spawning {} instances of the bunny", totalInstances);
}

glm::vec4 MeshTaskBenchmark::GenerateBoundingSphere(const std::vector<Renderer::Vertex>& vertices)
{
    glm::vec3 center = {0, 0, 0};

    for (auto&& vertex : vertices) {
        center += vertex.position;
    }
    center /= static_cast<float>(vertices.size());


    float radius = glm::dot(vertices[0].position - center, vertices[0].position - center);
    for (size_t i = 1; i < vertices.size(); ++i) {
        radius = std::max(radius, glm::dot(vertices[i].position - center, vertices[i].position - center));
    }
    radius = std::nextafter(sqrtf(radius), std::numeric_limits<float>::max());

    return glm::vec4(center, radius);
}

Renderer::ModelData MeshTaskBenchmark::LoadModel(const std::filesystem::path& path)
{
    Renderer::ModelData model{};
    fastgltf::Parser parser{fastgltf::Extensions::KHR_texture_basisu | fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform};
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                                 | fastgltf::Options::AllowDouble
                                 | fastgltf::Options::LoadExternalBuffers
                                 | fastgltf::Options::LoadExternalImages;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!static_cast<bool>(gltfFile)) {
        fmt::println("Failed to open glTF file. "
                     "It's supposed to be at {}. "
                     "Please download the Stanford Bunny from https://casual-effects.com/data/ and convert it from .obj to .gltf using blender.", absolute(path).string());
        return {};
    }

    auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    if (!load) {
        fmt::println("Failed to load glTF: {}\n", to_underlying(load.error()));
        return {};
    }

    model.name = path.filename().string();
    model.path = path;
    fastgltf::Asset gltf = std::move(load.get());

    model.meshes.reserve(gltf.meshes.size());

    // Used by both
    std::vector<Renderer::Vertex> allVertices{};
    std::vector<Renderer::MaterialProperties> materials{};

    // Traditional only
    std::vector<Renderer::TraditionalPrimitive> traditionalPrimitives{};
    std::vector<uint32_t> allIndices{};

    // Meshlet Only
    std::vector<Renderer::MeshletPrimitive> meshletPrimitives{};
    std::vector<Renderer::Meshlet> allMeshlets{};
    std::vector<uint32_t> allMeshletVertices{};
    std::vector<uint8_t> allMeshletTriangles{};

    // Temp loop vars
    std::vector<Renderer::Vertex> primitiveVertices{};
    std::vector<uint32_t> primitiveIndices{};

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        Renderer::MeshInformation meshData{};
        meshData.name = mesh.name;
        meshData.primitiveIndices.reserve(mesh.primitives.size());
        meshletPrimitives.reserve(meshletPrimitives.size() + mesh.primitives.size());
        traditionalPrimitives.reserve(traditionalPrimitives.size() + mesh.primitives.size());

        for (fastgltf::Primitive& p : mesh.primitives) {
            Renderer::TraditionalPrimitive traditionalPrimitive{};
            Renderer::MeshletPrimitive meshletPrimitive{};

            if (p.materialIndex.has_value()) {
                constexpr int32_t materialIndexOffset = 1;
                traditionalPrimitive.materialIndex = p.materialIndex.value() + materialIndexOffset;
                meshletPrimitive.materialIndex = p.materialIndex.value() + materialIndexOffset;
            }

            // INDICES
            const fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
            primitiveIndices.clear();
            primitiveIndices.reserve(indexAccessor.count);

            fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](const std::uint32_t idx) {
                primitiveIndices.push_back(idx);
            });

            // POSITION (REQUIRED)
            const fastgltf::Attribute* positionIt = p.findAttribute("POSITION");
            const fastgltf::Accessor& posAccessor = gltf.accessors[positionIt->accessorIndex];
            primitiveVertices.clear();
            primitiveVertices.resize(posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, posAccessor, [&](fastgltf::math::fvec3 v, const size_t index) {
                primitiveVertices[index] = {};
                primitiveVertices[index].position = {v.x(), v.y(), v.z()};
            });


            // NORMALS
            const fastgltf::Attribute* normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](fastgltf::math::fvec3 n, const size_t index) {
                    primitiveVertices[index].normal = {n.x(), n.y(), n.z()};
                });
            }

            // TANGENTS
            const fastgltf::Attribute* tangents = p.findAttribute("TANGENT");
            if (tangents != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex], [&](fastgltf::math::fvec4 t, const size_t index) {
                    primitiveVertices[index].tangent = {t.x(), t.y(), t.z(), t.w()};
                });
            }

            // UV
            const fastgltf::Attribute* uvs = p.findAttribute("TEXCOORD_0");
            if (uvs != p.attributes.end()) {
                const fastgltf::Accessor& uvAccessor = gltf.accessors[uvs->accessorIndex];

                switch (uvAccessor.componentType) {
                    case fastgltf::ComponentType::Byte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s8vec2>(gltf, uvAccessor, [&](fastgltf::math::s8vec2 uv, const size_t index) {
                            // f = max(c / 127.0, -1.0)
                            float u = std::max(static_cast<float>(uv.x()) / 127.0f, -1.0f);
                            float v = std::max(static_cast<float>(uv.y()) / 127.0f, -1.0f);
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedByte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec2>(gltf, uvAccessor, [&](fastgltf::math::u8vec2 uv, const size_t index) {
                            // f = c / 255.0
                            float u = static_cast<float>(uv.x()) / 255.0f;
                            float v = static_cast<float>(uv.y()) / 255.0f;
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Short:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::s16vec2>(gltf, uvAccessor, [&](fastgltf::math::s16vec2 uv, const size_t index) {
                            // f = max(c / 32767.0, -1.0)
                            float u = std::max(
                                static_cast<float>(uv.x()) / 32767.0f, -1.0f);
                            float v = std::max(
                                static_cast<float>(uv.y()) / 32767.0f, -1.0f);
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::UnsignedShort:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec2>(gltf, uvAccessor, [&](fastgltf::math::u16vec2 uv, const size_t index) {
                            // f = c / 65535.0
                            float u = static_cast<float>(uv.x()) / 65535.0f;
                            float v = static_cast<float>(uv.y()) / 65535.0f;
                            primitiveVertices[index].uv = {u, v};
                        });
                        break;
                    case fastgltf::ComponentType::Float:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, uvAccessor, [&](fastgltf::math::fvec2 uv, const size_t index) {
                            primitiveVertices[index].uv = {uv.x(), uv.y()};
                        });
                        break;
                    default:
                        fmt::print("Unsupported UV component type: {}\n", static_cast<int>(uvAccessor.componentType));
                        break;
                }
            }

            // VERTEX COLOR
            const fastgltf::Attribute* colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](const fastgltf::math::fvec4& color, const size_t index) {
                    primitiveVertices[index].color = {
                        color.x(), color.y(), color.z(), color.w()
                    };
                });
            }


            // ====== Traditional Primitive =====
            traditionalPrimitive.firstIndex = static_cast<uint32_t>(allIndices.size());
            traditionalPrimitive.vertexOffset = static_cast<int32_t>(allVertices.size());
            traditionalPrimitive.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            traditionalPrimitive.boundingSphere = GenerateBoundingSphere(primitiveVertices);

            // ===== Meshlet Primitive =====
            constexpr size_t meshletMaxVertices = 64;
            constexpr size_t meshletMaxTriangles = 64;

            // build clusters (meshlets) out of the mesh
            size_t max_meshlets = meshopt_buildMeshletsBound(primitiveIndices.size(), meshletMaxVertices, meshletMaxTriangles);
            std::vector<meshopt_Meshlet> meshlets(max_meshlets);
            std::vector<unsigned int> meshletVertices(primitiveIndices.size());
            std::vector<unsigned char> meshletTriangles(primitiveIndices.size());

            std::vector<uint32_t> primitiveVertexPositions;
            meshlets.resize(meshopt_buildMeshlets(&meshlets[0], &meshletVertices[0], &meshletTriangles[0],
                                                  primitiveIndices.data(), primitiveIndices.size(),
                                                  reinterpret_cast<const float*>(primitiveVertices.data()), primitiveVertices.size(), sizeof(Renderer::Vertex),
                                                  meshletMaxVertices, meshletMaxTriangles, 0.f));

            // Optimize each meshlet's micro index buffer/vertex layout individually
            for (auto& meshlet : meshlets) {
                meshopt_optimizeMeshlet(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset], meshlet.triangle_count, meshlet.vertex_count);
            }

            // Trim the meshlet data to minimize waste for meshletVertices/meshletTriangles
            const meshopt_Meshlet& last = meshlets.back();
            meshletVertices.resize(last.vertex_offset + last.vertex_count);
            meshletTriangles.resize(last.triangle_offset + last.triangle_count * 3);


            meshletPrimitive.meshletOffset = allMeshlets.size();
            meshletPrimitive.meshletCount = meshlets.size();
            meshletPrimitive.boundingSphere = GenerateBoundingSphere(primitiveVertices);

            uint32_t vertexOffset = allVertices.size();
            uint32_t meshletVertexOffset = allMeshletVertices.size();
            uint32_t meshletTrianglesOffset = allMeshletTriangles.size();

            allMeshletVertices.insert(allMeshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
            allMeshletTriangles.insert(allMeshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());

            for (meshopt_Meshlet& meshlet : meshlets) {
                meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                    &meshletVertices[meshlet.vertex_offset],
                    &meshletTriangles[meshlet.triangle_offset],
                    meshlet.triangle_count,
                    reinterpret_cast<const float*>(primitiveVertices.data()),
                    primitiveVertices.size(),
                    sizeof(Renderer::Vertex)
                );

                allMeshlets.push_back({
                    .meshletBoundingSphere = glm::vec4(
                        bounds.center[0], bounds.center[1], bounds.center[2],
                        bounds.radius
                    ),
                    .coneApex = glm::vec3(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]),
                    .coneCutoff = bounds.cone_cutoff,

                    .coneAxis = glm::vec3(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]),
                    .vertexOffset = vertexOffset,

                    .meshletVerticesOffset = meshletVertexOffset + meshlet.vertex_offset,
                    .meshletTriangleOffset = meshletTrianglesOffset + meshlet.triangle_offset,
                    .meshletVerticesCount = meshlet.vertex_count,
                    .meshletTriangleCount = meshlet.triangle_count,
                });
            }


            //
            allVertices.insert(allVertices.end(), primitiveVertices.begin(), primitiveVertices.end());
            allIndices.insert(allIndices.end(), primitiveIndices.begin(), primitiveIndices.end());

            meshData.primitiveIndices.push_back(traditionalPrimitives.size());
            traditionalPrimitives.push_back(traditionalPrimitive);
            meshletPrimitives.push_back(meshletPrimitive);
        }

        model.meshes.push_back(meshData);
    }


    // ===== Buffer Upload =====
    // === Used by both ===
    size_t sizeMaterials = materials.size() * sizeof(Renderer::MaterialProperties);
    model.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + model.materialAllocation.offset, materials.data(), sizeMaterials);

    size_t sizeVertices = allVertices.size() * sizeof(Renderer::Vertex);
    model.vertexAllocation = vertexBufferAllocator.allocate(sizeVertices);
    memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + model.vertexAllocation.offset, allVertices.data(), sizeVertices);


    // Used by traditional
    // Indices
    size_t sizeIndices = allIndices.size() * sizeof(uint32_t);
    model.indexAllocation = indexBufferAllocator.allocate(sizeIndices);
    memcpy(static_cast<char*>(megaIndexBuffer.allocationInfo.pMappedData) + model.indexAllocation.offset, allIndices.data(), sizeIndices);

    // Primitives
    uint32_t firstIndexCount = model.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = model.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t materialOffsetCount = model.materialAllocation.offset / sizeof(Renderer::MaterialProperties);

    for (auto& primitive : traditionalPrimitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += static_cast<int32_t>(vertexOffsetCount);
        if (primitive.materialIndex > 0) {
            primitive.materialIndex += materialOffsetCount;
        }
    }

    size_t sizeTraditionalPrimitives = traditionalPrimitives.size() * sizeof(Renderer::TraditionalPrimitive);
    model.traditionalPrimitiveAllocation = traditionalPrimitiveBufferAllocator.allocate(sizeTraditionalPrimitives);
    memcpy(static_cast<char*>(traditionalPrimitiveBuffer.allocationInfo.pMappedData) + model.traditionalPrimitiveAllocation.offset, traditionalPrimitives.data(), sizeTraditionalPrimitives);


    // === Used by meshlet ===
    // Meshlet Vertices
    size_t sizeMeshletVertices = allMeshletVertices.size() * sizeof(uint32_t);
    model.meshletVerticesAllocation = meshletVerticesBufferAllocator.allocate(sizeMeshletVertices);
    memcpy(static_cast<char*>(megaMeshletVerticesBuffer.allocationInfo.pMappedData) + model.meshletVerticesAllocation.offset, allMeshletVertices.data(), sizeMeshletVertices);

    // Meshlet Triangles
    size_t sizeMeshletTriangles = allMeshletTriangles.size() * sizeof(uint8_t);
    model.meshletTrianglesAllocation = meshletTrianglesBufferAllocator.allocate(sizeMeshletTriangles);
    memcpy(static_cast<char*>(megaMeshletTrianglesBuffer.allocationInfo.pMappedData) + model.meshletTrianglesAllocation.offset, allMeshletTriangles.data(), sizeMeshletTriangles);

    // Meshlets
    uint32_t vertexOffset = model.vertexAllocation.offset / sizeof(Renderer::Vertex);
    uint32_t meshletVerticesOffset = model.meshletVerticesAllocation.offset / sizeof(uint32_t);
    uint32_t meshletTriangleOffset = model.meshletTrianglesAllocation.offset / sizeof(uint8_t);
    for (Renderer::Meshlet& meshlet : allMeshlets) {
        meshlet.vertexOffset += vertexOffset;
        meshlet.meshletVerticesOffset += meshletVerticesOffset;
        meshlet.meshletTriangleOffset += meshletTriangleOffset;
    }

    size_t sizeMeshlets = allMeshlets.size() * sizeof(Renderer::Meshlet);
    model.meshletAllocation = meshletBufferAllocator.allocate(sizeMeshlets);
    memcpy(static_cast<char*>(megaMeshletBuffer.allocationInfo.pMappedData) + model.meshletAllocation.offset, allMeshlets.data(), sizeMeshlets);

    // Primitives
    uint32_t meshletOffset = model.meshletAllocation.offset / sizeof(Renderer::Meshlet);
    for (auto& primitive : meshletPrimitives) {
        primitive.meshletOffset += meshletOffset;
        if (primitive.materialIndex > 0) {
            primitive.materialIndex += materialOffsetCount;
        }
    }

    size_t sizePrimitives = meshletPrimitives.size() * sizeof(Renderer::MeshletPrimitive);
    model.meshletPrimitiveAllocation = meshletPrimitiveBufferAllocator.allocate(sizePrimitives);
    memcpy(static_cast<char*>(meshletPrimitiveBuffer.allocationInfo.pMappedData) + model.meshletPrimitiveAllocation.offset, meshletPrimitives.data(), sizePrimitives);

    // Offset primitive index once. Should be the same for both.
    uint32_t primitiveOffsetCount = model.traditionalPrimitiveAllocation.offset / sizeof(Renderer::TraditionalPrimitive);
    uint32_t meshletOffsetCount = model.meshletPrimitiveAllocation.offset / sizeof(Renderer::MeshletPrimitive);
    if (primitiveOffsetCount != meshletOffsetCount) {
        fmt::println("Offsets do not match");
        exit(1);
    }

    for (auto& mesh : model.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }

    fmt::println("Model Name          : {}", model.name);
    fmt::println("Model               : {} Vertices, {} Tris, {} Materials", allVertices.size(), allIndices.size() / 3, materials.size());
    fmt::println("Model (Traditional) : {} primitives", traditionalPrimitives.size());
    fmt::println("Model (Meshlet)     : {} meshlets across {} primitives", allMeshlets.size(), meshletPrimitives.size());
    model.indexCount = allIndices.capacity();
    model.indexOffset = 0;
    model.vertexOffset = 0;
    model.meshletCount = allMeshlets.size();
    return model;
}
