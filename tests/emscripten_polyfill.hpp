#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

static char _idbvfs_file_name_buffer[1024];

inline void emscripten_idb_exists(const char *db_name, const char *file_id, int* pexists, int *perror) {
	snprintf(_idbvfs_file_name_buffer, sizeof(_idbvfs_file_name_buffer), "%s/%s", db_name, file_id);
	*pexists = stat(_idbvfs_file_name_buffer, nullptr) == 0;
}

inline void emscripten_idb_load(const char *db_name, const char *file_id, void** pbuffer, int* pnum, int *perror) {
	snprintf(_idbvfs_file_name_buffer, sizeof(_idbvfs_file_name_buffer), "%s/%s", db_name, file_id);
	FILE *f = fopen(_idbvfs_file_name_buffer, "rb");
	if (!f) {
		*perror = 1;
		return;
	}

	fseek(f, 0, SEEK_END);
	size_t file_size = ftell(f);
	*pbuffer = malloc(file_size);
	if (*pbuffer) {
		fseek(f, 0, SEEK_SET);
		*pnum = fread(*pbuffer, 1, file_size, f);
	}
	else {
		*perror = 1;
	}
	fclose(f);
}

inline void emscripten_idb_store(const char *db_name, const char *file_id, void* buffer, int num, int *perror) {
	mkdir(db_name, 0777);

	snprintf(_idbvfs_file_name_buffer, sizeof(_idbvfs_file_name_buffer), "%s/%s", db_name, file_id);
	FILE *f = fopen(_idbvfs_file_name_buffer, "wb");
	if (!f) {
		*perror = 1;
		return;
	}

	size_t bytes_written = fwrite(buffer, 1, num, f);
	if (bytes_written < num) {
		*perror = 1;
	}
	fclose(f);
}

inline void emscripten_idb_delete(const char *db_name, const char *file_id, int *perror) {
	snprintf(_idbvfs_file_name_buffer, sizeof(_idbvfs_file_name_buffer), "%s/%s", db_name, file_id);
	*perror = remove(_idbvfs_file_name_buffer) != 0;
}
