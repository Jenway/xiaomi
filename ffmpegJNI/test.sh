cmake -S . -B build -GNinja -DBUILD_ANDROID_LIB=OFF
cmake --build build

ln -sf build/compile_commands.json compile_commands.json
