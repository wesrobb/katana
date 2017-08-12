#include <copyfile.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

static time_t get_last_modified(char *path)
{
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtimespec.tv_sec;
}

static game_memory_t allocate_game_memory(void *base_address)
{
    game_memory_t memory = { 0 };
    memory.transient_store_size = Megabytes(512);
    memory.transient_store =
        mmap(base_address, memory.transient_store_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    memory.permanent_store_size = Megabytes(64);
    memory.permanent_store = mmap((void *)(base_address + memory.transient_store_size),
        memory.permanent_store_size,
        PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_PRIVATE,
        -1,
        0);
    return memory;
}

static void copy_file(char *existing_file_path, char *new_file_path)
{
    copyfile(existing_file_path, new_file_path, 0, COPYFILE_ALL);
}