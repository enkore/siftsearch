
/* struct db { */
/*     int fd; */
/* }; */
/* typedef struct db* DB; */

/* struct db_record { */
/*     size_t key_size; */
/*     size_t value_size; */
/*     // char key[]; */
/*     // char value[]; */
/* }; */

/* #define DB_WRITE 1 */
/* #define DB_CREATE 2 */
/* #define DB_TRUNCATE 4 */

#include <string.h>

#include "db.h"

DB db_open(const char *file, int flags)
{
    DB db = malloc(sizeof(db));

    if(!(flags & DB_WRITE))
        flags |= O_RDONLY;
    if(flags & DB_CREATE) {
        db->fd = open(file, flags, 00644);
    } else {
        db->fd = open(file, flags);
    }

    if(!db->fd) {
        free(db);
        return 0;
    }

    return db;
}

void db_close(DB db)
{
    close(db->fd);
    free(db);
}

int db_append(DB db, const char *key, const char *value, size_t size)
{
    struct db_record record;
    record.key_size = strlen(key)+1;
    record.value_size = size;

    if(write(db->fd, &record, sizeof(record)) != sizeof(record))
        return 0;
    if(write(db->fd, key, record.key_size) != record.key_size)
        return 0;
    if(write(db->fd, value, record.value_size) != record.value_size)
        return 0;
    return 1;
}

int db_next(DB db, off_t *index,  char **key, char **value, size_t *size)
{
    struct db_record record;
    ssize_t record_size;
    char *data;
    lseek(db->fd, *index, SEEK_SET);

    if(read(db->fd, &record, sizeof(record)) != sizeof(record))
        return 0;

    record_size = record.key_size + record.value_size;
    data = malloc(record_size);

    if(read(db->fd, data, record_size) != record_size) {
        free(data);
        return 0;
    }

    *key = data;
    *value = data + record.key_size;
    if(size)
	*size = record.value_size;
    *index += sizeof(record) + record_size;

    return 1;
}

