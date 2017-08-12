#include "gg_platform.h"

#include "SDL.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#ifdef _WIN32
#include "gg_platform_windows.c"
#elif
#include "gg_platform_osx.c"
#endif

SDL_GameController *controller_handles[GG_MAX_CONTROLLERS];
SDL_Haptic *haptic_handles[GG_MAX_CONTROLLERS];

typedef void (*game_update_and_render_fn_t)(game_memory_t *,
                                            game_frame_buffer_t *,
                                            game_audio_t *,
                                            game_input_t *,
                                            game_output_t *,
                                            game_callbacks_t *,
                                            game_work_queues_t *);

typedef struct {
    char *game_lib;
    char *temp_game_lib;
} game_lib_paths_t;

typedef struct {
    void *lib;
    time_t last_modified;
    game_update_and_render_fn_t update_and_render_fn;
} game_t;

#define GG_MAX_RECORDED_INPUT_EVENTS 6000 // 100 seconds of input at 60fps
typedef struct {
    game_input_t *input_events;
    u32 input_record_index;
    u32 input_playback_index;
    game_memory_t memory;
} game_record_t;

static game_lib_paths_t get_game_lib_paths()
{
    // We own this pointer.
    char *base_path = SDL_GetBasePath();
    if (base_path)
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Base path is:  %s",
                    base_path);
    }
    else
    {
        // TODO(Wes) : Handle failure to get exe path.
    }

    size_t base_path_len = strnlen(base_path, 1024);
    static char lib_path[1024] = {0};
    strncpy(lib_path, base_path, base_path_len + 1);
    strcat(lib_path, "gg_game.dll");

    static char temp_lib_path[1024] = {0};
    strncpy(temp_lib_path, base_path, base_path_len + 1);
    strcat(temp_lib_path, "gg_game_temp.dll");

    game_lib_paths_t game_lib_paths;
    game_lib_paths.game_lib = lib_path;
    game_lib_paths.temp_game_lib = temp_lib_path;
    return game_lib_paths;
}

static void load_game(game_t *game, const char *game_so_path)
{
    game->lib = SDL_LoadObject(game_so_path);
    game->update_and_render_fn = (game_update_and_render_fn_t)SDL_LoadFunction(game->lib, "game_update_and_render");
}

static void init_kb(game_input_t *input, u32 controller_index)
{
    game_controller_input_t *controller = &input->controllers[controller_index];
    controller->is_analog = 0;
    controller->is_connected = 1;
}

static void init_controller(game_input_t *input, u32 controller_index)
{
    game_controller_input_t *controller = &input->controllers[controller_index];
    controller->is_analog = 1;
    controller->is_connected = 1;
}

static void process_controller_button(game_button_state_t *old_state,
                                      game_button_state_t *new_state,
                                      SDL_GameController *controller_handle,
                                      SDL_GameControllerButton button)
{
    new_state->ended_down =
        SDL_GameControllerGetButton(controller_handle, button);
    new_state->half_transition_count +=
        ((new_state->ended_down == old_state->ended_down) ? 0 : 1);
}

static void process_keydown(game_button_state_t *old_state,
                            game_button_state_t *new_state)
{
    new_state->ended_down = 1;
    old_state->ended_down = 1;
    new_state->half_transition_count +=
        ((new_state->ended_down == old_state->ended_down) ? 0 : 1);
}

static void process_keyup(game_button_state_t *old_state,
                          game_button_state_t *new_state)
{
    new_state->ended_down = 0;
    old_state->ended_down = 0;
    new_state->half_transition_count +=
        ((new_state->ended_down == old_state->ended_down) ? 0 : 1);
}

static f32 process_controller_axis(i16 stick_value, i16 dead_zone_threshold)
{
    f32 result = 0;

    if (stick_value < -dead_zone_threshold) {
        result = (f32)((stick_value + dead_zone_threshold) /
                       (32768.0f - dead_zone_threshold));
    } else if (stick_value > dead_zone_threshold) {
        result = (f32)((stick_value - dead_zone_threshold) /
                       (32767.0f - dead_zone_threshold));
    }

    return result;
}

