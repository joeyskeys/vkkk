#include <cstring>
#include <unordered_map>

#include "vk_ins/context.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef MemoryBarrier
#undef MemoryBarrier
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_win32.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace vkkk
{
namespace
{

#ifdef _WIN32
#define VKKK_CUDAAPI __stdcall
#else
#define VKKK_CUDAAPI
#endif

using CUresult = int;
using CUdeviceptr = unsigned long long;
using CUexternalMemory = void*;

constexpr int kCudaSuccess = 0;
constexpr unsigned int kCudaExternalMemoryOpaqueFd = 1;
constexpr unsigned int kCudaExternalMemoryOpaqueWin32 = 2;

struct CudaExternalMemoryHandleDesc {
    unsigned int type = 0;
    union {
        int fd;
        struct {
            void* handle;
            const void* name;
        } win32;
        const void* nvSciBufObject;
    } handle{};
    unsigned long long size = 0;
    unsigned int flags = 0;
    unsigned int reserved[16]{};
};

struct CudaExternalMemoryBufferDesc {
    unsigned long long offset = 0;
    unsigned long long size = 0;
    unsigned int flags = 0;
    unsigned int reserved[16]{};
};

using CuInitFn = CUresult(VKKK_CUDAAPI*)(unsigned int);
using CuImportExternalMemoryFn =
    CUresult(VKKK_CUDAAPI*)(CUexternalMemory*, const CudaExternalMemoryHandleDesc*);
using CuExternalMemoryGetMappedBufferFn =
    CUresult(VKKK_CUDAAPI*)(CUdeviceptr*, CUexternalMemory, const CudaExternalMemoryBufferDesc*);
using CuDestroyExternalMemoryFn = CUresult(VKKK_CUDAAPI*)(CUexternalMemory);
using CuMemcpyDtoDFn = CUresult(VKKK_CUDAAPI*)(CUdeviceptr, CUdeviceptr, size_t);

} // namespace

struct CudaMappedBuffer {
    uint64_t vk_buffer = 0;
    void* external_memory = nullptr;
    uint64_t device_ptr = 0;
    vk::DeviceSize bytes = 0;
};

struct CudaInteropState {
    void* library = nullptr;
    CuInitFn cuInit = nullptr;
    CuImportExternalMemoryFn cuImportExternalMemory = nullptr;
    CuExternalMemoryGetMappedBufferFn cuExternalMemoryGetMappedBuffer = nullptr;
    CuDestroyExternalMemoryFn cuDestroyExternalMemory = nullptr;
    CuMemcpyDtoDFn cuMemcpyDtoD = nullptr;
    bool loaded = false;
    std::unordered_map<std::string, CudaMappedBuffer> maps;

    ~CudaInteropState() {
        for (auto& [_, mapped] : maps) {
            if (mapped.external_memory != nullptr && cuDestroyExternalMemory != nullptr) {
                cuDestroyExternalMemory(mapped.external_memory);
            }
        }
        maps.clear();
#ifdef _WIN32
        if (library != nullptr) {
            FreeLibrary(static_cast<HMODULE>(library));
        }
#else
        if (library != nullptr) {
            dlclose(library);
        }
#endif
    }
};

void CudaInteropDeleter::operator()(CudaInteropState* state) const {
    delete state;
}

namespace
{

template <typename T>
bool load_cuda_symbol(void* library, T& function, const char* name) {
#ifdef _WIN32
    FARPROC proc = GetProcAddress(static_cast<HMODULE>(library), name);
#else
    void* proc = dlsym(library, name);
#endif
    if (proc == nullptr) {
        return false;
    }
    function = reinterpret_cast<T>(proc);
    return true;
}

bool ensure_cuda_loaded(CudaInteropState& state) {
    if (state.loaded) {
        return true;
    }
#ifdef _WIN32
    state.library = LoadLibraryA("nvcuda.dll");
#else
    state.library = dlopen("libcuda.so.1", RTLD_NOW);
    if (state.library == nullptr) {
        state.library = dlopen("libcuda.so", RTLD_NOW);
    }
#endif
    if (state.library == nullptr) {
        return false;
    }
    if (!load_cuda_symbol(state.library, state.cuInit, "cuInit")
        || !load_cuda_symbol(state.library, state.cuImportExternalMemory, "cuImportExternalMemory")
        || !load_cuda_symbol(state.library, state.cuExternalMemoryGetMappedBuffer,
            "cuExternalMemoryGetMappedBuffer")
        || !load_cuda_symbol(state.library, state.cuDestroyExternalMemory, "cuDestroyExternalMemory")
        || !load_cuda_symbol(state.library, state.cuMemcpyDtoD, "cuMemcpyDtoD_v2"))
    {
        return false;
    }
    if (state.cuInit(0) != kCudaSuccess) {
        return false;
    }
    state.loaded = true;
    return true;
}

bool export_memory_handle(const vk::raii::Device& device, const vk::raii::DeviceMemory& memo,
    vk::DeviceSize alloc_bytes, CudaExternalMemoryHandleDesc& desc)
{
#ifdef _WIN32
    auto get_handle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        device.getProcAddr("vkGetMemoryWin32HandleKHR"));
    if (get_handle == nullptr) {
        return false;
    }
    VkMemoryGetWin32HandleInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    info.memory = *memo;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    HANDLE handle = nullptr;
    if (get_handle(static_cast<VkDevice>(*device), &info, &handle) != VK_SUCCESS || handle == nullptr) {
        return false;
    }
    desc.type = kCudaExternalMemoryOpaqueWin32;
    desc.handle.win32.handle = handle;
    desc.handle.win32.name = nullptr;
    desc.size = alloc_bytes;
    return true;
#else
    auto get_fd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(device.getProcAddr("vkGetMemoryFdKHR"));
    if (get_fd == nullptr) {
        return false;
    }
    VkMemoryGetFdInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    info.memory = *memo;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    int fd = -1;
    if (get_fd(static_cast<VkDevice>(*device), &info, &fd) != VK_SUCCESS || fd < 0) {
        return false;
    }
    desc.type = kCudaExternalMemoryOpaqueFd;
    desc.handle.fd = fd;
    desc.size = alloc_bytes;
    return true;
#endif
}

void close_exported_handle(CudaExternalMemoryHandleDesc& desc) {
#ifdef _WIN32
    if (desc.handle.win32.handle != nullptr) {
        CloseHandle(desc.handle.win32.handle);
        desc.handle.win32.handle = nullptr;
    }
#else
    if (desc.handle.fd >= 0) {
        close(desc.handle.fd);
        desc.handle.fd = -1;
    }
#endif
}

} // namespace

