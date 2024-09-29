add_library(idbvfs idbvfs.cpp idbvfs.h)
target_compile_features(idbvfs PUBLIC cxx_std_11)
target_compile_options(idbvfs PRIVATE -fno-rtti -fno-exceptions)
target_include_directories(idbvfs
  PUBLIC .
  PRIVATE ../libs/sqlite-amalgamation ../libs/sqlite-vfs-cpp
)
if(EMSCRIPTEN)
  target_link_options(idbvfs
    PUBLIC
      -lidbfs.js
  )
endif()
if(IDBVFS_TRACE)
  target_compile_definitions(idbvfs PRIVATE TRACE)
endif()