static void handle_controller_input(game_input_t *new_input,
                                        game_input_t *old_input)
{
    for (int i = 0; i < GG_MAX_CONTROLLERS; ++i) {
        if (!SDL_GameControllerGetAttached(controller_handles[i])) {
            continue;
        }

        SDL_GameController *handle = controller_handles[i];
        game_controller_input_t *old_controller = &old_input->controllers[i];
        game_controller_input_t *new_controller = &new_input->controllers[i];

        process_controller_button(&(old_controller->move_up),
                                  &(new_controller->move_up),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_DPAD_UP);
        process_controller_button(&(old_controller->move_down),
                                  &(new_controller->move_down),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        process_controller_button(&(old_controller->move_left),
                                  &(new_controller->move_left),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        process_controller_button(&(old_controller->move_right),
                                  &(new_controller->move_right),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        process_controller_button(&(old_controller->start),
                                  &(new_controller->start),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_START);
        process_controller_button(&(old_controller->back),
                                  &(new_controller->back),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_BACK);
        process_controller_button(&(old_controller->left_shoulder),
                                  &(new_controller->left_shoulder),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        process_controller_button(&(old_controller->right_shoulder),
                                  &(new_controller->right_shoulder),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        process_controller_button(&(old_controller->action_down),
                                  &(new_controller->action_down),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_A);
        process_controller_button(&(old_controller->action_up),
                                  &(new_controller->action_up),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_Y);
        process_controller_button(&(old_controller->action_left),
                                  &(new_controller->action_left),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_X);
        process_controller_button(&(old_controller->action_right),
                                  &(new_controller->action_right),
                                  handle,
                                  SDL_CONTROLLER_BUTTON_B);

        i16 left_stick_x =
            SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTX);
        i16 left_stick_y =
            SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_LEFTY);
        i16 right_stick_x =
            SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_RIGHTX);
        i16 right_stick_y =
            SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_RIGHTY);
        // TODO(Wes): Tune dead zones
        const i16 dead_zone = 4096;
        new_controller->left_stick_x =
            process_controller_axis(left_stick_x, dead_zone);
        new_controller->left_stick_y =
            process_controller_axis(left_stick_y, dead_zone);
        new_controller->right_stick_x =
            process_controller_axis(right_stick_x, dead_zone);
        new_controller->right_stick_y =
            process_controller_axis(right_stick_y, dead_zone);
    }
}

static void handle_keyup(SDL_Keycode key,
                         game_controller_input_t *old_input,
                         game_controller_input_t *new_input)
{
    switch (key) {
    case SDLK_w:
    case SDLK_UP:
        process_keyup(&(old_input->move_up), &(new_input->move_up));
        break;
    case SDLK_s:
    case SDLK_DOWN:
        process_keyup(&(old_input->move_down), &(new_input->move_down));
        break;
    case SDLK_a:
    case SDLK_LEFT:
        process_keyup(&(old_input->move_left), &(new_input->move_left));
        break;
    case SDLK_d:
    case SDLK_RIGHT:
        process_keyup(&(old_input->move_right), &(new_input->move_right));
        break;
    case SDLK_RETURN:
        process_keyup(&(old_input->start), &(new_input->start));
        break;
    case SDLK_SPACE:
        process_keyup(&(old_input->action_down), &(new_input->action_down));
        break;
#ifdef GG_EDITOR
    case SDLK_e:
        osx_process_keyup(&(old_input->editor_mode), &(new_input->editor_mode));
        break;
#endif
    }
}

static void handle_keydown(SDL_Keycode key,
                           game_controller_input_t *old_input,
                           game_controller_input_t *new_input)
{
    switch (key) {
    case SDLK_w:
    case SDLK_UP:
        process_keydown(&(old_input->move_up), &(new_input->move_up));
        break;
    case SDLK_s:
    case SDLK_DOWN:
        process_keydown(&(old_input->move_down), &(new_input->move_down));
        break;
    case SDLK_a:
    case SDLK_LEFT:
        process_keydown(&(old_input->move_left), &(new_input->move_left));
        break;
    case SDLK_d:
    case SDLK_RIGHT:
        process_keydown(&(old_input->move_right), &(new_input->move_right));
        break;
    case SDLK_RETURN:
        process_keydown(&(old_input->start), &(new_input->start));
        break;
    case SDLK_SPACE:
        process_keydown(&(old_input->action_down), &(new_input->action_down));
        break;
#ifdef GG_EDITOR
    case SDLK_e:
        osx_process_keydown(&(old_input->editor_mode), &(new_input->editor_mode));
        break;
#endif
    }
}

static void copy_game_memory(game_memory_t *dst, game_memory_t *src)
{
    memcpy(dst->transient_store, src->transient_store, (size_t)src->transient_store_size);
    memcpy(dst->permanent_store, src->permanent_store, (size_t)src->permanent_store_size);
}

static loaded_file_t load_file(const char *path)
{
    loaded_file_t file = { 0 };
    SDL_RWops *sdl_rwops = SDL_RWFromFile(path, "rb");

    if (!sdl_rwops) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "Failed to open file %s", path);
        return file;
    }

    // Get file size
    file.size = SDL_RWsize(sdl_rwops);
    file.contents = malloc(file.size);
    if (!file.contents) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate memory when attempting to open file %s", path);
        return file;
    }

    size_t nb_read_total = 0;
    size_t nb_read = 1;
    char* buf = file.contents;
    while (nb_read_total < file.size && nb_read != 0) {
        nb_read = SDL_RWread(sdl_rwops, buf, 1, ((size_t)file.size - nb_read_total));
        nb_read_total += nb_read;
        buf += nb_read;
    }

    SDL_RWclose(sdl_rwops);

    return file;
}

