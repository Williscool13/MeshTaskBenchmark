//
// Created by William on 2025-11-22.
//

#ifndef MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#define MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "game/camera/free_camera.h"
#include "render/vk_resources.h"
#include "render/vk_types.h"
#include "render/pipelines/basic_mesh_shader_pipeline.h"

namespace Renderer
{
struct FrameSynchronization;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

static inline constexpr int32_t DEFAULT_WINDOW_WIDTH = 1700;
static inline constexpr int32_t DEFAULT_WINDOW_HEIGHT = 900;

class MeshTaskBenchmark
{
public:
    MeshTaskBenchmark();
    ~MeshTaskBenchmark();

    void Initialize();

    void Run();

    void Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync);

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> context{};
    std::unique_ptr<Renderer::Swapchain> swapchain;
    std::unique_ptr<Renderer::RenderTargets> renderTargets;
    std::vector<Renderer::FrameSynchronization> frameSynchronization;

    bool bShouldExit{false};
    uint32_t frameNumber{0};

    Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
    Renderer::SceneData sceneData{};
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;

    Renderer::BasicMeshShaderPipeline basicMeshShaderPipeline{};
};


#endif //MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H