#include <iostream>

#include <idbvfs.h>
#include <sqlite3.h>

#include <catch2/catch_test_macros.hpp>

int print_data(void *_, int column_count, char **column_values, char **column_names) {
	std::cout << column_values[0];
	for (int i = 1; i < column_count; i++) {
		std::cout << ' ' << column_values[i];
	}
	std::cout << std::endl;
	return SQLITE_OK;
}

TEST_CASE("SQLite using idbvfs can read and write database", "[idbvfs]") {
	idbvfs_register(false);

	sqlite3 *db;
	REQUIRE(sqlite3_open_v2("test.sqlite", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, IDBVFS_NAME) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS test_table(id INTEGER PRIMARY KEY)", NULL, NULL, NULL) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "INSERT INTO test_table(id) VALUES(NULL)", NULL, NULL, NULL) == SQLITE_OK);
	REQUIRE(sqlite3_exec(db, "SELECT count(*) FROM test_table", &print_data, NULL, NULL) == SQLITE_OK);

	sqlite3_close(db);
}

TEST_CASE("SQLite using idbvfs can read and write database file", "[idbvfs]") {
	idbvfs_register(false);

	{
		sqlite3 *db;
		REQUIRE(sqlite3_open_v2("test_file.sqlite", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
		REQUIRE(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS test_table(id INTEGER PRIMARY KEY)", NULL, NULL, NULL) == SQLITE_OK);
		REQUIRE(sqlite3_exec(db, "INSERT INTO test_table(id) VALUES(NULL)", NULL, NULL, NULL) == SQLITE_OK);
		sqlite3_close(db);
	}

	{
		sqlite3 *db;
		REQUIRE(sqlite3_open_v2("test_file.sqlite", &db, SQLITE_OPEN_READWRITE, IDBVFS_NAME) == SQLITE_OK);
		REQUIRE(sqlite3_exec(db, "INSERT INTO test_table(id) VALUES(NULL)", NULL, NULL, NULL) == SQLITE_OK);
		REQUIRE(sqlite3_exec(db, "SELECT count(*) FROM test_table", &print_data, NULL, NULL) == SQLITE_OK);
		sqlite3_close(db);
	}
}
