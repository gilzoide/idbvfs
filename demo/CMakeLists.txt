add_executable(demo main.cpp ../libs/sqlite-amalgamation/sqlite3.c)
target_link_libraries(demo idbvfs)
target_link_options(demo
  PUBLIC
    -lidbfs.js
    -sASYNCIFY=1
    -sASYNCIFY_STACK_SIZE=32768
    -sEXPORTED_FUNCTIONS=_run_sql
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
)
configure_file(index.html index.html COPYONLY)
