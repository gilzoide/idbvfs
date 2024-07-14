add_library(idbvfs_sqlite3 STATIC sqlite3.c)
target_include_directories(idbvfs_sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
