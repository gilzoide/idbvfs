#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <emscripten.h>

#include <sqlite3.h>
#include <SQLiteVfs.hpp>

#define IDBVFS_NAME "idbvfs"

// SQLite file format offsets
#define DISK_SECTOR_SIZE 512

// Helper macros for byte/math operations
#define LOAD_8(p) \
	(((uint8_t *) p)[0])
#define LOAD_16_BE(p) \
	(((uint8_t *) p)[0] << 8) + (((uint8_t *) p)[1])
#define LOAD_32_BE(p) \
	(((uint8_t *) p)[0] << 24) + (((uint8_t *) p)[1] << 16) + (((uint8_t *) p)[2] << 8) + (((uint8_t *) p)[1])
#define MIN(a, b) \
	((a) < (b) ? (a) : (b))
#define MAX(a, b) \
	((a) > (b) ? (a) : (b))

using namespace sqlitevfs;
using namespace std;

// class SqliteString {
// public:
// 	SqliteString(const char *fmt, ...) {
// 		va_list args;
// 		va_start(args, fmt);
// 		str = sqlite3_vmprintf(fmt, args);
// 		va_end(args);
// 	}
// 	~SqliteString() {
// 		sqlite3_free(str);
// 	}

// 	const char *cstr() const {
// 		return str;
// 	}
// 	operator const char*() const {
// 		return cstr();
// 	}

// private:
// 	char *str;
// };

struct AutoFreePtr {
	uint8_t *ptr;

	~AutoFreePtr() {
		if (ptr) {
			free(ptr);
		}
	}

	operator void **() {
		return (void **) &ptr;
	}
};

#define LOG(js_str, ...) \
	EM_ASM({ console.log(js_str) }, ##__VA_ARGS__)

struct WasmFile : public SQLiteFileImpl {
	sqlite3_filename dbname;
	char page_name_buffer[16];

	WasmFile() {}
	WasmFile(sqlite3_filename dbname) : dbname(dbname) {}

	int iVersion() const override {
		return 1;
	}

	int xClose() override {
		return SQLITE_OK;
	}

	int xRead(void *p, int iAmt, sqlite3_int64 iOfst) override {
		LOG('READ ' + $0 + ' @ ' + $1, iAmt, (int) iOfst);

		uint8_t *buffer = (uint8_t *) p;
		int sector_index = iOfst / DISK_SECTOR_SIZE;
		int offset_in_sector = iOfst % DISK_SECTOR_SIZE;

		while (iAmt > 0) {
			AutoFreePtr idb_buffer;
			int size = 0;
			int error;
			emscripten_idb_load(dbname, sector_name(sector_index), idb_buffer, &size, &error);
			if (error) {
				return SQLITE_IOERR_SHORT_READ;
			}
			int copied_bytes = MIN(iAmt, size - offset_in_sector);
			memcpy(buffer, idb_buffer.ptr + offset_in_sector, copied_bytes);
			buffer += copied_bytes;
			offset_in_sector = 0;
			sector_index++;
		}
		return iAmt > 0 ? SQLITE_IOERR_SHORT_READ : SQLITE_OK;
	}

	int xWrite(const void *p, int iAmt, sqlite3_int64 iOfst) override {
		LOG('WRITE ' + $0 + ' @ ' + $1, iAmt, (int) iOfst);

		const uint8_t *buffer = (const uint8_t *) p;
		int sector_index = iOfst / DISK_SECTOR_SIZE;
		int offset_in_sector = iOfst % DISK_SECTOR_SIZE;

		// handle writing in the middle of a sector:
		// read current data and rewrite what's beyond the offset
		if (offset_in_sector > 0) {
			LOG('OH NO ' + $0, 21);
			return SQLITE_IOERR_WRITE;
		}

		while (iAmt > 0) {
			int error;
			int size = MIN(iAmt, DISK_SECTOR_SIZE);
			emscripten_idb_store(dbname, sector_name(sector_index), (void *) buffer, size, &error);
			if (error) {
				LOG('ERROU =/ ' + $0, 52);
				return SQLITE_IOERR_WRITE;
			}
			iAmt -= size;
			buffer += size;
			sector_index++;
		}
		return SQLITE_OK;
	}

	int xTruncate(sqlite3_int64 size) override {
		int sector_index = size / DISK_SECTOR_SIZE;
		int offset_in_sector = size % DISK_SECTOR_SIZE;

		// handle truncating in the middle of a sector:
		// read current data and rewrite only until the offset
		if (offset_in_sector > 0) {
			AutoFreePtr idb_buffer;
			int size = 0;
			int error = 0;
			const char *sector_str = sector_name(sector_index);
			emscripten_idb_load(dbname, sector_str, idb_buffer, &size, &error);
			if (!error) {
				emscripten_idb_store(dbname, sector_str, idb_buffer.ptr, offset_in_sector, &error);
			}
			sector_index++;
		}

		// now remove all the other sectors
		while (true) {
			int error = 0;
			emscripten_idb_delete(dbname, sector_name(sector_index), &error);
			if (error) {
				break;
			}
			sector_index++;
		}

		return SQLITE_ERROR;
	}

	int xSync(int flags) override {
		return SQLITE_OK;
	}

	int xFileSize(sqlite3_int64 *pSize) override {
		sqlite3_int64 total_size = 0;
		for (int i = 0; ; i++) {
			AutoFreePtr idb_buffer;
			int size = 0;
			int error = 0;
			emscripten_idb_load(dbname, sector_name(i), idb_buffer, &size, &error);
			if (error) {
				break;
			}
			total_size += size;
		}
		*pSize = total_size;
		return SQLITE_OK;
	}

	int xLock(int flags) override {
		return SQLITE_OK;
	}

	int xUnlock(int flags) override {
		return SQLITE_OK;
	}

	int xCheckReservedLock(int *pResOut) override {
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
	const char *sector_name(int index) {
		snprintf(page_name_buffer, sizeof(page_name_buffer), "%d", index);
		return page_name_buffer;
	}
};

// 3. Implement your own `SQLiteVfsImpl<>` subclass.
// Pass your SQLiteFileImpl subclass as template parameter.
// Override any methods necessary. Reference: https://www.sqlite.org/c3ref/vfs.html
// Default implementation will forward execution to the `original_vfs` passed in `SQLiteVfs` construtor.
// Notice that `xOpen` receives a `SQLiteFile<LogIOFileImpl> *` instead of `sqlite3_file`.
struct IdbVfs : public SQLiteVfsImpl<WasmFile> {
	int xOpen(sqlite3_filename zName, SQLiteFile<WasmFile> *file, int flags, int *pOutFlags) override {
		int res = SQLITE_OK;
		if (!(flags & SQLITE_OPEN_READWRITE)) {
			int exists;
			int error;
			emscripten_idb_exists(zName, "0", &exists, &error);
			if (!exists) {
				res = SQLITE_NOTFOUND;
			}
			if (error) {
				res = SQLITE_ERROR;
			}
		}
		file->setup(res, zName);
		return res;
	}

	int xDelete(const char *zName, int syncDir) override {

		return SQLITE_OK;
	}
};

extern "C" int idbvfs_register(int makeDefault) {
	static SQLiteVfs<IdbVfs> idbvfs(IDBVFS_NAME);
	return idbvfs.register_vfs(makeDefault);
}
