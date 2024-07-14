# idbvfs
WebAssembly [SQLite](https://sqlite.org/) [VFS](https://www.sqlite.org/vfs.html) for web browsers, powered by [IndexedDB](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API).

Checkout the live demo: https://gilzoide.github.io/idbvfs/

Note: this project implements only the SQLite VFS that uses IndexedDB for persistence.
You must compile and link SQLite with your app yourself.
This project is supposed to be statically linked to your WebAssembly applications.


### This project is for...
- Web browser applications that use SQLite directly from WebAssembly-compiled code, for example apps built using C/C++ game engines


### This project is NOT for...
- Web browser applications that use SQLite from JavaScript, like users of [sqlite3.wasm](https://sqlite.org/wasm) or [sql.js](https://sql.js.org/)


## How to use
TL;DR: call `idbvfs_register` before opening your SQLite databases and that's it!
```c
// 1. Include `idbvfs.h`
#include <idbvfs.h>

// 2. Somewhere in your app's initialization, register idbvfs.
// Pass `1` to make idbvfs the default VFS for new connections.
int result = idbvfs_register(1);
if (restult != SQLITE_OK) {
    // handle errors if necessary...
}

// 3. Just use SQLite normally
sqlite3 *db;
int result = sqlite3_open_v2(
  "mydb", &db,
  SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
  IDBVFS_NAME
);
// The above is the same as below, if idbvfs is the default VFS
// int result = sqlite3_open("mydb", &db);
```


### Linking idbvfs in CMake builds:
```cmake
# 1. Import `idbvfs` as a subdirectory
# This will build idbvfs as a static library
add_subdirectory(path/to/idbvfs)

# 2. Link your library/executable to the `idbvfs` library
# This will also add the necessary includes for `idbvfs.h`
# as well as link the IDBFS and turn on ASYNCIFY
target_link_libraries(my-wasm-app PUBLIC idbvfs)

# 3. (optional) Configure a bigger `ASYNCIFY_STACK_SIZE` if you
# have problems when running SQLite code due to Asyncify stack size.
target_link_options(my-wasm-app PUBLIC -sASYNCIFY_STACK_SIZE=24576)
```


### Linking idbvfs in non-CMake builds:
```sh
# 1. Compile idbvfs using CMake, from the project root
# This will output a static library at `build/libidbvfs.a`
emcmake cmake . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Link your app with `build/libidbvfs.a`.
# Don't forget to add `-lidbfs.js` and `-sASYNCIFY=1` link options
# or idbvfs will not work!
emcc -o my-wasm-app my-wasm-app.o \
  -lidbfs.js -sASYNCIFY=1 \
  -Lidbvfs/build -lidbvfs
```
