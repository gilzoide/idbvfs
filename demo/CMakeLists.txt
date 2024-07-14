add_executable(demo main.cpp)
target_link_libraries(demo idbvfs idbvfs_sqlite3)
target_link_options(demo
  PUBLIC
    -sEXPORTED_FUNCTIONS=_run_sql
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
)
configure_file(index.html index.html COPYONLY)
