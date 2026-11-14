#include <common.h>

cl_float* fetch_random_data(size_t length, cl_float min, cl_float max)
{
    // Initialize our data
    cl_float* data = (cl_float*)malloc(sizeof(cl_float) * length);
    cl_float scale = max - min;
    for (size_t i = 0; i < length; ++i)
    {
        float x = (float)rand() / RAND_MAX;
        data[i] = x * scale + min;
    }
    return data;
}

float* array_copy(const float* src, size_t length)
{
    float* dest = (float*)malloc(sizeof(float) * length);
    for (size_t i = 0; i < length; ++i)
    {
        dest[i] = src[i];
    }
    return dest;
}

void assert_equals(const float* const v1, const float* const v2, size_t length, float tolerance)
{
    for (size_t i = 0; i < length; ++i)
    {
        float diff = fabsf(v1[i] - v2[i]);
        if (diff >= tolerance)
        {
            fprintf(stderr, "Assertion for equal elements failed at #%zu!\n", i);
            return;
        }
    }
}

cl_platform_id* load_platforms(cl_uint* num_platforms)
{
    CHECK_CL(clGetPlatformIDs(0, NULL, num_platforms));
    printf("%u platform(s) found\n", *num_platforms);

    cl_platform_id* platforms = (cl_platform_id*)malloc(*num_platforms * sizeof(cl_platform_id));
    CHECK_CL(clGetPlatformIDs(*num_platforms, platforms, NULL));

    for (size_t i = 0; i < *num_platforms; ++i)
    {
        char buffer[512];
        CHECK_CL(clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(buffer), buffer, NULL));
        printf("-- %zu: Named:      %s\n", i, buffer);
        CHECK_CL(clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, sizeof(buffer), buffer, NULL));
        printf("      Versioned:  %s\n", buffer);
        CHECK_CL(clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(buffer), buffer, NULL));
        printf("      Authored:   %s\n", buffer);
    }
    printf("\n");
    return platforms;
}

cl_device_id* load_devices(cl_platform_id platform, cl_uint* num_devices)
{
    char platform_name[256];
    CHECK_CL(clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL));

    CHECK_CL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, num_devices));
    printf("%u device(s) found for \"%s\"\n", *num_devices, platform_name);

    cl_device_id* devices = (cl_device_id*)malloc(*num_devices * sizeof(cl_device_id));
    CHECK_CL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, *num_devices, devices, NULL));

    for (size_t i = 0; i < *num_devices; ++i)
    {
        char buffer[512];
        cl_device_type type;

        CHECK_CL(clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL));
        printf("-- %zu: Named:    %s\n", i, buffer);
        CHECK_CL(clGetDeviceInfo(devices[i], CL_DEVICE_TYPE, sizeof(type), &type, NULL));
        printf("      Type:     ");
        if (type & CL_DEVICE_TYPE_CPU)
            printf("CPU ");
        if (type & CL_DEVICE_TYPE_GPU)
            printf("GPU ");
        if (type & CL_DEVICE_TYPE_ACCELERATOR)
            printf("ACCELERATOR ");
        if (type & CL_DEVICE_TYPE_DEFAULT)
            printf("DEFAULT ");
        printf("\n");
        CHECK_CL(clGetDeviceInfo(devices[i], CL_DEVICE_PROFILE, sizeof(buffer), buffer, NULL));
        printf("      Profile:  %s\n", buffer);
    }
    printf("\n");
    return devices;
}

static long env_index(const char* name)
{
    const char* value = getenv(name);
    if (!value || !*value)
        return -1;
    return strtol(value, NULL, 10);
}

