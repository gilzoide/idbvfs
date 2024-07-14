add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE idbvfs idbvfs_sqlite3 Catch2::Catch2WithMain)
