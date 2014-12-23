#include "katana.h"

#include "SDL.h"

#include <copyfile.h>
#include <time.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

int quitting = 0;

typedef void (*game_update_and_render_fn_t)(game_memory_t *, game_frame_buffer_t *);

typedef struct {
        char *so;
        char *temp_so;
} osx_game_so_paths_t;

typedef struct {
        const char *so_path;
        void *so;
        time_t so_last_modified;
        game_update_and_render_fn_t update_and_render_fn;
} osx_game_t;

osx_game_so_paths_t osx_get_game_so_paths()
{
        char symbolic_exe_path[1024] = {0};
        u32 size = sizeof(symbolic_exe_path);
        if (_NSGetExecutablePath(symbolic_exe_path, &size) == 0) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Symbolic exe path is:  %s", symbolic_exe_path);
                // TODO(Wes) : Handle failure to get exe path.
        }

        char real_exe_path[1024] = {0};
        realpath(symbolic_exe_path, real_exe_path);

        u32 last_slash = 0;
        for (u32 i = 0; i < sizeof(real_exe_path); ++i) {
                if (real_exe_path[i] == '/') {
                        last_slash = i;
                }
        }

        static char so_path[1024] = {0};
        strncpy(so_path, real_exe_path, last_slash + 1);
        strcat(so_path, "katana.so");

        static char temp_so_path[1024] = {0};
        strncpy(temp_so_path, real_exe_path, last_slash + 1);
        strcat(temp_so_path, "katana_temp.so");

        osx_game_so_paths_t game_so_paths;
        game_so_paths.so = so_path;
        game_so_paths.temp_so = temp_so_path;
        return game_so_paths;
}

void osx_load_game(osx_game_t *game, const char *game_so_path)
{
        game->so = dlopen(game_so_path, 0);
        game->update_and_render_fn = dlsym(game->so, "game_update_and_render");
}

time_t osx_get_last_modified(char *path)
{
        struct stat attr;
        stat(path, &attr);
        return attr.st_mtimespec.tv_sec;
}

int main(void)
{
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
                // TODO(Wes): SDL_Init didn't work!
        }

        u32 window_width = 1280;
        u32 window_height = 720;
        SDL_Window *window = SDL_CreateWindow("Handmade Hero",
                                              SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              window_width,
                                              window_height,
                                              SDL_WINDOW_RESIZABLE);

        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        SDL_Texture *texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);

        osx_game_so_paths_t game_so_paths = osx_get_game_so_paths();
        osx_game_t game = {};

        game_memory_t memory = {};
        memory.transient_store_size = Megabytes(512);
        memory.transient_store = mmap(
            (void *)Terabytes(2), memory.transient_store_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

        memory.permanent_store_size = Megabytes(64);
        memory.permanent_store = mmap((void *)(Terabytes(2) + memory.transient_store_size),
                                      memory.permanent_store_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_ANON | MAP_PRIVATE,
                                      -1,
                                      0);

        game_frame_buffer_t frame_buffer = {};
        frame_buffer.width = window_width;
        frame_buffer.height = window_height;
        frame_buffer.pixels =
            mmap(0, window_width * window_height * 4, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

        u64 last_time = 0;
        u64 perf_freq = SDL_GetPerformanceFrequency();
        i32 frame_counter = 0;
        SDL_Event event;
        while (!quitting) {
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_QUIT:
                                quitting = 1;
                                break;
                        }
                }

                // TODO(Wes): Debug builds only.
                time_t last_modified = osx_get_last_modified(game_so_paths.so);
                if (difftime(last_modified, game.so_last_modified) != 0.0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "katana.so modified - Reloading");
                        dlclose(game.so);
                        copyfile(game_so_paths.so, game_so_paths.temp_so, 0, COPYFILE_ALL);
                        osx_load_game(&game, game_so_paths.temp_so);
                        game.so_last_modified = osx_get_last_modified(game_so_paths.so);
                }

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);

                game.update_and_render_fn(&memory, &frame_buffer);

                if (SDL_UpdateTexture(texture, 0, (void *)frame_buffer.pixels, frame_buffer.width * 4)) {
                        // TODO: Do something about this error!
                }

                SDL_RenderCopy(renderer, texture, 0, 0);

                SDL_RenderPresent(renderer);

                u64 current_time = SDL_GetPerformanceCounter();
                f32 frame_ms = (1000.0f * (current_time - last_time)) / (f32)perf_freq;
                last_time = current_time;
                if (frame_counter++ % 60 == 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame time %.02f ms", frame_ms);
                }
        }

        SDL_Quit();
        return 0;
}