MeshGPU* Context::find_draw_mesh(const std::string& name) {
    if (auto found = deformable_meshes.find(name); found != deformable_meshes.end()) {
        return &found->second;
    }
    if (auto found = meshes.find(name); found != meshes.end()) {
        return &found->second;
    }
    return nullptr;
}

DeformableMeshGPU* Context::find_deformable_mesh(const std::string& name) {
    if (auto found = deformable_meshes.find(name); found != deformable_meshes.end()) {
        return &found->second;
    }
    return nullptr;
}

bool Context::map_mesh_vertices_to_cuda(const MeshGPU& mesh, const std::string& map_key,
    CudaDeviceBuffer& view)
{
    view = {};
    if (!external_memory_export_available || *mesh.vbuf == VK_NULL_HANDLE
        || *mesh.vbuf_memo == VK_NULL_HANDLE || mesh.vert_bytes == 0)
    {
        return false;
    }
    if (!cuda_interop) {
        cuda_interop.reset(new CudaInteropState());
    }
    if (!ensure_cuda_loaded(*cuda_interop)) {
        return false;
    }

    const uint64_t vk_buffer = reinterpret_cast<uint64_t>(static_cast<VkBuffer>(*mesh.vbuf));
    auto& mapped = cuda_interop->maps[map_key];
    if (mapped.vk_buffer == vk_buffer && mapped.device_ptr != 0) {
        view.device_ptr = mapped.device_ptr;
        view.bytes = mesh.vert_bytes;
        return true;
    }
    if (mapped.external_memory != nullptr) {
        cuda_interop->cuDestroyExternalMemory(mapped.external_memory);
        mapped = {};
    }

    const vk::MemoryRequirements mem_reqs = mesh.vbuf.getMemoryRequirements();
    CudaExternalMemoryHandleDesc handle_desc{};
    if (!export_memory_handle(device, mesh.vbuf_memo, mem_reqs.size, handle_desc)) {
        return false;
    }

    CUexternalMemory ext_mem = nullptr;
    const CUresult import_rc = cuda_interop->cuImportExternalMemory(&ext_mem, &handle_desc);
    close_exported_handle(handle_desc);
    if (import_rc != kCudaSuccess || ext_mem == nullptr) {
        return false;
    }

    CudaExternalMemoryBufferDesc buffer_desc{};
    buffer_desc.size = mem_reqs.size;
    CUdeviceptr device_ptr = 0;
    if (cuda_interop->cuExternalMemoryGetMappedBuffer(&device_ptr, ext_mem, &buffer_desc) != kCudaSuccess
        || device_ptr == 0)
    {
        cuda_interop->cuDestroyExternalMemory(ext_mem);
        return false;
    }

    mapped.vk_buffer = vk_buffer;
    mapped.external_memory = ext_mem;
    mapped.device_ptr = device_ptr;
    mapped.bytes = mesh.vert_bytes;
    view.device_ptr = device_ptr;
    view.bytes = mesh.vert_bytes;
    return true;
}

