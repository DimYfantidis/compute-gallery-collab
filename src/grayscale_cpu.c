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

    {
        // image bytes = bytes per row * number of rows
        size_t len_bytes = (size_t)surface->pitch * (size_t)surface->h;
        Uint8* pixel_buffer = (Uint8*)malloc(len_bytes);
        memcpy(pixel_buffer, surface->pixels, len_bytes);

        double ts_grayscale_begin = read_time_sec();
        cpu_apply_grayscale_rgba32(pixel_buffer, surface->w, surface->h, surface->pitch);
        double ts_grayscale_end = read_time_sec();

        printf("Applied grayscale filter after: %lf ms\n", (ts_grayscale_end - ts_grayscale_begin) * 1000.0);
        memcpy(surface->pixels, pixel_buffer, len_bytes);
        free(pixel_buffer);
    }

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