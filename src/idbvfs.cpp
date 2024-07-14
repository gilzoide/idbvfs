#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SQLiteVfs.hpp>

#include "idbvfs.h"

/// Used size for Indexed DB "disk sectors"
#define DISK_SECTOR_SIZE 512
/// Return the minimum value between `a` and `b`
#define MIN(a, b) \
	((a) < (b) ? (a) : (b))


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


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
// Polyfill used solely for automated testing.
// Do not use this VFS without emscripten on any other circumstance.
#include "../tests/emscripten_polyfill.hpp"
#endif

using namespace sqlitevfs;
using namespace std;

class IdbDiskSector {
public:
	IdbDiskSector(const char *dbname, int sector_index)
		: dbname(dbname)
	{
		snprintf(filename, sizeof(filename), "%d", sector_index);
	}

	~IdbDiskSector() {
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

	int load_into(uint8_t *data, int data_size, sqlite3_int64 offset_in_sector) {
		int sector_size = load();
		if (sector_size <= 0) {
			return 0;
		}
		int copied_bytes = MIN(data_size, sector_size - offset_in_sector);
		if (copied_bytes > 0) {
			memcpy(data, buffer + offset_in_sector, copied_bytes);
			return copied_bytes;
		}
		else {
			return 0;
		}
	}

	int store(const void *data, int data_size, sqlite3_int64 offset_in_sector) {
		// offsetted write: read, patch existing data, then write
		if (offset_in_sector > 0) {
			int sector_size = load();
			if (sector_size <= 0) {
				return 0;
			}
			int written_sector_size = MIN(offset_in_sector + data_size, DISK_SECTOR_SIZE);
			if (sector_size < written_sector_size) {
				if (void *new_buffer = realloc(buffer, written_sector_size)) {
					sector_size = written_sector_size;
					buffer = (uint8_t *) new_buffer;
				}
				else {
					return 0;
				}
			}
			int written_bytes = MIN(sector_size - offset_in_sector, data_size);
			memcpy(buffer + offset_in_sector, data, written_bytes);
			int error = 0;
			emscripten_idb_store(dbname, filename, buffer, sector_size, &error);
			return error ? 0 : written_bytes;
		}
		// write full sector: just write a full disk sector
		else if (data_size >= DISK_SECTOR_SIZE) {
			int error = 0;
			emscripten_idb_store(dbname, filename, (void *) data, DISK_SECTOR_SIZE, &error);
			return error ? 0 : DISK_SECTOR_SIZE;
		}
		// patch sector beginning: read, patch existing data if any, then write
		else {
			int sector_size = load();
			if (sector_size > data_size) {
				memcpy(buffer, data, data_size);
				int error = 0;
				emscripten_idb_store(dbname, filename, buffer, sector_size, &error);
				return error ? 0 : data_size;
			}
			else {
				int error = 0;
				emscripten_idb_store(dbname, filename, (void *) data, data_size, &error);
				return error ? 0 : data_size;
			}
		}
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

struct IdbFile : public SQLiteFileImpl {
	sqlite3_filename dbname;

	int iVersion() const override {
		return 1;
	}

	int xClose() override {
		return SQLITE_OK;
	}

	int xRead(void *p, int iAmt, sqlite3_int64 iOfst) override {
		TRACE_LOG("READ %s %d @ %ld", dbname, iAmt, iOfst);

		uint8_t *buffer = (uint8_t *) p;
		int sector_index = iOfst / DISK_SECTOR_SIZE;
		int offset_in_sector = iOfst % DISK_SECTOR_SIZE;

		while (iAmt > 0) {
			IdbDiskSector sector(dbname, sector_index);
			int loaded_bytes = sector.load_into(buffer, iAmt, offset_in_sector);
			if (loaded_bytes <= 0) {
				return SQLITE_IOERR_SHORT_READ;
			}
			buffer += loaded_bytes;
			iAmt -= loaded_bytes;
			offset_in_sector = 0;
			sector_index++;
		}
		return SQLITE_OK;
	}

	int xWrite(const void *p, int iAmt, sqlite3_int64 iOfst) override {
		TRACE_LOG("WRITE %s %d @ %ld", dbname, iAmt, iOfst);

		const uint8_t *buffer = (const uint8_t *) p;
		int sector_index = iOfst / DISK_SECTOR_SIZE;
		int offset_in_sector = iOfst % DISK_SECTOR_SIZE;

		while (iAmt > 0) {
			IdbDiskSector sector(dbname, sector_index);
			int written_bytes = sector.store(buffer, iAmt, offset_in_sector);
			if (written_bytes <= 0) {
				return SQLITE_IOERR_WRITE;
			}
			buffer += written_bytes;
			iAmt -= written_bytes;
			offset_in_sector = 0;
			sector_index++;
		}
		return SQLITE_OK;
	}

	int xTruncate(sqlite3_int64 size) override {
		TRACE_LOG("TRUNCATE %s to %ld", size);
		int sector_index = size / DISK_SECTOR_SIZE;
		int offset_in_sector = size % DISK_SECTOR_SIZE;

		// handle truncating in the middle of a sector:
		// read current data and rewrite only until the offset
		if (offset_in_sector > 0) {
			IdbDiskSector sector(dbname, sector_index);
			sector.truncate(offset_in_sector);
			sector_index++;
		}

		// now remove all the other sectors
		while (true) {
			IdbDiskSector sector(dbname, sector_index);
			if (!sector.remove()) {
				break;
			}
			sector_index++;
		}

		return SQLITE_OK;
	}

	int xSync(int flags) override {
		return SQLITE_OK;
	}

	int xFileSize(sqlite3_int64 *pSize) override {
		sqlite3_int64 total_size = 0;
		for (int sector_index = 0; ; sector_index++) {
			IdbDiskSector sector(dbname, sector_index);
			int size = sector.load();
			if (size <= 0) {
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
};

struct IdbVfs : public SQLiteVfsImpl<IdbFile> {
	int xOpen(sqlite3_filename zName, SQLiteFile<IdbFile> *file, int flags, int *pOutFlags) override {
		TRACE_LOG("OPEN %s", zName);
		int res = SQLITE_OK;
		if (!(flags & SQLITE_OPEN_READWRITE)) {
			IdbDiskSector first_sector(zName, 0);
			if (!first_sector.exists()) {
				res = SQLITE_NOTFOUND;
			}
		}
		file->implementation.dbname = zName;
		return res;
	}

	int xDelete(const char *zName, int syncDir) override {
		TRACE_LOG("DELETE %s", zName);
		for (int sector_index = 0; ; sector_index++) {
			IdbDiskSector sector(zName, sector_index);
			if (!sector.remove()) {
				break;
			}
		}
		return SQLITE_OK;
	}

	int xAccess(const char *zName, int flags, int *pResOut) override {
		TRACE_LOG("ACCESS %s %d", zName, flags);
		switch (flags) {
			case SQLITE_ACCESS_EXISTS:
			case SQLITE_ACCESS_READWRITE:
			case SQLITE_ACCESS_READ:
				IdbDiskSector first_sector(zName, 0);
				*pResOut = first_sector.exists();
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
