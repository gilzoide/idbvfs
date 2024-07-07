#ifdef __cplusplus
extern "C" {
#endif

/**
 * String containing idbvfs name.
 */
extern const char *IDBVFS_NAME;

/**
 * Registers idbvfs in SQLite 3.
 *
 * @param makeDefault  Whether idbvfs will be the new default VFS.
 * @return Return value from `sqlite3_vfs_register`
 * @see https://sqlite.org/c3ref/vfs_find.html
 */
int idbvfs_register(int makeDefault);

#ifdef __cplusplus
}
#endif
