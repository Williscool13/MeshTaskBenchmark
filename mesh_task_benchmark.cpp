//
// Created by William on 2025-11-22.
//

#include "mesh_task_benchmark.h"

#include <fmt/format.h>

#include "render/vk_context.h"
#include "input/input.h"

MeshTaskBenchmark::MeshTaskBenchmark() = default;

MeshTaskBenchmark::~MeshTaskBenchmark() = default;

void MeshTaskBenchmark::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        fmt::println("SDL_Init failed: {}", SDL_GetError());
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

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
}
void MeshTaskBenchmark::Run()
{
    fmt::println("Run");
}
void MeshTaskBenchmark::Cleanup()
{
    fmt::println("Cleanup");
}
