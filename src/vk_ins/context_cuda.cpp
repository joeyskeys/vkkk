#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>
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
using CUjitOption = int;
using CUdevice = int;
using CUcontext = void*;
using CUdeviceptr = unsigned long long;
using CUexternalMemory = void*;
using CUmodule = void*;
using CUfunction = void*;
using CUstream = void*;

constexpr int kCudaSuccess = 0;
constexpr unsigned int kCudaExternalMemoryOpaqueFd = 1;
constexpr unsigned int kCudaExternalMemoryOpaqueWin32 = 2;
// Vulkan vertex allocations are created with VkMemoryDedicatedAllocateInfo.
constexpr unsigned int kCudaExternalMemoryDedicated = 0x1;
constexpr CUjitOption kCudaJitInfoLogBuffer = 3;
constexpr CUjitOption kCudaJitInfoLogBufferSizeBytes = 4;
constexpr CUjitOption kCudaJitErrorLogBuffer = 5;
constexpr CUjitOption kCudaJitErrorLogBufferSizeBytes = 6;
constexpr CUjitOption kCudaJitLogVerbose = 12;

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
using CuModuleLoadDataExFn =
    CUresult(VKKK_CUDAAPI*)(CUmodule*, const void*, unsigned int, CUjitOption*, void**);
using CuModuleGetFunctionFn =
    CUresult(VKKK_CUDAAPI*)(CUfunction*, CUmodule, const char*);
using CuModuleUnloadFn = CUresult(VKKK_CUDAAPI*)(CUmodule);
using CuLaunchKernelFn = CUresult(VKKK_CUDAAPI*)(
    CUfunction, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int,
    unsigned int, unsigned int, CUstream, void**, void**);
using CuCtxSynchronizeFn = CUresult(VKKK_CUDAAPI*)(void);
using CuDeviceGetFn = CUresult(VKKK_CUDAAPI*)(CUdevice*, int);
using CuDeviceGetNameFn = CUresult(VKKK_CUDAAPI*)(char*, int, CUdevice);
using CuCtxSetCurrentFn = CUresult(VKKK_CUDAAPI*)(CUcontext);
using CuDevicePrimaryCtxRetainFn = CUresult(VKKK_CUDAAPI*)(CUcontext*, CUdevice);
using CuDevicePrimaryCtxReleaseFn = CUresult(VKKK_CUDAAPI*)(CUdevice);
using CuGetErrorNameFn = CUresult(VKKK_CUDAAPI*)(CUresult, const char**);
using CuGetErrorStringFn = CUresult(VKKK_CUDAAPI*)(CUresult, const char**);

constexpr const char* kPositionScatterPtx = R"ptx(
.version 6.0
.target sm_52
.address_size 64

