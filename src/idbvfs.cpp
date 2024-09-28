#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <SQLiteVfs.hpp>

#include "idbvfs.h"

/// Used size for Indexed DB "disk sectors"
#ifndef DISK_SECTOR_SIZE
	#define DISK_SECTOR_SIZE 32
#endif

/// Return the minimum value between `a` and `b`
#define MIN(a, b) \
	((a) < (b) ? (a) : (b))

/// Indexed DB key used to store idbvfs file sizes
#define IDBVFS_SIZE_KEY "file_size"


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
// Polyfill used solely for automated testing.
// Do not use this VFS without emscripten on any other circumstance.
#include "../tests/emscripten_polyfill.hpp"
#endif


#ifdef TRACE
static void TRACE_LOG(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	static char _idbvfs_trace_log_buffer[1024];
	vsnprintf(_idbvfs_trace_log_buffer, sizeof(_idbvfs_trace_log_buffer), fmt, args);
#ifdef __EMSCRIPTEN__
	EM_ASM({ console.log(UTF8ToString($0)) }, _idbvfs_trace_log_buffer);
#else
	printf("%s\n", _idbvfs_trace_log_buffer);
#endif
	va_end(args);
}
#else
	#define TRACE_LOG(...)
#endif

using namespace sqlitevfs;

class IdbPage {
public:
	IdbPage(const char *dbname, int page_number)
		: dbname(dbname)
	{
		snprintf(filename, sizeof(filename), "%d", page_number);
	}

	~IdbPage() {
		dispose();
	}

	bool exists() const {
		int exists = 0;
		int error = 0;
		emscripten_idb_exists(dbname, filename, &exists, &error);
		return !error && exists;
	}

	int load() {
		dispose();
		int size = 0;
		int error = 0;
		emscripten_idb_load(dbname, filename, (void **) &buffer, &size, &error);
		return error ? 0 : size;
	}

	int load_into(uint8_t *data, int data_size, sqlite3_int64 offset_in_page) {
		int loaded_size = load();
		if (loaded_size <= 0) {
			return 0;
		}
		int copied_bytes = MIN(data_size, loaded_size - offset_in_page);
		if (copied_bytes > 0) {
			memcpy(data, buffer + offset_in_page, copied_bytes);
			return copied_bytes;
		}
		else {
			return 0;
		}
	}

	int load_into(std::vector<uint8_t>& out_buffer) {
		int loaded_size = load();
		out_buffer.resize(loaded_size);
		if (loaded_size > 0) {
			memcpy(out_buffer.data(), buffer, loaded_size);
		}
		return loaded_size;
	}

	int store(const void *data, int data_size) {
		int error;
		emscripten_idb_store(dbname, filename, (void*) data, data_size, &error);
		return error ? 0 : data_size;
	}

	int store(const std::vector<uint8_t> data) {
		return store(data.data(), data.size());
	}

	bool truncate(sqlite3_int64 new_size) {
		int sector_size = load();
		if (sector_size > new_size) {
			int error = 0;
			emscripten_idb_store(dbname, filename, buffer, new_size, &error);
			return !error;
		}
		else {
			return false;
		}
	}

	bool remove() {
		int exists = 0;
		int error = 0;
		emscripten_idb_exists(dbname, filename, &exists, &error);
		if (exists) {
			emscripten_idb_delete(dbname, filename, &error);
			return !error;
		}
		else {
			return false;
		}
	}

	void dispose() {
		if (buffer) {
			free(buffer);
			buffer = nullptr;
		}
	}

private:
	const char *dbname;
	char filename[16];
	uint8_t *buffer = nullptr;
};

struct IdbFileSize {
	IdbFileSize() {}
	IdbFileSize(sqlite3_filename file_name, bool autoload = true) : file_name(file_name) {
		if (autoload) {
			load();
		}
	}

	bool exists() const {
		int exists = 0;
		int error = 0;
		emscripten_idb_exists(file_name, IDBVFS_SIZE_KEY, &exists, &error);
		return !error && exists;
	}

