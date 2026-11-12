#ifndef COMPUTE_GALLERY_COMMON_H
#define COMPUTE_GALLERY_COMMON_H

// C standard includes
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <inttypes.h>

// OpenCL includes
#include <CL/cl.h>

// SDL2 includes
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>

cl_float *fetch_random_data(size_t length, cl_float min, cl_float max);

float *array_copy(const float *src, size_t length);

/* Returns 1 if every pair of elements differs by less than `tolerance`, else reports the
 * first mismatch on stderr and returns 0. */
int assert_equals(const float *const v1, const float *const v2, size_t length, float tolerance);

cl_platform_id *load_platforms(cl_uint *num_platforms);

cl_device_id *load_devices(cl_platform_id platform, cl_uint *num_devices);

/*
 * Pick the device to run on: the first GPU found on any platform, falling back
 * to the first device of any kind (e.g. PoCL on the CPU inside a container).
 * Set CG_CL_PLATFORM and/or CG_CL_DEVICE to an index to choose explicitly.
 */
cl_device_id select_device(void);

double read_time_sec();

char *get_text_from_file(const char *path);

void print_image_details(const SDL_Surface *const surface);

void cleanup_sdl(SDL_Surface *surface, SDL_Window *win, SDL_Renderer *rend, SDL_Texture *tex);

int should_sdl_exit(SDL_Event *event);

#define CHECK_CL(expression)                                          \
    do                                                                \
    {                                                                 \
        const cl_int cl_error_ = (expression);                        \
        if (cl_error_ != CL_SUCCESS)                                  \
        {                                                             \
            fprintf(stderr, "%s failed with %d at %s:%d\n",           \
                    #expression, (int)cl_error_, __FILE__, __LINE__); \
            exit(EXIT_FAILURE);                                       \
        }                                                             \
    } while (0)

#define PRINT_CL_ERROR(errcode_ret_, enum_name_) \
    do                                           \
    {                                            \
        if (errcode_ret_ == enum_name_)          \
        {                                        \
            printf(#enum_name_ "\n");            \
        }                                        \
    } while (0);

#endif // COMPUTE_GALLERY_COMMON_H