static void unload_file(loaded_file_t *loaded_file)
{
    assert(loaded_file);
    free(loaded_file->contents);
}

static void gg_log(const char* format, ...)
{
    assert(format);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, buffer);
}

static void handle_debug_counters(game_memory_t *memory, b8 must_print)
{
#ifdef GG_INTERNAL
    if (must_print) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "DEBUG COUNTERS");
    }
    for (u32 i = 0; i < ARRAY_LEN(memory->counters); ++i) {
        dbg_counter_t *counter = &memory->counters[i];
        if (must_print) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "\tcy %I64u, h %u, cy/h %I64u", 
                        counter->cycles, 
                        counter->hits,
                        counter->cycles / counter->hits);
        }
        if (counter->hits)
        {
            counter->cycles = 0;
            counter->hits = 0;
        }
    }
#endif
}

// Queue stuff ===============================

typedef struct {
    wq_fn work_fn;
    void *data;
} wq_entry_t;

#define WQ_SIZE 1024
typedef struct wq_t {
    // Try keep start and end off the same cache line
    // as they will be owned by different threads.
    volatile i32 start;
    wq_entry_t entries[WQ_SIZE];
    volatile i32 end;
    volatile i32 remaining_work_count;
    SDL_sem *semaphore;
} wq_t;

void wqCreate(wq_t *wq, SDL_sem *semaphore)
{
    wq->start = 0;
    wq->end = 0;
    wq->remaining_work_count = 0;
    wq->semaphore = semaphore;
}

b8 wqIsEmpty(wq_t *wq)
{
    return wq->start == wq->end;
}

b8 wqIsFull(wq_t *wq)
{
    i32 next_end = (wq->end + 1) % WQ_SIZE;
    return next_end == wq->start;
}

