CC = clang
CFLAGS = -Wall -Werror -std=c11 -g -msse4 -fPIC -ffast-math
DEFINES = -DKATANA_DEBUG 
DISABLED_WARNINGS = -Wno-unused-function

BIN_TARGET = katana
BIN_SRC = src/osx_katana.c
GAME_TARGET = katana.so
GAME_SRC = src/katana.c

SDL_HEADERS = /usr/local/include/SDL2
SDL_LIB = SDL2

BUILD_DIR = build


.PHONY: all
all: $(BUILD_DIR) $(BIN_TARGET) $(GAME_TARGET) tags cscope.out

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_TARGET):
	$(CC) $(BIN_SRC) -o $(BUILD_DIR)/$(BIN_TARGET) $(CFLAGS) -I$(SDL_HEADERS) -l$(SDL_LIB) $(DEFINES) $(DISABLED_WARNINGS)

$(GAME_TARGET):
	$(CC) $(GAME_SRC) -shared -o $(BUILD_DIR)/$(GAME_TARGET) $(CFLAGS) $(DEFINES) $(DISABLED_WARNINGS)

tags:
	/usr/local/bin/ctags -R --exclude=.git --exclude=build --extra=+f --languages=c --tag-relative

cscope.out:
	/usr/local/bin/cscope -R -b


clean:
	@rm -rf $(BUILD_DIR)
