mkdir -p build
pushd build
clang ../src/osx_katana.c -o katana -g -I/usr/local/include/SDL2 -lSDL2 -DKATANA_DEBUG
clang -shared -o katana.so -g -msse4 -fPIC -ffast-math ../src/katana.c -DKATANA_DEBUG
popd 
