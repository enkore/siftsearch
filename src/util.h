
int has_extension(const char *path, const char *extension);
int has_valid_extension(const char *path, const char **extensions);
int enumerate_files(const char *dir, const char **extensions, void cb(char*, void*), void *cbdat);
