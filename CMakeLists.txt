cmake_minimum_required(VERSION 3.1)

if(DEFINED PROJECT_NAME)
  set(IS_SUBPROJECT ON)
else()
  set(IS_SUBPROJECT OFF)
endif()

project(idbvfs LANGUAGES C CXX)

# Options
include(CMakeDependentOption)
cmake_dependent_option(IDBVFS_BUILD_TESTS "Builds unit tests" ON "NOT EMSCRIPTEN;NOT IS_SUBPROJECT" OFF)
cmake_dependent_option(IDBVFS_BUILD_DEMO "Builds demo WASM file" OFF "EMSCRIPTEN;NOT IS_SUBPROJECT" OFF)
option(IDBVFS_TRACE "Logs trace messages on I/O operations" OFF)

# idbvfs library
add_library(idbvfs idbvfs.cpp)
target_compile_features(idbvfs PUBLIC cxx_std_11)
target_include_directories(idbvfs PRIVATE libs/sqlite-amalgamation libs/sqlite-vfs-cpp)
if(IDBVFS_TRACE)
  target_compile_definitions(idbvfs PRIVATE TRACE)
endif()

# unit tests
if(IDBVFS_BUILD_TESTS)
  add_subdirectory(libs/catch2)
  add_executable(tests tests/test.cpp libs/sqlite-amalgamation/sqlite3.c)
  target_link_libraries(tests PRIVATE idbvfs Catch2::Catch2WithMain)
endif()

# demo app
if(EMSCRIPTEN AND IDBVFS_BUILD_DEMO)
  add_subdirectory(demo)
endif()
