add_library(idbvfs idbvfs.cpp)
target_compile_features(idbvfs PUBLIC cxx_std_11)
target_include_directories(idbvfs
  PUBLIC .
  PRIVATE ../libs/sqlite-amalgamation ../libs/sqlite-vfs-cpp
)
if(IDBVFS_TRACE)
  target_compile_definitions(idbvfs PRIVATE TRACE)
endif()
