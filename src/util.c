
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <fts.h>

#include "util.h"

int has_extension(const char *path, const char *extension)
{
    const char *dot = strrchr(path, '.');
    if(!dot)
        return 0;
    return strcasecmp(dot, extension) == 0;
}

int has_valid_extension(const char *path, const char **extensions)
{
    for(const char **ext = extensions; *ext; ext++) {
        if(has_extension(path, *ext))
            return 1;
    }
    return 0;
}

/**
 * Find all files in dir ending with a extension from a list
 *
 * @param dir directory to search recursively
 * @param extensions null-terminated list of extensions (including leading dot)
 * @param cb callback, the first parameter is the path (must be freed by the callback), the second is cbdat
 * @param cbdat pointer passed to callback
 * @return zero on failure, nonzero on success
 *
 * @note cb is called asynchronously using OpenMP
 */
int enumerate_files(const char *dir, const char **extensions, void cb(char*, void*), void *cbdat)
{
    FTS *fts;
    FTSENT *ent;
    int fts_options = FTS_NOSTAT | FTS_COMFOLLOW | FTS_LOGICAL;
    char *dir_nonconst = strdup(dir);
    char * const dirs[2] = {dir_nonconst, 0};

    if(!(fts = fts_open(dirs, fts_options, 0))) {
        free(dir_nonconst);
        return 0;
    }

    #pragma omp parallel
    {
        #pragma omp single
        while((ent = fts_read(fts))) {
            if(ent->fts_info == FTS_F && has_valid_extension(ent->fts_path, extensions)) {
                char *s = strdup(ent->fts_path);
                #pragma omp task
                cb(s, cbdat);
            }
        }
    }

    fts_close(fts);
    free(dir_nonconst);

    return 1;
}
