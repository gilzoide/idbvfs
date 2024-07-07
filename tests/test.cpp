#include "../idbvfs.h"
#include <sqlite3.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("SQLite using idbvfs can read and write database", "[idbvfs]") {
	idbvfs_register(false);

	sqlite3 *db;
	REQUIRE(sqlite3_open_v2("test.sqlite", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, IDBVFS_NAME) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS test_table(id INTEGER PRIMARY KEY)", NULL, NULL, NULL) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "INSERT INTO test_table(id) VALUES(NULL)", NULL, NULL, NULL) == SQLITE_OK);

	sqlite3_close(db);
}
