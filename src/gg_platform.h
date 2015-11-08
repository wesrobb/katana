#pragma once

#include <stdint.h>

#ifdef _MSC_VER
#ifdef GG_DLL
#define DLL_FN __declspec(dllexport)
#else
#define DLL_FN __declspec(dllimport)
#endif
#else
#define DLL_FN
#endif

#define GG_TARGET_FPS 60

#if GG_DEBUG
#define assert(Expression)                                                     \
    if (!(Expression)) {                                                       \
        *(volatile int *)0 = 0;                                                \
    }
#else
#define assert(Expression)
#endif

#define set_alignment(BYTE_COUNT) __attribute__((__aligned__(BYTE_COUNT)))
#define is_aligned(POINTER, BYTE_COUNT)                                        \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#define gg_min(a, b) a < b ? a : b
#define gg_max(a, b) a > b ? a : b

typedef unsigned char b8;
typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

typedef struct {
    u8 *transient_store;
    u64 transient_store_size;

    u8 *permanent_store;
    u64 permanent_store_size;

    b8 is_initialized;
} game_memory_t;

typedef struct {
    u32 *data; // Always 4bpp. RR GG BB AA
    u32 w;
    u32 h;
    u32 pitch;
} game_frame_buffer_t;

typedef struct {
    i32 half_transition_count;
    b8 ended_down;
} game_button_state_t;

typedef struct {
    b8 is_connected;
    b8 is_analog;
    f32 left_stick_x;
    f32 left_stick_y;
    f32 right_stick_x;
    f32 right_stick_y;

    union {
        game_button_state_t Buttons[12];
        struct {
            game_button_state_t move_up;
            game_button_state_t move_down;
            game_button_state_t move_left;
            game_button_state_t move_right;

            game_button_state_t action_up;
            game_button_state_t action_down;
            game_button_state_t action_left;
            game_button_state_t action_right;

            game_button_state_t left_shoulder;
            game_button_state_t right_shoulder;

            game_button_state_t back;
            game_button_state_t start;

            game_button_state_t terminator;
        };
    };
} game_controller_input_t;

#define GG_MAX_CONTROLLERS 4
typedef struct {
    game_button_state_t mouse_buttons[5];
    i32 mouse_x;
    i32 mouse_y;
    i32 mouse_z;

    game_controller_input_t controllers[GG_MAX_CONTROLLERS];

    f32 delta_time;
} game_input_t;

typedef struct {
    b8 rumble;
    f32 intensity;
} game_controller_output_t;

#define GG_MAX_AUDIO_SAMPLE_COUNT_PER_FRAME 8192
typedef struct {
    i16 samples[GG_MAX_AUDIO_SAMPLE_COUNT_PER_FRAME];
    u32 sample_count;
    u32 samples_per_second;
} game_audio_t;

typedef struct {
    game_controller_output_t controllers[GG_MAX_CONTROLLERS];
} game_output_t;

typedef struct {
    void *contents;
    i64 size;
    b8 success;
} loaded_file_t;

typedef loaded_file_t (*load_file_fn)(const char *filename);
typedef void (*unload_file_fn)(loaded_file_t *);

// TODO(Wes): Pass these once during game_init or something.
typedef struct {
    load_file_fn load_file;
    unload_file_fn unload_file;
} game_callbacks_t;

DLL_FN void game_update_and_render(game_memory_t *memory,
                            game_frame_buffer_t *frame_buffer,
                            game_audio_t *audio,
                            game_input_t *input,
                            game_output_t *output,
                            game_callbacks_t *callbacks);
