#pragma once

#include <stdint.h>

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
        void *transient_store;
        u64 transient_store_size;

        void *permanent_store;
        u64 permanent_store_size;
} game_memory_t;

typedef struct {
        i32 block_x;
        i32 block_y;
} game_state_t;

typedef struct {
        u32 *pixels; // Always 4 bytes per pixel. AA BB GG RR
        u32 width;
        u32 height;
} game_frame_buffer_t;

typedef struct {
        i32 half_transition_count;
        b8 ended_down;
} game_button_state_t;

#define KATANA_MAX_STICK_VALUES 16
typedef struct {
        b8 is_connected;
        b8 is_analog;
        f32 stick_x[KATANA_MAX_STICK_VALUES];
        f32 stick_y[KATANA_MAX_STICK_VALUES];
        u32 stick_value_count;

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

                        // NOTE(casey): All buttons must be added above this line

                        game_button_state_t terminator;
                };
        };
} game_controller_input_t;

#define KATANA_MAX_CONTROLLERS 4
typedef struct {
        game_button_state_t mouse_buttons[5];
        i32 mouse_x;
        i32 mouse_y;
        i32 mouse_z;

        game_controller_input_t controllers[KATANA_MAX_CONTROLLERS];
} game_input_t;

typedef struct {
        b8 rumble;
        f32 intensity;
} game_controller_output_t;

typedef struct {
        game_controller_output_t controllers[KATANA_MAX_CONTROLLERS];
} game_output_t;

void game_update_and_render(game_memory_t *memory, game_frame_buffer_t *frame_buffer, game_input_t *input,
                            game_output_t *output);