	void load() {
		void *size_buffer;
		int size_buffer_size;
		int error;
		emscripten_idb_load(file_name, IDBVFS_SIZE_KEY, &size_buffer, &size_buffer_size, &error);
		if (error) {
			file_size = 0;
		}
		else {
			sscanf((const char *) size_buffer, "%lu", &file_size);
			free(size_buffer);
			is_dirty = false;
		}
	}

	size_t get() const {
		return file_size;
	}

	void set(size_t new_file_size) {
		if (new_file_size != file_size) {
			file_size = new_file_size;
			is_dirty = true;
		}
	}

	void update_if_greater(size_t new_file_size) {
		if (new_file_size > file_size) {
			set(new_file_size);
		}
	}

	bool sync() {
		if (!is_dirty) {
			return true;
		}
		char buffer[16];
		int written_size = snprintf(buffer, sizeof(buffer), "%lu", file_size);
		int error;
		emscripten_idb_store(file_name, IDBVFS_SIZE_KEY, buffer, MIN(written_size, sizeof(buffer)), &error);
		return error == 0;
	}

private:
	sqlite3_filename file_name;
	size_t file_size = 0;
	bool is_dirty = false;
};

struct IdbFile : public SQLiteFileImpl {
	sqlite3_filename file_name;
	IdbFileSize file_size;
	std::vector<uint8_t> journal_data;
	bool is_db;

	IdbFile() {}
	IdbFile(sqlite3_filename file_name, bool is_db) : file_name(file_name), file_size(file_name), is_db(is_db) {}

	int iVersion() const override {
		return 1;
	}

	int xClose() override {
		return SQLITE_OK;
	}

	int xRead(void *p, int iAmt, sqlite3_int64 iOfst) override {
		TRACE_LOG("READ %s %d @ %ld", file_name, iAmt, iOfst);
		if (iAmt + iOfst > file_size.get()) {
			TRACE_LOG("  > %d", false);
			return SQLITE_IOERR_SHORT_READ;
		}

		int result;
		if (is_db) {
			result = readDb(p, iAmt, iOfst);
		}
		else {
			result = readJournal(p, iAmt, iOfst);
		}
		TRACE_LOG("  > %d", result);
		return result;
	}

	int xWrite(const void *p, int iAmt, sqlite3_int64 iOfst) override {
		TRACE_LOG("WRITE %s %d @ %ld", file_name, iAmt, iOfst);
		int result;
		if (is_db) {
			result = writeDb(p, iAmt, iOfst);
		}
		else {
			result = writeJournal(p, iAmt, iOfst);
		}
		TRACE_LOG("  > %d", result);
		return result;
	}

	int xTruncate(sqlite3_int64 size) override {
		TRACE_LOG("TRUNCATE %s to %ld", file_name, size);
		file_size.set(size);
		TRACE_LOG("  > %d", true);
		return SQLITE_OK;
	}

	int xSync(int flags) override {
		TRACE_LOG("SYNC %s %d", file_name, flags);
		// journal data is stored in-memory and synced all at once
		if (!journal_data.empty()) {
			IdbPage file(file_name, 0);
			file.store(journal_data);
			file_size.set(journal_data.size());
		}
		bool success = file_size.sync();
		TRACE_LOG("  > %d", success);
		return success ? SQLITE_OK : SQLITE_IOERR_FSYNC;
	}

	int xFileSize(sqlite3_int64 *pSize) override {
		TRACE_LOG("FILE SIZE %s", file_name);
		if (!journal_data.empty()) {
			*pSize = journal_data.size();
		}
		else {
			*pSize = file_size.get();
		}
		TRACE_LOG("  > %d", *pSize);
		return SQLITE_OK;
	}

	int xLock(int flags) override {
		return SQLITE_OK;
	}

	int xUnlock(int flags) override {
		return SQLITE_OK;
	}

	int xCheckReservedLock(int *pResOut) override {
		*pResOut = 0;
		return SQLITE_OK;
	}

	int xFileControl(int op, void *pArg) override {
		switch (op) {
			case SQLITE_FCNTL_VFSNAME:
				*(char **) pArg = sqlite3_mprintf("%z", IDBVFS_NAME);
				return SQLITE_OK;
		}
		return SQLITE_NOTFOUND;
	}

