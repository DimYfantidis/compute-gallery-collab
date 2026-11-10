#pragma once

#include <string>
#include <vector>

struct ClDevice
{
    unsigned platform_index;  // as seen by the demos' select_device() (CG_CL_PLATFORM)
    unsigned device_index;    // index within that platform (CG_CL_DEVICE)
    std::string platform;
    std::string name;
    std::string version;
    bool gpu;
};

struct SystemInfo
{
    std::string gl_renderer;
    std::string gl_version;
    std::vector<ClDevice> cl_devices;
    std::string cl_error;     // why OpenCL is unavailable, if cl_devices is empty
};

// Enumerates OpenCL devices by loading the OpenCL ICD loader at runtime, so the gallery
// still starts (with OpenCL demos disabled) on machines that have no OpenCL driver.
void queryOpenCL(SystemInfo& info);

// Reads the renderer of the gallery's own OpenGL context; call with a current context.
void queryOpenGL(SystemInfo& info);
