#include <sys/stat.h>
#include <Windows.h>

#include "gg_platform.h"

static time_t get_last_modified(char *path)
{
    struct stat attr;
    int error_code = stat(path, &attr);
    return attr.st_mtime;
}

static game_memory_t allocate_game_memory(void *base_address)
{
    game_memory_t memory = { 0 };
    memory.permanent_store_size = Megabytes(64);
    memory.transient_store_size = Megabytes(512);

    u64 total_size = memory.transient_store_size + memory.permanent_store_size;
    void *buffer = VirtualAlloc(base_address,
        (size_t)total_size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    memory.permanent_store = (u8 *)buffer;
    memory.transient_store = memory.permanent_store + memory.permanent_store_size;

    return memory;
}

static void copy_file(char *existing_file_path, char *new_file_path)
{
    CopyFile(existing_file_path, new_file_path, 0);
}

static void atomic_increment(i32 volatile *value)
{
    InterlockedIncrement((long volatile *)value);
}

static void atomic_decrement(i32 volatile *value)
{
    InterlockedDecrement((long volatile *)value);
}

static i32 atomic_cas(i32 volatile *destination, i32 new_value, i32 comparand)
{
    return InterlockedCompareExchange((long volatile *)destination, new_value, comparand);
}