#include <cstdlib>

#include <emscripten.h>

#include "../idbvfs.h"
#include "../libs/sqlite-amalgamation/sqlite3.h"

sqlite3 *db;

static int sql_callback(void *userdata, int column_count, char **column_values, char **column_names) {
	bool *is_first = (bool *) userdata;
	if (*is_first) {
		for (int i = 0; i < column_count; i++) {
			EM_ASM({
				onSQLColumnName($0, UTF8ToString($1))
			}, i, column_names[i]);
		}
		EM_ASM({
			onSQLColumnNameFinished()
		});
		*is_first = false;
	}
	for (int i = 0; i < column_count; i++) {
		EM_ASM({
			onSQLColumnValue($0, UTF8ToString($1))
		}, i, column_values[i]);
	}
	return SQLITE_OK;
}

static int initialize_sql() {
	idbvfs_register(true);
	return sqlite3_open_v2("idbvfs-demo", &db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, IDBVFS_NAME);
}

extern "C" void run_sql(const char *sql) {
	if (!db) {
		int result = initialize_sql();
		if (result != SQLITE_OK) {
			if (const char *errmsg = sqlite3_errmsg(db)) {
				EM_ASM({
					onSQLError(UTF8ToString($0))
				}, errmsg);
			}
			return;
		}
	}

	char *errmsg;
	bool is_first = true;
	sqlite3_exec(db, sql, sql_callback, &is_first, &errmsg);
	if (errmsg) {
		EM_ASM({
			onSQLError(UTF8ToString($0))
		}, errmsg);
		sqlite3_free(errmsg);
	}
}
