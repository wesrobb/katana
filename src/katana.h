#pragma once

#include <stdint.h>

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
	u32 *pixels; // Always 4 bytes per pixel. AA BB GG RR
	u32 width;
	u32 height;
} game_frame_buffer_t;

void game_update_and_render(game_memory_t *memory,
			    game_frame_buffer_t *frame_buffer);
