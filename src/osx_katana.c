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

SDL_GameController *controller_handles[KATANA_MAX_CONTROLLERS];
SDL_Haptic *haptic_handles[KATANA_MAX_CONTROLLERS];

typedef void (*game_update_and_render_fn_t)(game_memory_t *, game_frame_buffer_t *, game_audio_t *, game_input_t *,
                                            game_output_t *);

typedef struct {
        char *so;
        char *temp_so;
} osx_game_so_paths_t;

typedef struct {
        void *so;
        time_t so_last_modified;
        game_update_and_render_fn_t update_and_render_fn;
} osx_game_t;

#define KATANA_MAX_RECORDED_INPUT_EVENTS 65536
typedef struct {
        game_input_t *input_events;
        u32 input_record_index;
        u32 input_playback_index;
        game_memory_t memory;
} osx_game_record_t;

static osx_game_so_paths_t osx_get_game_so_paths()
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

static void osx_load_game(osx_game_t *game, const char *game_so_path)
{
        game->so = dlopen(game_so_path, 0);
        game->update_and_render_fn = dlsym(game->so, "game_update_and_render");
}

static time_t osx_get_last_modified(char *path)
{
        struct stat attr;
        stat(path, &attr);
        return attr.st_mtimespec.tv_sec;
}

static void osx_process_controller_button(game_button_state_t *old_state, game_button_state_t *new_state,
                                          SDL_GameController *controller_handle, SDL_GameControllerButton button)
{
        new_state->ended_down = SDL_GameControllerGetButton(controller_handle, button);
        new_state->half_transition_count += ((new_state->ended_down == old_state->ended_down) ? 0 : 1);
}

static f32 osx_process_controller_axis(i16 stick_value, i16 dead_zone_threshold)
{
        f32 result = 0;

        if (stick_value < -dead_zone_threshold) {
                result = (f32)((stick_value + dead_zone_threshold) / (32768.0f - dead_zone_threshold));
        } else if (stick_value > dead_zone_threshold) {
                result = (f32)((stick_value - dead_zone_threshold) / (32767.0f - dead_zone_threshold));
        }

        return result;
}

