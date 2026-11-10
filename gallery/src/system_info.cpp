#include "system_info.hpp"

#include <glad/gl.h>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
using GetPlatformIDs = cl_int (CL_API_CALL*)(cl_uint, cl_platform_id*, cl_uint*);
using GetPlatformInfo = cl_int (CL_API_CALL*)(cl_platform_id, cl_platform_info, size_t, void*, size_t*);
using GetDeviceIDs = cl_int (CL_API_CALL*)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
using GetDeviceInfo = cl_int (CL_API_CALL*)(cl_device_id, cl_device_info, size_t, void*, size_t*);

void* openLibrary()
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryA("OpenCL.dll"));
#elif defined(__APPLE__)
    return dlopen("/System/Library/Frameworks/OpenCL.framework/OpenCL", RTLD_NOW);
#else
    void* lib = dlopen("libOpenCL.so.1", RTLD_NOW);
    return lib ? lib : dlopen("libOpenCL.so", RTLD_NOW);
#endif
}

void* symbol(void* lib, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
    return dlsym(lib, name);
#endif
}

template <typename Fn, typename Handle, typename Param>
std::string infoString(Fn fn, Handle h, Param param)
{
    char buffer[512] = {};
    if (fn(h, param, sizeof(buffer) - 1, buffer, nullptr) != CL_SUCCESS)
        return {};
    return buffer;
}
} // namespace

void queryOpenCL(SystemInfo& info)
{
    // The library stays loaded for the lifetime of the gallery; it is only a few calls.
    static void* lib = openLibrary();
    if (!lib)
    {
        info.cl_error = "No OpenCL driver found (install your GPU vendor's driver, or PoCL for CPUs)";
        return;
    }
    auto getPlatformIDs = reinterpret_cast<GetPlatformIDs>(symbol(lib, "clGetPlatformIDs"));
    auto getPlatformInfo = reinterpret_cast<GetPlatformInfo>(symbol(lib, "clGetPlatformInfo"));
    auto getDeviceIDs = reinterpret_cast<GetDeviceIDs>(symbol(lib, "clGetDeviceIDs"));
    auto getDeviceInfo = reinterpret_cast<GetDeviceInfo>(symbol(lib, "clGetDeviceInfo"));
    if (!getPlatformIDs || !getPlatformInfo || !getDeviceIDs || !getDeviceInfo)
    {
        info.cl_error = "The OpenCL library is incomplete";
        return;
    }

    cl_uint num_platforms = 0;
    if (getPlatformIDs(0, nullptr, &num_platforms) != CL_SUCCESS || num_platforms == 0)
    {
        info.cl_error = "OpenCL is installed but reports no platforms (no driver for this hardware)";
        return;
    }
    std::vector<cl_platform_id> platforms(num_platforms);
    getPlatformIDs(num_platforms, platforms.data(), nullptr);

    for (cl_uint p = 0; p < num_platforms; ++p)
    {
        const std::string platform_name = infoString(getPlatformInfo, platforms[p], CL_PLATFORM_NAME);
        cl_uint num_devices = 0;
        if (getDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, nullptr, &num_devices) != CL_SUCCESS)
            continue;
        std::vector<cl_device_id> devices(num_devices);
        getDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, num_devices, devices.data(), nullptr);

        for (cl_uint d = 0; d < num_devices; ++d)
        {
            cl_device_type type = 0;
            getDeviceInfo(devices[d], CL_DEVICE_TYPE, sizeof(type), &type, nullptr);
            info.cl_devices.push_back({
                .platform_index = p,
                .device_index = d,
                .platform = platform_name,
                .name = infoString(getDeviceInfo, devices[d], CL_DEVICE_NAME),
                .version = infoString(getDeviceInfo, devices[d], CL_DEVICE_VERSION),
                .gpu = (type & CL_DEVICE_TYPE_GPU) != 0,
            });
        }
    }
    if (info.cl_devices.empty())
        info.cl_error = "OpenCL reports no devices";
}

void queryOpenGL(SystemInfo& info)
{
    auto str = [](GLenum e) {
        const auto* s = reinterpret_cast<const char*>(glGetString(e));
        return std::string(s ? s : "unknown");
    };
    info.gl_renderer = str(GL_RENDERER);
    info.gl_version = str(GL_VERSION);
}
