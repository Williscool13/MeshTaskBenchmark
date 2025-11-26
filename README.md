# Task and Mesh Shader Benchmark

Benchmarking traditional vertex pipelines against GPU-driven mesh shaders in Vulkan. Includes implementations of indirect drawing, task shaders, and mesh shaders with frustum and backface culling.

**Article:** https://www.williscool.com/technical/task-mesh-benchmarking/task-mesh-benchmarking.md.html

## What's Inside

- Traditional vertex pipeline (baseline)
- Indirect draw with GPU instance culling
- Task + Mesh shaders with meshlet-level culling
- Indirect + Task + Mesh with both GPU instance and meshlet-level culling

Test scene renders 125 Stanford Bunnies (72K vertices each) with meshlets generated via meshoptimizer.

## Build & Run

Everything you need is included - assets, dependencies, and code are all packaged in this repo.

**Requirements:**
- CMake 3.20+
- Visual Studio 2022
- Vulkan SDK 1.4+ (includes slangc compiler)
- Slang 2025.17.2 or newer
- GPU with mesh shader support (NVIDIA Turing+, AMD RDNA2+, Intel Arc)

**Build Instructions:**

1. Open CMake GUI
2. Set source directory to the cloned repo
3. Set build directory to `repo/build`
4. Click "Configure" → Select "Visual Studio 17 2022"
5. Click "Generate"
6. Click "Open Project" or navigate to `build/` and open the `.sln` file
7. Build in Visual Studio (Release recommended). Right-click `meshTaskBenchmark` project and select "Build"
8. Copy the `assets` folder from the repo root into the executable folder
9. Copy the compiled shader folder (in the build directory) into the executable folder

**Controls:**

Press number keys to switch between pipeline configurations:
- `1` - Traditional
- `2` - Indirect + Traditional
- `3` - Task + Mesh
- `4` - Indirect + Task + Mesh

## Results

Task + Mesh shaders achieve 5.3x performance over traditional rendering on this workload. Check the article for detailed analysis and profiler data.