.visible .entry orl_scatter_point_positions(
    .param .u64 p_points,
    .param .u64 p_vertices,
    .param .f64 m0,
    .param .f64 m1,
    .param .f64 m2,
    .param .f64 m3,
    .param .f64 m4,
    .param .f64 m5,
    .param .f64 m6,
    .param .f64 m7,
    .param .f64 m8,
    .param .f64 m9,
    .param .f64 m10,
    .param .f64 m11,
    .param .u32 p_count,
    .param .u32 p_stride,
    .param .u32 p_offset
)
{
    .reg .pred %p;
    .reg .b32 %r<8>;
    .reg .b64 %rd<8>;
    .reg .f64 %fd<16>;
    .reg .f32 %f<4>;

    ld.param.u64 %rd0, [p_points];
    ld.param.u64 %rd1, [p_vertices];
    ld.param.f64 %fd4, [m0];
    ld.param.f64 %fd5, [m1];
    ld.param.f64 %fd6, [m2];
    ld.param.f64 %fd7, [m3];
    ld.param.f64 %fd8, [m4];
    ld.param.f64 %fd9, [m5];
    ld.param.f64 %fd10, [m6];
    ld.param.f64 %fd11, [m7];
    ld.param.f64 %fd12, [m8];
    ld.param.f64 %fd13, [m9];
    ld.param.f64 %fd14, [m10];
    ld.param.f64 %fd15, [m11];
    ld.param.u32 %r1, [p_count];
    ld.param.u32 %r2, [p_stride];
    ld.param.u32 %r3, [p_offset];

    mov.u32 %r0, %tid.x;
    mov.u32 %r5, %ctaid.x;
    mov.u32 %r6, %ntid.x;
    mad.lo.u32 %r0, %r5, %r6, %r0;
    setp.ge.u32 %p, %r0, %r1;
    @%p bra DONE;

    mul.wide.u32 %rd2, %r0, 32;
    add.u64 %rd2, %rd0, %rd2;
    ld.global.f64 %fd0, [%rd2];
    ld.global.f64 %fd1, [%rd2+8];
    ld.global.f64 %fd2, [%rd2+16];

    mul.f64 %fd3, %fd4, %fd0;
    fma.rn.f64 %fd3, %fd5, %fd1, %fd3;
    fma.rn.f64 %fd3, %fd6, %fd2, %fd3;
    add.f64 %fd3, %fd3, %fd7;
    cvt.rn.f32.f64 %f0, %fd3;

    mul.f64 %fd3, %fd8, %fd0;
    fma.rn.f64 %fd3, %fd9, %fd1, %fd3;
    fma.rn.f64 %fd3, %fd10, %fd2, %fd3;
    add.f64 %fd3, %fd3, %fd11;
    cvt.rn.f32.f64 %f1, %fd3;

    mul.f64 %fd3, %fd12, %fd0;
    fma.rn.f64 %fd3, %fd13, %fd1, %fd3;
    fma.rn.f64 %fd3, %fd14, %fd2, %fd3;
    add.f64 %fd3, %fd3, %fd15;
    cvt.rn.f32.f64 %f2, %fd3;

    mul.lo.u32 %r4, %r0, %r2;
    add.u32 %r4, %r4, %r3;
    mul.wide.u32 %rd3, %r4, 4;
    add.u64 %rd3, %rd1, %rd3;
    st.global.f32 [%rd3], %f0;
    st.global.f32 [%rd3+4], %f1;
    st.global.f32 [%rd3+8], %f2;

DONE:
    ret;
}
)ptx";

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
    CuModuleLoadDataExFn cuModuleLoadDataEx = nullptr;
    CuModuleGetFunctionFn cuModuleGetFunction = nullptr;
    CuModuleUnloadFn cuModuleUnload = nullptr;
    CuLaunchKernelFn cuLaunchKernel = nullptr;
    CuCtxSynchronizeFn cuCtxSynchronize = nullptr;
    CuDeviceGetFn cuDeviceGet = nullptr;
    CuDeviceGetNameFn cuDeviceGetName = nullptr;
    CuCtxSetCurrentFn cuCtxSetCurrent = nullptr;
    CuDevicePrimaryCtxRetainFn cuDevicePrimaryCtxRetain = nullptr;
    CuDevicePrimaryCtxReleaseFn cuDevicePrimaryCtxRelease = nullptr;
    CuGetErrorNameFn cuGetErrorName = nullptr;
    CuGetErrorStringFn cuGetErrorString = nullptr;
    CUmodule position_module = nullptr;
    CUfunction position_kernel = nullptr;
    CUdevice cuda_device = 0;
    CUcontext cuda_context = nullptr;
    bool primary_context_retained = false;
    bool loaded = false;
    std::unordered_map<std::string, CudaMappedBuffer> maps;

    ~CudaInteropState() {
        if (cuda_context != nullptr && cuCtxSetCurrent != nullptr) {
            cuCtxSetCurrent(cuda_context);
        }
        if (position_module != nullptr && cuModuleUnload != nullptr) {
            cuModuleUnload(position_module);
        }
        position_module = nullptr;
        position_kernel = nullptr;
        for (auto& [_, mapped] : maps) {
            if (mapped.external_memory != nullptr && cuDestroyExternalMemory != nullptr) {
                cuDestroyExternalMemory(mapped.external_memory);
            }
        }
        maps.clear();
        if (primary_context_retained && cuDevicePrimaryCtxRelease != nullptr) {
            cuDevicePrimaryCtxRelease(cuda_device);
        }
        primary_context_retained = false;
        cuda_context = nullptr;
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

bool activate_cuda_context(CudaInteropState& state);

bool ensure_cuda_loaded(CudaInteropState& state) {
    if (state.loaded) {
        return activate_cuda_context(state);
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
        || !load_cuda_symbol(state.library, state.cuMemcpyDtoD, "cuMemcpyDtoD_v2")
        || !load_cuda_symbol(state.library, state.cuModuleLoadDataEx, "cuModuleLoadDataEx")
        || !load_cuda_symbol(state.library, state.cuModuleGetFunction, "cuModuleGetFunction")
        || !load_cuda_symbol(state.library, state.cuModuleUnload, "cuModuleUnload")
        || !load_cuda_symbol(state.library, state.cuLaunchKernel, "cuLaunchKernel")
        || !load_cuda_symbol(state.library, state.cuCtxSynchronize, "cuCtxSynchronize")
        || !load_cuda_symbol(state.library, state.cuDeviceGet, "cuDeviceGet")
        || !load_cuda_symbol(state.library, state.cuCtxSetCurrent, "cuCtxSetCurrent")
        || !load_cuda_symbol(state.library, state.cuDevicePrimaryCtxRetain,
            "cuDevicePrimaryCtxRetain")
        || !load_cuda_symbol(state.library, state.cuDevicePrimaryCtxRelease,
            "cuDevicePrimaryCtxRelease"))
    {
        return false;
    }
    (void)load_cuda_symbol(state.library, state.cuGetErrorName, "cuGetErrorName");
    (void)load_cuda_symbol(state.library, state.cuGetErrorString, "cuGetErrorString");
    (void)load_cuda_symbol(state.library, state.cuDeviceGetName, "cuDeviceGetName");
    if (state.cuInit(0) != kCudaSuccess) {
        return false;
    }
    if (state.cuDeviceGet(&state.cuda_device, 0) != kCudaSuccess
        || state.cuDevicePrimaryCtxRetain(&state.cuda_context, state.cuda_device)
            != kCudaSuccess
        || state.cuda_context == nullptr)
    {
        return false;
    }
    state.primary_context_retained = true;
    if (!activate_cuda_context(state)) {
        state.cuDevicePrimaryCtxRelease(state.cuda_device);
        state.primary_context_retained = false;
        state.cuda_context = nullptr;
        return false;
    }
    char device_name[256] = {};
    if (state.cuDeviceGetName != nullptr
        && state.cuDeviceGetName(device_name, static_cast<int>(sizeof(device_name)),
            state.cuda_device) == kCudaSuccess)
    {
        std::cerr << "vkkk CUDA interop: using CUDA device " << state.cuda_device
            << " '" << device_name << "'\n";
    }
    state.loaded = true;
    return true;
}

bool activate_cuda_context(CudaInteropState& state) {
    return state.cuda_context != nullptr && state.cuCtxSetCurrent != nullptr
        && state.cuCtxSetCurrent(state.cuda_context) == kCudaSuccess;
}

std::string cuda_result_text(const CudaInteropState& state, CUresult result) {
    std::string text = "rc=" + std::to_string(result);
    const char* name = nullptr;
    const char* description = nullptr;
    if (state.cuGetErrorName != nullptr
        && state.cuGetErrorName(result, &name) == kCudaSuccess && name != nullptr)
    {
        text += " ";
        text += name;
    }
    if (state.cuGetErrorString != nullptr
        && state.cuGetErrorString(result, &description) == kCudaSuccess
        && description != nullptr)
    {
        text += " (";
        text += description;
        text += ")";
    }
    return text;
}

bool export_memory_handle(const vk::raii::Device& device, const vk::raii::DeviceMemory& memo,
    vk::DeviceSize alloc_bytes, CudaExternalMemoryHandleDesc& desc, int* result_out)
{
#ifdef _WIN32
    auto get_handle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        device.getProcAddr("vkGetMemoryWin32HandleKHR"));
    if (get_handle == nullptr) {
        if (result_out != nullptr) {
            *result_out = -1;
        }
        return false;
    }
    VkMemoryGetWin32HandleInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    info.memory = *memo;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    HANDLE handle = nullptr;
    const VkResult result = get_handle(static_cast<VkDevice>(*device), &info, &handle);
    if (result_out != nullptr) {
        *result_out = static_cast<int>(result);
    }
    if (result != VK_SUCCESS || handle == nullptr) {
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
        if (result_out != nullptr) {
            *result_out = -1;
        }
        return false;
    }
    VkMemoryGetFdInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    info.memory = *memo;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    int fd = -1;
    const VkResult result = get_fd(static_cast<VkDevice>(*device), &info, &fd);
    if (result_out != nullptr) {
        *result_out = static_cast<int>(result);
    }
    if (result != VK_SUCCESS || fd < 0) {
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

bool ensure_position_kernel(CudaInteropState& state) {
    if (!activate_cuda_context(state)) {
        std::cerr << "vkkk CUDA interop: failed to activate CUDA context for position kernel\n";
        return false;
    }
    if (state.cuModuleLoadDataEx == nullptr
        || state.cuModuleGetFunction == nullptr)
    {
        std::cerr << "vkkk CUDA interop: CUDA module loader symbols are unavailable\n";
        return false;
    }
    if (state.position_module != nullptr && state.position_kernel != nullptr) {
        return true;
    }

    CUmodule module = nullptr;
    char info_log[4096] = {};
    char error_log[4096] = {};
    unsigned int info_log_size = sizeof(info_log);
    unsigned int error_log_size = sizeof(error_log);
    unsigned int verbose = 1;
    CUjitOption jit_options[] = {
        kCudaJitInfoLogBuffer,
        kCudaJitInfoLogBufferSizeBytes,
        kCudaJitErrorLogBuffer,
        kCudaJitErrorLogBufferSizeBytes,
        kCudaJitLogVerbose,
    };
    void* jit_option_values[] = {
        info_log,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(info_log_size)),
        error_log,
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(error_log_size)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(verbose)),
    };
    const CUresult load_rc = state.cuModuleLoadDataEx(
        &module, kPositionScatterPtx,
        static_cast<unsigned int>(sizeof(jit_options) / sizeof(jit_options[0])),
        jit_options, jit_option_values);
    if (load_rc != kCudaSuccess || module == nullptr)
    {
        std::cerr << "vkkk CUDA interop: failed to load position PTX ("
            << cuda_result_text(state, load_rc) << ")\n";
        if (error_log[0] != '\0') {
            std::cerr << "vkkk CUDA interop: position PTX compiler error:\n"
                << error_log << '\n';
        }
        if (info_log[0] != '\0') {
            std::cerr << "vkkk CUDA interop: position PTX compiler info:\n"
                << info_log << '\n';
        }
        return false;
    }
    if (info_log[0] != '\0') {
        std::cerr << "vkkk CUDA interop: position PTX compiler info:\n"
            << info_log << '\n';
    }
    CUfunction function = nullptr;
    const CUresult function_rc =
        state.cuModuleGetFunction(&function, module, "orl_scatter_point_positions");
    if (function_rc != kCudaSuccess || function == nullptr)
    {
        std::cerr << "vkkk CUDA interop: position PTX entry point is unavailable ("
            << cuda_result_text(state, function_rc) << ")\n";
        state.cuModuleUnload(module);
        return false;
    }
    state.position_module = module;
    state.position_kernel = function;
    return true;
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

const MeshGPU* Context::find_draw_mesh(const std::string& name) const {
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

void Context::unmap_mesh_cuda(const std::string& name) {
    if (!cuda_interop) {
        return;
    }
    activate_cuda_context(*cuda_interop);
    auto unmap = [this](const std::string& key) {
        auto found = cuda_interop->maps.find(key);
        if (found == cuda_interop->maps.end()) {
            return;
        }
        if (found->second.external_memory != nullptr && cuda_interop->cuDestroyExternalMemory != nullptr) {
            cuda_interop->cuDestroyExternalMemory(found->second.external_memory);
        }
        cuda_interop->maps.erase(found);
    };
    unmap(name);
    unmap(name + ".draw");
    unmap(name + ".rest");
}

bool Context::map_buffer_to_cuda(const vk::raii::Buffer& buffer,
    const vk::raii::DeviceMemory& memory, vk::DeviceSize bytes,
    const std::string& map_key, CudaDeviceBuffer& view)
{
    view = {};
    if (!external_memory_export_available || *buffer == VK_NULL_HANDLE
        || *memory == VK_NULL_HANDLE || bytes == 0)
    {
        std::cerr << "vkkk CUDA interop: Vulkan buffer is not exportable"
            << " (export=" << external_memory_export_available
            << ", buffer=" << static_cast<VkBuffer>(*buffer)
            << ", memory=" << static_cast<VkDeviceMemory>(*memory)
            << ", bytes=" << bytes << ")\n";
        return false;
    }
    if (!cuda_interop) {
        cuda_interop.reset(new CudaInteropState());
    }
    if (!ensure_cuda_loaded(*cuda_interop)) {
        std::cerr << "vkkk CUDA interop: CUDA driver/context initialization failed\n";
        return false;
    }

    const uint64_t vk_buffer =
        reinterpret_cast<uint64_t>(static_cast<VkBuffer>(*buffer));
    auto& mapped = cuda_interop->maps[map_key];
    if (mapped.vk_buffer == vk_buffer && mapped.device_ptr != 0) {
        view.device_ptr = mapped.device_ptr;
        view.bytes = bytes;
        return true;
    }
    if (mapped.external_memory != nullptr) {
        cuda_interop->cuDestroyExternalMemory(mapped.external_memory);
        mapped = {};
    }

    const vk::MemoryRequirements mem_reqs = buffer.getMemoryRequirements();
    std::cerr << "vkkk CUDA interop: mapping '" << map_key
        << "' Vulkan buffer=" << static_cast<VkBuffer>(*buffer)
        << " memory=" << static_cast<VkDeviceMemory>(*memory)
        << " bytes=" << bytes
        << " allocation-bytes=" << mem_reqs.size << "\n";
    CudaExternalMemoryHandleDesc handle_desc{};
    int export_result = -1;
    if (!export_memory_handle(
            device, memory, mem_reqs.size, handle_desc, &export_result)) {
        std::cerr << "vkkk CUDA interop: Vulkan memory handle export failed for '"
            << map_key << "' (VkResult=" << export_result << ")\n";
        return false;
    }
    handle_desc.flags |= kCudaExternalMemoryDedicated;
    std::cerr << "vkkk CUDA interop: exported '" << map_key
        << "' handle-type=" << handle_desc.type
        << " flags=" << handle_desc.flags
        << " size=" << handle_desc.size << "\n";

    CUexternalMemory ext_mem = nullptr;
    const CUresult import_rc = cuda_interop->cuImportExternalMemory(&ext_mem, &handle_desc);
    close_exported_handle(handle_desc);
    if (import_rc != kCudaSuccess || ext_mem == nullptr) {
        std::cerr << "vkkk CUDA interop: CUDA external-memory import failed for '"
            << map_key << "' (" << cuda_result_text(*cuda_interop, import_rc) << ")\n";
        return false;
    }

    CudaExternalMemoryBufferDesc buffer_desc{};
    buffer_desc.size = mem_reqs.size;
    CUdeviceptr device_ptr = 0;
    const CUresult map_rc =
        cuda_interop->cuExternalMemoryGetMappedBuffer(&device_ptr, ext_mem, &buffer_desc);
    if (map_rc != kCudaSuccess || device_ptr == 0)
    {
        std::cerr << "vkkk CUDA interop: CUDA external-memory mapping failed for '"
            << map_key << "' (" << cuda_result_text(*cuda_interop, map_rc) << ")\n";
        cuda_interop->cuDestroyExternalMemory(ext_mem);
        return false;
    }

    mapped.vk_buffer = vk_buffer;
    mapped.external_memory = ext_mem;
    mapped.device_ptr = device_ptr;
    mapped.bytes = bytes;
    view.device_ptr = device_ptr;
    view.bytes = bytes;
    return true;
}

bool Context::map_mesh_vertices_to_cuda(const MeshGPU& mesh,
    const std::string& map_key, CudaDeviceBuffer& view)
{
    if (*mesh.vbuf == VK_NULL_HANDLE || *mesh.vbuf_memo == VK_NULL_HANDLE
        || mesh.vert_bytes == 0)
    {
        view = {};
        return false;
    }
    return map_buffer_to_cuda(
        mesh.vbuf, mesh.vbuf_memo, mesh.vert_bytes, map_key, view);
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

bool Context::write_pipeline_ssbo_from_cuda(
    const std::string& pipeline_name, const std::string& block_name,
    uint32_t frame_idx, uint64_t src_device_ptr, vk::DeviceSize bytes)
{
    if (src_device_ptr == 0 || bytes == 0) {
        return false;
    }
    const auto pipeline = pipelines.find(pipeline_name);
    if (pipeline == pipelines.end()) {
        return false;
    }
    const auto ssbo = pipeline->second.ssbos.find(block_name);
    if (ssbo == pipeline->second.ssbos.end()
        || ssbo->second.uses_borrowed_descriptors
        || frame_idx >= ssbo->second.gpu_bufs.size()
        || frame_idx >= ssbo->second.memos.size())
    {
        return false;
    }

    const auto& destination = ssbo->second;
    const vk::DeviceSize destination_bytes =
        static_cast<vk::DeviceSize>(destination.size)
        * destination.vecsize;
    if (bytes > destination_bytes) {
        return false;
    }

    // The selected per-frame allocation was fenced by begin_frame. CUDA is
    // synchronized below before the graphics command buffer is submitted, so
    // no device-wide wait or host readback is needed here.
    CudaDeviceBuffer mapped{};
    const std::string map_key = pipeline_name + ":" + block_name + ":"
        + std::to_string(frame_idx);
    if (!map_buffer_to_cuda(
            destination.gpu_bufs[frame_idx], destination.memos[frame_idx],
            destination_bytes, map_key, mapped)
        || !cuda_interop
        || cuda_interop->cuMemcpyDtoD(
               mapped.device_ptr, src_device_ptr,
               static_cast<size_t>(bytes)) != kCudaSuccess)
    {
        return false;
    }
    return cuda_interop->cuCtxSynchronize() == kCudaSuccess;
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
    wait_idle();
    CudaDeviceBuffer dest{};
    if (!mesh_cuda_vertex_ptr(name, dest) || bytes == 0 || bytes > dest.bytes) {
        return false;
    }
    if (!cuda_interop
        || cuda_interop->cuMemcpyDtoD(dest.device_ptr, src_device_ptr,
            static_cast<size_t>(bytes)) != kCudaSuccess)
    {
        return false;
    }
    return cuda_interop->cuCtxSynchronize() == kCudaSuccess;
}

bool Context::write_mesh_positions_from_cuda(const std::string& name, uint64_t src_device_ptr,
    vk::DeviceSize src_bytes, uint32_t vertex_count, uint32_t vertex_stride,
    uint32_t vertex_offset, const double* world_to_object)
{
    const MeshGPU* mesh = find_draw_mesh(name);
    if (mesh == nullptr || src_device_ptr == 0 || vertex_count == 0
        || vertex_count > mesh->vcnt || vertex_stride == 0
        || vertex_offset > vertex_stride || vertex_stride - vertex_offset < 3
        || world_to_object == nullptr)
    {
        std::cerr << "vkkk CUDA interop: invalid position-write arguments for '" << name
            << "'\n";
        return false;
    }

    const auto required_bytes =
        static_cast<vk::DeviceSize>(vertex_count) * vertex_stride * sizeof(float);
    const auto source_bytes =
        static_cast<vk::DeviceSize>(vertex_count) * sizeof(double) * 4;
    if (required_bytes > mesh->vert_bytes || src_bytes < source_bytes) {
        std::cerr << "vkkk CUDA interop: position-write buffer size mismatch for '" << name
            << "' (src=" << src_bytes << ", required-src=" << source_bytes
            << ", dst=" << mesh->vert_bytes << ", required-dst=" << required_bytes << ")\n";
        return false;
    }

    // The Vulkan queue may still read this shared vertex allocation from the
    // previous frame. The interop path has no external semaphore yet, so make
    // the ownership transition explicit before CUDA writes it.
    wait_idle();

    CudaDeviceBuffer destination{};
    if (!mesh_cuda_vertex_ptr(name, destination) || destination.device_ptr == 0
        || required_bytes > destination.bytes || !cuda_interop
        || !ensure_position_kernel(*cuda_interop))
    {
        std::cerr << "vkkk CUDA interop: failed to prepare draw buffer for '" << name
            << "' (mapped-bytes=" << destination.bytes
            << ", required-bytes=" << required_bytes << ")\n";
        return false;
    }

    uint64_t points = src_device_ptr;
    uint64_t vertices = destination.device_ptr;
    double matrix[12] = {
        world_to_object[0], world_to_object[4], world_to_object[8], world_to_object[12],
        world_to_object[1], world_to_object[5], world_to_object[9], world_to_object[13],
        world_to_object[2], world_to_object[6], world_to_object[10], world_to_object[14],
    };
    uint32_t count = vertex_count;
    uint32_t stride = vertex_stride;
    uint32_t offset = vertex_offset;
    void* parameters[] = {
        &points, &vertices,
        &matrix[0], &matrix[1], &matrix[2], &matrix[3],
        &matrix[4], &matrix[5], &matrix[6], &matrix[7],
        &matrix[8], &matrix[9], &matrix[10], &matrix[11],
        &count, &stride, &offset,
    };
    const unsigned int block_size = 128;
    const unsigned int block_count =
        (vertex_count + block_size - 1) / block_size;
    const CUresult launch_rc = cuda_interop->cuLaunchKernel(
            cuda_interop->position_kernel,
            block_count, 1, 1, block_size, 1, 1, 0, nullptr, parameters, nullptr);
    if (launch_rc != kCudaSuccess)
    {
        std::cerr << "vkkk CUDA interop: position conversion kernel launch failed for '"
            << name << "' (" << cuda_result_text(*cuda_interop, launch_rc) << ")\n";
        return false;
    }
    const CUresult sync_rc = cuda_interop->cuCtxSynchronize();
    if (sync_rc != kCudaSuccess) {
        std::cerr << "vkkk CUDA interop: position conversion synchronization failed for '"
            << name << "' (rc=" << sync_rc << ")\n";
        return false;
    }
    return true;
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
