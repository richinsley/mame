#ifndef SHMBUFFER_H
#define SHMBUFFER_H

#include <stddef.h>
#include <stdbool.h>

struct shmbuf
{
    char    shm_path[1024];
    int     fd;
    size_t  buffer_size;
    int     pageSize;
    int     pageCount;
    char *  data;
    bool    read_only;
    bool    created;
    int     frame_id;
    int     width;
    int     height;
};

#endif // SHMBUFFER_H
