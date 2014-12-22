#include "katana.h"

#include "SDL.h"

#include <sys/mman.h>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

int quitting = 0;

typedef void (*game_update_and_render_fn_t)(game_memory_t *,
					    game_frame_buffer_t *);

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

	SDL_Renderer *renderer = SDL_CreateRenderer(
	    window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_Texture *texture = SDL_CreateTexture(renderer,
						 SDL_PIXELFORMAT_ABGR8888,
						 SDL_TEXTUREACCESS_STREAMING,
						 window_width,
						 window_height);

	void *game_object = SDL_LoadObject("katana.so");
	game_update_and_render_fn_t game_update_and_render_fn =
	    SDL_LoadFunction(game_object, "game_update_and_render");

	game_memory_t memory = {};
	memory.transient_store_size = Megabytes(512);
	memory.transient_store = mmap((void *)Terabytes(2),
				      memory.transient_store_size,
				      PROT_READ | PROT_WRITE,
				      MAP_ANON | MAP_PRIVATE,
				      -1,
				      0);

	memory.permanent_store_size = Megabytes(64);
	memory.permanent_store =
	    mmap((void *)(Terabytes(2) + memory.transient_store_size),
		 memory.permanent_store_size,
		 PROT_READ | PROT_WRITE,
		 MAP_ANON | MAP_PRIVATE,
		 -1,
		 0);

	game_frame_buffer_t frame_buffer = {};
	frame_buffer.width = window_width;
	frame_buffer.height = window_height;
	frame_buffer.pixels = mmap(0,
				   window_width * window_height * 4,
				   PROT_READ | PROT_WRITE,
				   MAP_ANON | MAP_PRIVATE,
				   -1,
				   0);

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

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		game_update_and_render_fn(&memory, &frame_buffer);

		if (SDL_UpdateTexture(texture,
				      0,
				      (void *)frame_buffer.pixels,
				      frame_buffer.width * 4)) {
			// TODO: Do something about this error!
		}

		SDL_RenderCopy(renderer, texture, 0, 0);

		SDL_RenderPresent(renderer);

		u64 current_time = SDL_GetPerformanceCounter();
		f32 frame_ms =
		    (1000.0f * (current_time - last_time)) / (f32)perf_freq;
		last_time = current_time;
		if (frame_counter++ % 60 == 0) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
				    "Frame time %.02f ms",
				    frame_ms);
		}
	}

	SDL_Quit();
	return 0;
}