cl_device_id select_device(void)
{
    cl_uint num_platforms;
    cl_platform_id* platforms = load_platforms(&num_platforms);

    const long want_platform = env_index("CG_CL_PLATFORM");
    const long want_device = env_index("CG_CL_DEVICE");

    if (want_platform >= (long)num_platforms)
    {
        fprintf(stderr, "CG_CL_PLATFORM=%ld but only %u platform(s) exist\n", want_platform, num_platforms);
        exit(EXIT_FAILURE);
    }

    cl_device_id chosen = NULL;
    cl_device_id fallback = NULL;

    for (cl_uint p = 0; p < num_platforms && !chosen; ++p)
    {
        if (want_platform >= 0 && p != (cl_uint)want_platform)
            continue;

        cl_uint num_devices = 0;
        cl_device_id* devices = load_devices(platforms[p], &num_devices);

        if (want_device >= 0)
        {
            if (want_device >= (long)num_devices)
            {
                fprintf(stderr, "CG_CL_DEVICE=%ld but platform %u has only %u device(s)\n", want_device, p,
                        num_devices);
                exit(EXIT_FAILURE);
            }
            chosen = devices[want_device];
        }
        for (cl_uint d = 0; d < num_devices && !chosen; ++d)
        {
            cl_device_type type;
            CHECK_CL(clGetDeviceInfo(devices[d], CL_DEVICE_TYPE, sizeof(type), &type, NULL));
            if (type & CL_DEVICE_TYPE_GPU)
                chosen = devices[d];
            else if (!fallback)
                fallback = devices[d];
        }
        free(devices);
    }
    free(platforms);

    if (!chosen)
        chosen = fallback;
    if (!chosen)
    {
        fprintf(stderr, "No OpenCL device found\n");
        exit(EXIT_FAILURE);
    }

    char name[512];
    CHECK_CL(clGetDeviceInfo(chosen, CL_DEVICE_NAME, sizeof(name), name, NULL));
    printf("Running on: %s\n\n", name);
    return chosen;
}

double read_time_sec()
{
    // Monotonic high-resolution clock that also exists on Windows, unlike clock_gettime.
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

char* get_text_from_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        return NULL;
    }

    rewind(f);

    char* buffer = malloc((size_t)size + 1);
    if (!buffer)
    {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (bytes_read != (size_t)size)
    {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

void print_image_details(const SDL_Surface* const surface)
{
    if (!surface)
    {
        fprintf(stderr, "print_image_details: NULL surface\n");
        return;
    }

    const SDL_PixelFormat* fmt = surface->format;

    if (!fmt)
    {
        fprintf(stderr, "print_image_details: NULL pixel format\n");
        return;
    }

    size_t row_bytes = (size_t)surface->w * fmt->BytesPerPixel;
    size_t buffer_size = (size_t)surface->pitch * surface->h;

    printf("========== SDL Surface Details ==========\n");

    printf("Dimensions:\n");
    printf("  Width:             %d px\n", surface->w);
    printf("  Height:            %d px\n", surface->h);

    printf("\nPixel Format:\n");
    printf("  Format:            %s\n", SDL_GetPixelFormatName(fmt->format));
    printf("  Bits per pixel:    %u\n", fmt->BitsPerPixel);
    printf("  Bytes per pixel:   %u\n", fmt->BytesPerPixel);

    printf("\nMemory Layout:\n");
    printf("  Pitch:             %d bytes\n", surface->pitch);
    printf("  Row pixel bytes:   %zu bytes\n", row_bytes);
    printf("  Row padding:       %lld bytes\n", (long long)surface->pitch - (long long)row_bytes);
    printf("  Buffer size:       %zu bytes\n", buffer_size);
    printf("  Buffer size:       %.2f MiB\n", buffer_size / (1024.0 * 1024.0));
    printf("  Pixels pointer:    %p\n", surface->pixels);

    printf("\nChannel Masks:\n");
    printf("  Red mask:          0x%08X\n", fmt->Rmask);
    printf("  Green mask:        0x%08X\n", fmt->Gmask);
    printf("  Blue mask:         0x%08X\n", fmt->Bmask);
    printf("  Alpha mask:        0x%08X\n", fmt->Amask);

    printf("\nSurface Properties:\n");
    printf("  Flags:             0x%08X\n", surface->flags);
    printf("  Must lock:         %s\n", SDL_MUSTLOCK(surface) ? "Yes" : "No");
    printf("  Reference count:   %d\n", surface->refcount);

    printf("\nClipping Rectangle:\n");
    printf("  X:                 %d\n", surface->clip_rect.x);
    printf("  Y:                 %d\n", surface->clip_rect.y);
    printf("  Width:             %d\n", surface->clip_rect.w);
    printf("  Height:            %d\n", surface->clip_rect.h);

    if (fmt->palette)
    {
        printf("\nPalette:\n");
        printf("  Colors:            %d\n", fmt->palette->ncolors);
    }

    printf("=========================================\n");
}

void cleanup_sdl(SDL_Surface* surface, SDL_Window* win, SDL_Renderer* rend, SDL_Texture* tex)
{
    if (surface)
        SDL_FreeSurface(surface);

    if (tex)
        SDL_DestroyTexture(tex);

    if (rend)
        SDL_DestroyRenderer(rend);

    if (win)
        SDL_DestroyWindow(win);

    IMG_Quit();
    SDL_Quit();
}

int should_sdl_exit(SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_QUIT:
        return 1;
    case SDL_KEYDOWN:
        if (event->key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}