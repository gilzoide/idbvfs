cmake_minimum_required(VERSION 3.21...4.1)

project(idbvfs LANGUAGES C CXX)

# Options
include(CMakeDependentOption)
cmake_dependent_option(IDBVFS_BUILD_TESTS "Builds unit tests" ON "NOT EMSCRIPTEN;PROJECT_IS_TOP_LEVEL" OFF)
cmake_dependent_option(IDBVFS_BUILD_DEMO "Builds demo WASM file" OFF "EMSCRIPTEN;PROJECT_IS_TOP_LEVEL" OFF)
option(IDBVFS_TRACE "Logs trace messages on I/O operations" OFF)

# idbvfs library
add_subdirectory(src)

# unit tests
if(IDBVFS_BUILD_TESTS)
  add_subdirectory(libs/sqlite-amalgamation)
  add_subdirectory(libs/catch2)
  add_subdirectory(tests)
endif()

# demo app
if(EMSCRIPTEN AND IDBVFS_BUILD_DEMO)
  add_subdirectory(libs/sqlite-amalgamation)
  add_subdirectory(demo)
endif()
