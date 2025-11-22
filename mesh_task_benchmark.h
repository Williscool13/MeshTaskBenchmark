//
// Created by William on 2025-11-22.
//

#ifndef MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#define MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#include <memory>
#include <SDL3/SDL.h>

namespace Renderer
{
struct VulkanContext;
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

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> context{};
};


#endif //MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H