static void osx_handle_input(game_input_t *new_input, game_input_t *old_input)
{
        u32 stick_values_index = 0;
        for (int i = 0; i < KATANA_MAX_CONTROLLERS; ++i) {
                if (!SDL_GameControllerGetAttached(controller_handles[i])) {
                        continue;
                }

                SDL_GameController *handle = controller_handles[i];
                game_controller_input_t *old_controller = &old_input->controllers[i];
                game_controller_input_t *new_controller = &new_input->controllers[i];

                osx_process_controller_button(&(old_controller->move_up), &(new_controller->move_up), handle,
                                              SDL_CONTROLLER_BUTTON_DPAD_UP);
                osx_process_controller_button(&(old_controller->move_down), &(new_controller->move_down), handle,
                                              SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                osx_process_controller_button(&(old_controller->move_left), &(new_controller->move_left), handle,
                                              SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                osx_process_controller_button(&(old_controller->move_right), &(new_controller->move_right), handle,
                                              SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                osx_process_controller_button(&(old_controller->start), &(new_controller->start), handle,
                                              SDL_CONTROLLER_BUTTON_START);
                osx_process_controller_button(&(old_controller->back), &(new_controller->back), handle,
                                              SDL_CONTROLLER_BUTTON_BACK);
                osx_process_controller_button(&(old_controller->left_shoulder), &(new_controller->left_shoulder),
                                              handle, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                osx_process_controller_button(&(old_controller->right_shoulder), &(new_controller->right_shoulder),
                                              handle, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                osx_process_controller_button(&(old_controller->action_down), &(new_controller->action_down), handle,
                                              SDL_CONTROLLER_BUTTON_A);
                osx_process_controller_button(&(old_controller->action_up), &(new_controller->action_up), handle,
                                              SDL_CONTROLLER_BUTTON_Y);
                osx_process_controller_button(&(old_controller->action_left), &(new_controller->action_left), handle,
                                              SDL_CONTROLLER_BUTTON_X);
                osx_process_controller_button(&(old_controller->action_right), &(new_controller->action_right), handle,
                                              SDL_CONTROLLER_BUTTON_B);

                if (stick_values_index < KATANA_MAX_STICK_VALUES) {
                        i16 stick_x = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTX);
                        i16 stick_y = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTY);
                        // TODO(Wes): Tune dead zones
                        const i16 dead_zone = 4096;
                        new_controller->stick_x[stick_values_index] = osx_process_controller_axis(stick_x, dead_zone);
                        new_controller->stick_y[stick_values_index] = osx_process_controller_axis(stick_y, dead_zone);
                        stick_values_index++;
                }
                new_controller->stick_value_count = stick_values_index + 1;
        }
}

static game_memory_t osx_allocate_game_memory(void *base_address)
{
        game_memory_t memory = {};
        memory.transient_store_size = Megabytes(512);
        memory.transient_store =
            mmap(base_address, memory.transient_store_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

        memory.permanent_store_size = Megabytes(64);
        memory.permanent_store = mmap((void *)(base_address + memory.transient_store_size), memory.permanent_store_size,
                                      PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        return memory;
}

static void osx_copy_game_memory(game_memory_t *dst, game_memory_t *src)
{
        memcpy(dst->transient_store, src->transient_store, src->transient_store_size);
        memcpy(dst->permanent_store, src->permanent_store, src->permanent_store_size);
}

int main(void)
{
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
                // TODO(Wes): SDL_Init didn't work!
        }

        u32 window_width = 1280;
        u32 window_height = 720;
        SDL_Window *window = SDL_CreateWindow("Handmade Hero", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                              window_width, window_height, SDL_WINDOW_RESIZABLE);

        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                                 window_width, window_height);

        SDL_AudioSpec audio_spec_want = {};
        audio_spec_want.freq = 48000;
        audio_spec_want.format = AUDIO_S16LSB;
        audio_spec_want.channels = 2;
        audio_spec_want.samples = (audio_spec_want.freq / 60) * audio_spec_want.channels;

        SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(0, 0, &audio_spec_want, 0, 0);
        if (audio_device == 0) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device: %s", SDL_GetError());
                return -1;
        }

        game_memory_t game_memory = osx_allocate_game_memory((void *)Terabytes(2));

        osx_game_so_paths_t game_so_paths = osx_get_game_so_paths();
        osx_game_t game = {};

        game_frame_buffer_t frame_buffer = {};
        frame_buffer.width = window_width;
        frame_buffer.height = window_height;
        frame_buffer.pixels =
            mmap(0, window_width * window_height * 4, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

        i32 max_joysticks = SDL_NumJoysticks();
        i32 controller_index = 0;

        for (i32 i = 0; i < max_joysticks; ++i) {
                if (!SDL_IsGameController(i)) {
                        continue;
                }
                if (i >= KATANA_MAX_CONTROLLERS) {
                        break;
                }
                SDL_GameController *controller = SDL_GameControllerOpen(i);
                controller_handles[controller_index] = controller;
                SDL_Joystick *joystick_handle = SDL_GameControllerGetJoystick(controller);
                SDL_Haptic *haptic_handle = SDL_HapticOpenFromJoystick(joystick_handle);
                if (haptic_handle) {
                        haptic_handles[controller_index] = haptic_handle;
                } else {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize haptics %s", SDL_GetError());
                }
                controller_index++;

                if (SDL_HapticRumbleInit(haptic_handles[controller_index]) != 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize rumble %s", SDL_GetError());
                        // Failed to init rumble so just turn it off.
                        SDL_HapticClose(haptic_handles[controller_index]);
                        haptic_handles[controller_index] = 0;
                }
        }

        game_input_t input[2] = {};
        game_input_t *new_input = &input[0];
        game_input_t *old_input = &input[1];

        game_output_t output = {};

        game_audio_t audio;
        audio.samples_per_second = audio_spec_want.freq;
        audio.sample_count = audio_spec_want.samples;

        osx_game_record_t game_record = {};
        game_record.input_events = mmap(0, sizeof(game_input_t) * KATANA_MAX_RECORDED_INPUT_EVENTS,
                                        PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        game_record.memory = osx_allocate_game_memory((void *)Terabytes(4));

        u64 last_time = 0;
        u64 perf_freq = SDL_GetPerformanceFrequency();
        i32 frame_counter = 0;
        SDL_Event event;
        b8 recording = false;
        b8 playing_back = false;
        b8 quitting = 0;
        SDL_PauseAudioDevice(audio_device, 0);
        while (!quitting) {
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_QUIT:
                                quitting = 1;
                                break;
                        case SDL_KEYDOWN:
                                if (event.key.keysym.sym == SDLK_r) {
                                        if (recording) {
                                                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Recorded %d input events",
                                                            game_record.input_record_index);
                                                recording = 0;
                                        } else {
                                                osx_copy_game_memory(&game_record.memory, &game_memory);
                                                game_record.input_record_index = 0;
                                                recording = 1;
                                        }
                                }
                                if (event.key.keysym.sym == SDLK_p) {
                                        game_record.input_playback_index = 0;
                                        if (playing_back) {
                                                playing_back = 0;
                                        } else {
                                                playing_back = 1;
                                        }
                                }
                                if (event.key.keysym.sym == SDLK_ESCAPE) {
                                        quitting = 1;
                                }
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

                osx_handle_input(new_input, old_input);

                if (recording) {
                        game_record.input_events[game_record.input_record_index++] = *new_input;
                } else if (playing_back) {
                        u32 playback_index = game_record.input_playback_index++ % (game_record.input_record_index + 1);
                        if (playback_index == 0) {
                                osx_copy_game_memory(&game_memory, &game_record.memory);
                        }
                        *new_input = game_record.input_events[playback_index];
                }

                // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                // SDL_RenderClear(renderer);

                game.update_and_render_fn(&game_memory, &frame_buffer, &audio, new_input, &output);

                SDL_QueueAudio(audio_device, (void *)audio.samples, audio.sample_count * sizeof(i16));
                u32 queued_audio_size = SDL_GetQueuedAudioSize(audio_device);

                for (u32 i = 0; i < KATANA_MAX_CONTROLLERS; ++i) {
                        if (!haptic_handles[i]) {
                                continue;
                        }
                        if (output.controllers[i].rumble) {
                                SDL_HapticRumblePlay(haptic_handles[i], output.controllers[i].intensity,
                                                     SDL_HAPTIC_INFINITY);
                        } else {
                                SDL_HapticRumbleStop(haptic_handles[i]);
                        }
                }

                if (SDL_UpdateTexture(texture, 0, (void *)frame_buffer.pixels, frame_buffer.width * 4)) {
                        // TODO: Do something about this error!
                }

                SDL_RenderCopy(renderer, texture, 0, 0);
                SDL_RenderPresent(renderer);

                game_input_t *temp = new_input;
                new_input = old_input;
                old_input = temp;

                u64 current_time = SDL_GetPerformanceCounter();
                f32 frame_ms = (1000.0f * (current_time - last_time)) / (f32)perf_freq;
                last_time = current_time;
                if (frame_counter++ % (60 * 5) == 0) {
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame time %.02f ms", frame_ms);
                        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Queued audio bytes %d", queued_audio_size);
                }
        }

        SDL_Quit();
        return 0;
}
