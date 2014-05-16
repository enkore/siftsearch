

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

struct db {
    int fd;
};
typedef struct db* DB;

struct db_record {
    ssize_t key_size;
    ssize_t value_size;
    // char key[];
    // char value[];
};

#define DB_WRITE (O_RDWR|O_APPEND)
#define DB_CREATE O_CREAT
#define DB_TRUNCATE O_TRUNC

/**
 * Open or create a database file
 *
 * @param file path to db file
 * @param flags combination of DB_WRITE, DB_CREATE or DB_TRUNCATE. 0 means read-only.
 */
DB db_open(const char *file, int flags);

/**
 * Close database
 */
void db_close(DB db);

/**
 * Append a record to the database
 *
 * @return number of records written (0 on failure, 1 on success)
 */
int db_append(DB db, const char *key, const char *value, size_t size);

/**
 * Read record indicated by index and advance index to next record
 *
 * Index of first record is always 0.
 *
 * key and value are part of one memory region starting at key, the
 * caller only needs to free key. Freeing both key and value leads to
 * a double free.
 *
 * @param size if not 0, will hold size of value
 *
 * @return number of records read (0 on failure, 1 on success)
 */
int db_next(DB db, off_t *index, char **key, char **value, size_t *size);
