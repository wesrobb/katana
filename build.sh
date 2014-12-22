mkdir -p build
pushd build
clang ../src/sdl_katana.c -o katana -g -I/usr/local/include/SDL2 -lSDL2
clang -shared -o katana.so -g -fPIC ../src/katana.c
popd
