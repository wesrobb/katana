CC = clang
CFLAGS = -Wall -Werror -std=c11 -O0 -g -msse4 -fPIC -ffast-math -Wno-unused-variable -Wno-missing-braces
DEFINES = -DGG_DEBUG 
DISABLED_WARNINGS = -Wno-unused-function

BIN_TARGET = gg
BIN_SRC = src/gg_osx.c
GAME_TARGET = gg.so
GAME_SRC = src/gg_unitybuild.c

C_FILES = $(wildcard src/*.c)
H_FILES = $(wildcard src/*.h)

SDL_HEADERS = /usr/local/include/SDL2
USR_LIB_DIR = /usr/local/lib
SDL_LIB = SDL2

BUILD_DIR = build


.PHONY: all
all: $(BUILD_DIR) $(BIN_TARGET) $(GAME_TARGET) tags cscope.out

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_TARGET):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET) $(CFLAGS) -I$(SDL_HEADERS) -L$(USR_LIB_DIR) -l$(SDL_LIB) $(DEFINES) $(DISABLED_WARNINGS)

$(GAME_TARGET):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET) $(CFLAGS) $(DEFINES) $(DISABLED_WARNINGS)

tags: $(C_FILES) $(H_FILES)
	/usr/local/bin/ctags -R --exclude=.git --exclude=build --extra=+f --languages=c --tag-relative

cscope.out: $(C_FILES) $(H_FILES)
	/usr/local/bin/cscope -R -b


clean:
	@rm -rf $(BUILD_DIR)
