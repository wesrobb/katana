CC = clang
CFLAGS = -Wall -Werror -msse4 -ffast-math -std=c11 -Xlinker /subsystem:windows
RELEASE_FLAGS = -O2 -g
DEBUG_FLAGS = -O0 -g
DEFINES = -DGG_INTERNAL -DGAME_DLL=gg.dll -DGAME_TEMP_DLL=gg_temp.dll -DGG_EDITOR
DEFINES_D = -DGG_DEBUG -DGG_INTERNAL -DGAME_DLL=gg_d.dll -DGAME_TEMP_DLL=gg_temp_d.dll -DGG_EDITOR
DISABLED_WARNINGS = -Wno-unused-function -Wno-unused-variable -Wno-missing-braces -Wno-format-security -Wno-pragma-pack -Wno-macro-redefined -Wno-deprecated-declarations

BIN_TARGET = gg.exe
BIN_TARGET_D = gg_d.exe
BIN_SRC = src/gg_platform.c
GAME_TARGET = gg.dll
GAME_TARGET_D = gg_d.dll
GAME_SRC = src/gg_unitybuild.c

C_FILES = $(wildcard src/*.c)
H_FILES = $(wildcard src/*.h)

# USR_LIB_DIR = /usr/local/lib
# USR_HEADER_DIR = /usr/local/include
# SDL_HEADERS = /usr/local/include/SDL2
# SDL_LIB = SDL2
USR_LIB_DIR = /usr/local/lib
USR_HEADER_DIR = /usr/local/include
SDL_HEADERS = libs/SDL2-2.0.4/include
SDL_LIBS = libs/SDL2-2.0.4/lib/x64
SDL_LIB = SDL2
SDL_MAIN_LIB = SDL2main

BUILD_DIR = build

.PHONY: release debug
all: release debug
release: $(BUILD_DIR) $(BIN_TARGET) $(GAME_TARGET) # tags cscope.out
debug: $(BUILD_DIR) $(BIN_TARGET_D) $(GAME_TARGET_D) # tags cscope.out

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_TARGET):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET) $(CFLAGS) $(RELEASE_FLAGS) -I$(SDL_HEADERS) -L$(SDL_LIBS) -l$(SDL_MAIN_LIB) -l$(SDL_LIB) $(DEFINES) $(DISABLED_WARNINGS)

$(GAME_TARGET):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET) $(CFLAGS) $(RELEASE_FLAGS) $(DEFINES) $(DISABLED_WARNINGS)

$(BIN_TARGET_D):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET_D) $(CFLAGS) $(DEBUG_FLAGS) -I$(SDL_HEADERS) -L$(SDL_LIBS) -l$(SDL_MAIN_LIB) -l$(SDL_LIB) $(DEFINES_D) $(DISABLED_WARNINGS)

$(GAME_TARGET_D):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET_D) $(CFLAGS) $(DEBUG_FLAGS) $(DEFINES_D) $(DISABLED_WARNINGS)

# tags: $(C_FILES) $(H_FILES)
# 	/usr/local/bin/ctags -R --exclude=.git --exclude=build --extra=+f --languages=c --tag-relative

# cscope.out: $(C_FILES) $(H_FILES)
# 	/usr/local/bin/cscope -R -b


clean:
	@rm -rf $(BUILD_DIR)
