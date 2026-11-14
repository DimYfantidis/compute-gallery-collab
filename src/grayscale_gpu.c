#include <stdio.h>
#include <stdlib.h>

#include <common.h>

void cpu_apply_grayscale_rgba32(Uint8* pixels, int width, int height, int pitch)
{
    for (int y = 0; y < height; ++y)
    {
        Uint8* row = pixels + y * pitch;

        for (int x = 0; x < width; ++x)
        {
            Uint8* pixel = row + x * 4;

            Uint8 r = pixel[0];
            Uint8 g = pixel[1];
            Uint8 b = pixel[2];

            // Approximation of luminance
            Uint8 gray = (Uint8)((299 * r + 587 * g + 114 * b) / 1000);
            pixel[0] = gray;
            pixel[1] = gray;
            pixel[2] = gray;
            // Preserve the original alpha channel
        }
    }
}

int main(int argc, char* argv[])
{
    // returns zero on success else non-zero
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG))
    {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Surface* surface = NULL;
    SDL_Window* win = NULL;
    SDL_Renderer* rend = NULL;
    SDL_Texture* tex = NULL;

    surface = IMG_Load(PROJECT_ROOT_PATH "/data/images/felix-mittermeier.jpg");
    {
        if (!surface)
        {
            fprintf(stderr, "IMG_Load failed: %s\n", IMG_GetError());
            cleanup_sdl(surface, win, rend, tex);
            return EXIT_FAILURE;
        }
        // Convert to a known pixel format.
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (!converted)
        {
            fprintf(stderr, "Conversion failed: %s\n", SDL_GetError());
            SDL_FreeSurface(surface);
            return EXIT_FAILURE;
        }
        SDL_FreeSurface(surface);
        surface = converted;
    }
    print_image_details(surface);

    // Setup OpenCL
    cl_device_id device = select_device();

    // Setup Device
    cl_int errcode;
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &errcode);
    CHECK_CL(errcode);
    cl_queue_properties queue_properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, queue_properties, &errcode);
    CHECK_CL(errcode);

    const char* source = "__kernel void grayscale(__global uchar *input, __global uchar *output) {\n"
                         "    int gid = get_global_id(0);\n"
                         "    uchar4 pixel = vload4(gid, input);\n"
                         "    uchar gray = convert_uchar_sat_rte(\n"
                         "        0.2126f * pixel.x +\n"
                         "        0.7152f * pixel.y +\n"
                         "        0.0722f * pixel.z);\n"
                         "    vstore4((uchar4)(gray, gray, gray, pixel.w), gid, output);\n"
                         "}\n";

    // Compile the program (i.e. the collection of kernels).
    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&source, NULL, &errcode);
    CHECK_CL(errcode);
    CHECK_CL(clBuildProgram(program, 0, NULL, NULL, NULL, NULL));
    cl_kernel kernel = clCreateKernel(program, "grayscale", &errcode);
    CHECK_CL(errcode);

    // Image bytes
    size_t len_bytes = (size_t)surface->pitch * (size_t)surface->h * sizeof(uint8_t);
    size_t len_pixels = (size_t)surface->w * (size_t)surface->h;

    // Create the memory object.
    cl_mem input_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, len_bytes, NULL, &errcode);
    CHECK_CL(errcode);
    cl_mem output_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, len_bytes, NULL, &errcode);
    CHECK_CL(errcode);

    size_t global_dimensions[] = {len_pixels, 0, 0};
    {
        cl_event event;

        // Copy the data to the input.
        CHECK_CL(clEnqueueWriteBuffer(queue, input_buffer, CL_FALSE, 0, len_bytes, surface->pixels, 0, NULL, &event));

        // Execute the kernel
        CHECK_CL(clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_buffer));
        CHECK_CL(clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_buffer));

        CHECK_CL(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_dimensions, NULL, 0, NULL, &event));
        CHECK_CL(clWaitForEvents(1, &event));

        cl_ulong time_start;
        cl_ulong time_end;
        CHECK_CL(clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL));
        CHECK_CL(clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL));

        // Read back the results.
        CHECK_CL(clEnqueueReadBuffer(queue, output_buffer, CL_FALSE, 0, len_bytes, surface->pixels, 0, NULL, &event));

        double elapsed_kernel = (time_end - time_start) * 1e-6;
        printf("> Kernel exution finshed after: %.4lf ms \n", elapsed_kernel);

        CHECK_CL(clReleaseEvent(event));
        event = NULL;
    }
    // Wait for everything to finish.
    CHECK_CL(clFinish(queue));

    clReleaseMemObject(output_buffer);
    clReleaseMemObject(input_buffer);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    win = SDL_CreateWindow("Spring JPEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, surface->w / 8,
                           surface->h / 6, SDL_WINDOW_SHOWN);

    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        cleanup_sdl(surface, win, rend, tex);
        return EXIT_FAILURE;
    }

    rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    if (!rend)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        cleanup_sdl(surface, win, rend, tex);
        return EXIT_FAILURE;
    }

    tex = SDL_CreateTextureFromSurface(rend, surface);

    if (!tex)
    {
        fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        cleanup_sdl(surface, win, rend, tex);
        return EXIT_FAILURE;
    }

    SDL_FreeSurface(surface);
    surface = NULL;

    // Clear the rendering target
    SDL_RenderClear(rend);

    // Draw the texture, scaled to the window
    SDL_RenderCopy(rend, tex, NULL, NULL);

    // Present the completed frame
    SDL_RenderPresent(rend);

    // animation loop
    while (1)
    {
        SDL_Event event;

        // Process all pending events
        if (!SDL_WaitEvent(&event))
        {
            fprintf(stderr, "`SDL_WaitEvent` error: %s\n", SDL_GetError());
            cleanup_sdl(surface, win, rend, tex);
            return EXIT_FAILURE;
        }

        if (should_sdl_exit(&event))
            break;
    }

    cleanup_sdl(surface, win, rend, tex);
    return EXIT_SUCCESS;
}