b8 wqEnqueue(wq_t *wq, wq_fn work_fn, void *data)
{
    i32 start = wq->start;
    i32 end = wq->end;
    i32 next_end = (end + 1) % WQ_SIZE;
    if (start == next_end) {
        // Queue is full.
        return 0;
    }

    wq_entry_t *entry = wq->entries + end;
    entry->work_fn = work_fn;
    entry->data = data;
    // Atomic add?
    atomic_increment(&wq->remaining_work_count);
    wq->end = next_end;

    SDL_SemPost(wq->semaphore);
    return 1;
}

b8 wqDequeue(wq_t *wq, wq_entry_t *entry)
{
    while (!wqIsEmpty(wq)) {
        i32 start = wq->start;
        i32 next_start = (start + 1) % WQ_SIZE;
        i32 original_start = atomic_cas(&wq->start, next_start, start);
        if (original_start == start) {
            wq_entry_t *wq_entry = wq->entries + start;
            entry->work_fn = wq_entry->work_fn;
            entry->data = wq_entry->data;
            return 1;
        }
    }

    return 0;
}

void wqFinishWork(wq_t *wq)
{
    wq_entry_t entry;
    while (wq->remaining_work_count > 0) {
        if (wqDequeue(wq, &entry)) {
            entry.work_fn(entry.data);
            atomic_decrement(&wq->remaining_work_count);
        }
    }
}

// ===========================================

// Thread stuff
// ===========================================
typedef struct {
    u32 index;
    wq_t *work_queue;
} thread_info_t;

int thread_fn(void *data)
{
    thread_info_t *thread_info = (thread_info_t *)data;
    wq_t *work_queue = thread_info->work_queue;

    wq_entry_t entry;
    for (;;) {
        if (wqDequeue(work_queue, &entry)) {
            entry.work_fn(entry.data);
            atomic_decrement(&work_queue->remaining_work_count);
        }
        else
        {
            SDL_SemWait(work_queue->semaphore);
        }
    }
    return 1;
}
// TODO Read the core count to set the thread count.
#define WORKER_THREAD_COUNT 8
// ===========================================


int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        // TODO(Wes): SDL_Init didn't work!
    }

    SDL_sem *semaphore = SDL_CreateSemaphore(0);
    wq_t render_work_queue;
    wqCreate(&render_work_queue, semaphore);
    thread_info_t thread_infos[WORKER_THREAD_COUNT];

    SDL_Thread* threads[WORKER_THREAD_COUNT];
    for (u32 i = 0; i < WORKER_THREAD_COUNT; i++) {
        thread_info_t *thread_info = thread_infos + i;
        thread_info->index = i;
        thread_info->work_queue = &render_work_queue;
        threads[i] = SDL_CreateThread(thread_fn, "gg_worker", (void *)thread_info);
    }

    game_work_queues_t game_work_queues;
    game_work_queues.render_work_queue = &render_work_queue;
    game_work_queues.add_work = wqEnqueue;
    game_work_queues.finish_work = wqFinishWork;

    i32 window_width = 1920;
    i32 window_height = 1080;
    i32 window_pos_x = 0;
    i32 window_pos_y = 0;
    
    SDL_Window *window = SDL_CreateWindow("GG",
                                          window_pos_x,
                                          window_pos_y,
                                          window_width,
                                          window_height,
                                          SDL_WINDOW_RESIZABLE);

#define VSYNC_ON 0
#if VSYNC_ON
    u32 render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#else
    u32 render_flags = SDL_RENDERER_ACCELERATED;
