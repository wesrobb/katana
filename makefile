CC = gcc
CFLAGS = -Wall -Werror -std=c11 -msse4 -fPIC -ffast-math 
RELEASE_FLAGS = -O2 -g
DEBUG_FLAGS = -O0 -g -ggdb
DEFINES = -DGG_INTERNAL -DGAME_DLL=gg.so -DGAME_TEMP_DLL=gg_temp.so
DEFINES_D = -DGG_DEBUG -DGG_INTERNAL -DGAME_DLL=gg_d.so -DGAME_TEMP_DLL=gg_temp_d.so 
DISABLED_WARNINGS = -Wno-unused-function -Wno-unused-variable -Wno-missing-braces

BIN_TARGET = gg
BIN_TARGET_D = gg_d
BIN_SRC = src/gg_osx.c
GAME_TARGET = gg.so
GAME_TARGET_D = gg_d.so
GAME_SRC = src/gg_unitybuild.c

C_FILES = $(wildcard src/*.c)
H_FILES = $(wildcard src/*.h)

USR_LIB_DIR = /usr/local/lib
USR_HEADER_DIR = /usr/local/include
SDL_HEADERS = /usr/local/include/SDL2
SDL_LIB = SDL2

BUILD_DIR = build


.PHONY: release debug
release: $(BUILD_DIR) $(BIN_TARGET) $(GAME_TARGET) tags cscope.out
debug: $(BUILD_DIR) $(BIN_TARGET_D) $(GAME_TARGET_D) tags cscope.out

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_TARGET):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET) $(CFLAGS) $(RELEASE_FLAGS) -I$(SDL_HEADERS) -L$(USR_LIB_DIR) -l$(SDL_LIB) $(DEFINES) $(DISABLED_WARNINGS)

$(GAME_TARGET):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET) $(CFLAGS) $(RELEASE_FLAGS) -I$(USR_HEADER_DIR)  $(DEFINES) $(DISABLED_WARNINGS)

$(BIN_TARGET_D):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET_D) $(CFLAGS) $(DEBUG_FLAGS) -I$(SDL_HEADERS) -L$(USR_LIB_DIR) -l$(SDL_LIB) $(DEFINES_D) $(DISABLED_WARNINGS)

$(GAME_TARGET_D):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET_D) $(CFLAGS) $(DEBUG_FLAGS) -I$(USR_HEADER_DIR) $(DEFINES_D) $(DISABLED_WARNINGS)

tags: $(C_FILES) $(H_FILES)
	/usr/local/bin/ctags -R --exclude=.git --exclude=build --extra=+f --languages=c --tag-relative

cscope.out: $(C_FILES) $(H_FILES)
	/usr/local/bin/cscope -R -b


clean:
	@rm -rf $(BUILD_DIR)