bool Context::mesh_cuda_vertex_ptr(const std::string& name, CudaDeviceBuffer& view) {
    const MeshGPU* mesh = find_draw_mesh(name);
    if (mesh == nullptr) {
        return false;
    }
    return map_mesh_vertices_to_cuda(*mesh, name + ".draw", view);
}

bool Context::mesh_cuda_rest_ptr(const std::string& name, CudaDeviceBuffer& view) {
    const DeformableMeshGPU* mesh = find_deformable_mesh(name);
    if (mesh == nullptr) {
        return false;
    }
    return map_mesh_vertices_to_cuda(mesh->rest_mesh, name + ".rest", view);
}

bool Context::write_mesh_vertices(const std::string& name, vk::raii::Buffer& src, vk::DeviceSize bytes) {
    MeshGPU* mesh = find_draw_mesh(name);
    if (mesh == nullptr || *mesh->vbuf == VK_NULL_HANDLE || bytes == 0 || bytes > mesh->vert_bytes) {
        return false;
    }
    wait_idle();
    copy_buffer(src, mesh->vbuf, bytes);
    return true;
}

bool Context::write_mesh_vertices_from_cuda(const std::string& name, uint64_t src_device_ptr,
    vk::DeviceSize bytes)
{
    if (src_device_ptr == 0) {
        return false;
    }
    CudaDeviceBuffer dest{};
    if (!mesh_cuda_vertex_ptr(name, dest) || bytes == 0 || bytes > dest.bytes) {
        return false;
    }
    return cuda_interop
        && cuda_interop->cuMemcpyDtoD(dest.device_ptr, src_device_ptr, static_cast<size_t>(bytes))
            == kCudaSuccess;
}

bool Context::copy_mesh_rest_to_draw(const std::string& name) {
    DeformableMeshGPU* mesh = find_deformable_mesh(name);
    if (mesh == nullptr || *mesh->vbuf == VK_NULL_HANDLE || *mesh->rest_mesh.vbuf == VK_NULL_HANDLE
        || mesh->vert_bytes == 0 || mesh->rest_mesh.vert_bytes != mesh->vert_bytes)
    {
        return false;
    }
    wait_idle();
    copy_buffer(mesh->rest_mesh.vbuf, mesh->vbuf, mesh->vert_bytes);
    return true;
}

} // namespace vkkk