	int xSectorSize() override {
		return DISK_SECTOR_SIZE;
	}

	int xDeviceCharacteristics() override {
		return 0;
	}

private:
	int readDb(void *p, int iAmt, sqlite3_int64 iOfst) {
		int page_number;
		sqlite3_int64 offset_in_page;
		if (iOfst + iAmt >= 512) {
			if (iOfst % iAmt != 0) {
				return SQLITE_IOERR_READ;
			}
			page_number = iOfst / iAmt;
			offset_in_page = 0;
		} else {
			page_number = 0;
			offset_in_page = iOfst;
		}

		IdbPage page(file_name, page_number);
		int loaded_bytes = page.load_into((uint8_t*) p, iAmt, offset_in_page);
		if (loaded_bytes < iAmt) {
			return SQLITE_IOERR_SHORT_READ;
		}
		else {
			return SQLITE_OK;
		}
	}

	int readJournal(void *p, int iAmt, sqlite3_int64 iOfst) {
		if (journal_data.empty()) {
			size_t journal_size = file_size.get();
			if (journal_size > 0) {
				IdbPage page(file_name, 0);
				page.load_into(journal_data);
			}
		}
		if (iAmt + iOfst > journal_data.size()) {
			return SQLITE_IOERR_SHORT_READ;
		}
		memcpy(p, journal_data.data() + iOfst, iAmt);
		return SQLITE_OK;
	}

	int writeDb(const void *p, int iAmt, sqlite3_int64 iOfst) {
		int page_number = iOfst ? iOfst / iAmt : 0;

		IdbPage page(file_name, page_number);
		int stored_bytes = page.store(p, iAmt);
		if (stored_bytes < iAmt) {
			return SQLITE_IOERR_WRITE;
		}

		file_size.update_if_greater(iAmt + iOfst);
		return SQLITE_OK;
	}

	int writeJournal(const void *p, int iAmt, sqlite3_int64 iOfst) {
		if (iAmt + iOfst > journal_data.size()) {
			journal_data.resize(iAmt + iOfst);
		}
		memcpy(journal_data.data() + iOfst, p, iAmt);
		return SQLITE_OK;
	}
};

struct IdbVfs : public SQLiteVfsImpl<IdbFile> {
	int xOpen(sqlite3_filename zName, SQLiteFile<IdbFile> *file, int flags, int *pOutFlags) override {
		TRACE_LOG("OPEN %s", zName);
		bool is_db = (flags & SQLITE_OPEN_MAIN_DB) || (flags & SQLITE_OPEN_TEMP_DB);
		file->implementation = IdbFile(zName, is_db);
		return SQLITE_OK;
	}

	int xDelete(const char *zName, int syncDir) override {
		TRACE_LOG("DELETE %s", zName);
		int error;
		emscripten_idb_delete(zName, IDBVFS_SIZE_KEY, &error);
		for (int i = 0; ; i++) {
			IdbPage page(zName, i);
			if (!page.remove()) {
				break;
			}
		}
		TRACE_LOG("  > %d", !error);
		return error ? SQLITE_IOERR_DELETE : SQLITE_OK;
	}

	int xAccess(const char *zName, int flags, int *pResOut) override {
		TRACE_LOG("ACCESS %s %d", zName, flags);
		switch (flags) {
			case SQLITE_ACCESS_EXISTS:
			case SQLITE_ACCESS_READWRITE:
			case SQLITE_ACCESS_READ:
				IdbFileSize file_size(zName, false);
				*pResOut = file_size.exists();
				TRACE_LOG("  > %d", *pResOut);
				return SQLITE_OK;
		}
		return SQLITE_NOTFOUND;
	}
};

extern "C" {
	const char *IDBVFS_NAME = "idbvfs";

	int idbvfs_register(int makeDefault) {
		static SQLiteVfs<IdbVfs> idbvfs(IDBVFS_NAME);
		return idbvfs.register_vfs(makeDefault);
	}
}
