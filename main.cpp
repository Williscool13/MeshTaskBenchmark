#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "mesh_task_benchmark.h"

int main()
{
    MeshTaskBenchmark mtb{};
    mtb.Initialize();
    mtb.Run();
    mtb.Cleanup();

    return 0;
}