#endif

    u32 frame_buffer_width = 1920;
    u32 frame_buffer_height = 1080;
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, render_flags);
    SDL_RenderSetLogicalSize(renderer, frame_buffer_width, frame_buffer_height);

    // Ensure our frame buffer is on a 16 byte boundary so we can use it with SSE intructions.
    u32 frame_buffer_size = frame_buffer_width * frame_buffer_height * GG_BYTES_PP;
    u8 *pixels = (u8 *)malloc(frame_buffer_size);
    assert(((uintptr_t)pixels & 15) == 0);
    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame_buffer_width, frame_buffer_height);

    SDL_AudioSpec audio_spec_want = {0};
    audio_spec_want.freq = 48000;
    audio_spec_want.format = AUDIO_S16LSB;
    audio_spec_want.channels = 2;
    audio_spec_want.samples =
        (audio_spec_want.freq / 60) * audio_spec_want.channels;

    /* SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(0, 0,
    &audio_spec_want, 0, 0);
    if (audio_device == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Failed to open audio
    device: %s",
    SDL_GetError());
            return -1;
    }*/

    game_memory_t game_memory = allocate_game_memory((void *)Terabytes(2));

    game_lib_paths_t game_lib_paths = get_game_lib_paths();
    game_t game = {0};

    game_frame_buffer_t frame_buffer = {0};
    frame_buffer.w = frame_buffer_width;
    frame_buffer.h = frame_buffer_height;

    game_input_t input[2] = {0};
    game_input_t *new_input = &input[0];
    game_input_t *old_input = &input[1];

    i32 max_joysticks = SDL_NumJoysticks();
    i32 controller_index = 0;
    i32 kb_controller_index = controller_index++;
    init_kb(new_input, kb_controller_index);

    for (i32 i = 0; i < max_joysticks; ++i) {
        if (!SDL_IsGameController(i)) {
            continue;
        }
        if (i >= GG_MAX_CONTROLLERS) {
            break;
        }
        SDL_GameController *controller = SDL_GameControllerOpen(i);
        controller_handles[controller_index] = controller;
        SDL_Joystick *joystick_handle = SDL_GameControllerGetJoystick(controller);
        SDL_Haptic *haptic_handle = SDL_HapticOpenFromJoystick(joystick_handle);
        if (haptic_handle) {
            haptic_handles[controller_index] = haptic_handle;
            if (SDL_HapticRumbleInit(haptic_handle) != 0) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize rumble %s", SDL_GetError());
                // Failed to init rumble so just turn it off.
                SDL_HapticClose(haptic_handles[controller_index]);
                haptic_handles[controller_index] = 0;
            }
        }
        else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize haptics %s", SDL_GetError());
        }
        init_controller(new_input, controller_index);
        controller_index++;
    }

    game_output_t output = {0};

    game_audio_t audio;
    audio.samples_per_second = audio_spec_want.freq;
    audio.sample_count = audio_spec_want.samples;

    game_record_t game_record = {0};
    game_record.input_events = malloc(sizeof(game_input_t) * GG_MAX_RECORDED_INPUT_EVENTS);
    game_record.memory = allocate_game_memory((void *)Terabytes(4));

    game_callbacks_t callbacks = {0};
    callbacks.load_file = &load_file;
    callbacks.unload_file = &unload_file;
    callbacks.log = &gg_log;

    u64 last_time = SDL_GetPerformanceCounter();
    u64 perf_freq = SDL_GetPerformanceFrequency();
    i32 frame_counter = 0;
    f32 frame_sec = 1.0f / GG_TARGET_FPS;
    f32 frame_accumulator = 0.0f;
    SDL_Event event;
    b8 recording = false;
    b8 playing_back = false;
    b8 quitting = 0;
    // SDL_PauseAudioDevice(audio_device, 0);
    while (!quitting) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quitting = 1;
                break;
            case SDL_KEYUP:
                if (kb_controller_index != -1) {
                    handle_keyup(event.key.keysym.sym,
                                 &old_input->controllers[kb_controller_index],
                                 &new_input->controllers[kb_controller_index]);
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_f) {
                    u32 is_fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                if (event.key.keysym.sym == SDLK_r) {
                    if (playing_back) {
                        SDL_LogInfo(
                            SDL_LOG_CATEGORY_APPLICATION, "Cannot record while performing playback!");
                    }
                    else if (recording) {
                        SDL_LogInfo(
                            SDL_LOG_CATEGORY_APPLICATION, "Recorded %d input events", game_record.input_record_index);
                        recording = 0;
                    }
                    else {
                        copy_game_memory(&game_record.memory, &game_memory);
                        game_record.input_record_index = 0;
                        recording = 1;
                    }
                }
                if (event.key.keysym.sym == SDLK_p) {
                    game_record.input_playback_index = 0;
                    if (recording) {
                        SDL_LogInfo(
                            SDL_LOG_CATEGORY_APPLICATION, "Cannot perform playback while recording!");
                    }
                    else if (playing_back) {
                        playing_back = 0;
                    }
                    else {
                        playing_back = 1;
                    }
                }
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quitting = 1;
                }

                // NOTE(Wes): Pass other keys to the game.
                if (kb_controller_index != -1) {
                    handle_keydown(event.key.keysym.sym,
                                   &old_input->controllers[kb_controller_index],
                                   &new_input->controllers[kb_controller_index]);
                }
            }
        }

        // TODO(Wes): Debug builds only.
        time_t last_modified = get_last_modified(game_lib_paths.game_lib);
        if (difftime(last_modified, game.last_modified) != 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "gg game modified - Reloading");
            SDL_UnloadObject(game.lib);
            copy_file(game_lib_paths.game_lib, game_lib_paths.temp_game_lib);
            load_game(&game, game_lib_paths.temp_game_lib);
            game.last_modified = get_last_modified(game_lib_paths.game_lib);
        }

        handle_controller_input(new_input, old_input);
        new_input->delta_time = frame_sec;

        if (recording) {
            game_record.input_events[game_record.input_record_index++] = *new_input;
        } else if (playing_back) {
            u32 playback_index = game_record.input_playback_index++ %
                                 (game_record.input_record_index + 1);
            if (playback_index == 0) {
                copy_game_memory(&game_memory, &game_record.memory);
            }
            *new_input = game_record.input_events[playback_index];
        }

        // NOTE(Wes): The frame buffer pixels point directly to the SDL texture.
        //SDL_LockTexture(texture, 0, (void **)&frame_buffer.data, (i32 *)&frame_buffer.pitch);
        frame_buffer.data = pixels;
        frame_buffer.pitch = frame_buffer_width * GG_BYTES_PP;

        game.update_and_render_fn(&game_memory, &frame_buffer, &audio, new_input,
            &output, &callbacks, &game_work_queues);
        //SDL_UnlockTexture(texture);
        SDL_UpdateTexture(texture, 0, pixels, frame_buffer.pitch);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, 0, 0);
        SDL_RenderPresent(renderer);

        // SDL_QueueAudio(audio_device, (void *)audio.samples,
        // audio.sample_count *
        // sizeof(i16));
        // u32 queued_audio_size = SDL_GetQueuedAudioSize(audio_device);

        for (u32 i = 0; i < GG_MAX_CONTROLLERS; ++i) {
            if (!haptic_handles[i]) {
                continue;
            }
            if (output.controllers[i].rumble) {
                SDL_HapticRumblePlay(haptic_handles[i],
                                     output.controllers[i].intensity,
                                     SDL_HAPTIC_INFINITY);
            } else {
                SDL_HapticRumbleStop(haptic_handles[i]);
            }
        }

        game_input_t *temp = new_input;
        new_input = old_input;
        old_input = temp;

        u64 current_time = SDL_GetPerformanceCounter();
        frame_sec = (current_time - last_time) / (f32)perf_freq;
        frame_accumulator += frame_sec;
        last_time = current_time;
        b8 must_print = false;
        if (frame_counter++ % 120 == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "-------------------");
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Frame time %.02f ms", frame_sec * 1000);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "FPS %.01f", 120 / frame_accumulator);
            frame_accumulator = 0.0f;
            // SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Queued
            // audio bytes %d",
            // queued_audio_size);
            must_print = true;
        }
        handle_debug_counters(&game_memory, must_print);
    }

    SDL_Quit();
    return 0;
}
