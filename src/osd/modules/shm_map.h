#ifndef SHM_MAP_H
#define SHM_MAP_H

#include "shmbuffer.h"
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>

// create a read/write shm mmap buffer.  This side goes in mame to allow it to
// write out video frames.  Only the creator will maintain the fd for it's lifetime.
struct shmbuf* createShmBuffer(const char * shm_path, size_t length, bool read_only, int frame_id, int width, int height)
{
    shm_unlink(shm_path);
    int fd = shm_open(shm_path, O_CREAT | O_EXCL | O_RDWR,
                      S_IRUSR | S_IWUSR);

    if (fd == -1)
    {
        return NULL;
    }

    if (ftruncate(fd, length) == -1)
    {
        shm_unlink(shm_path);
        return NULL;
    }

    // fixate length to page boundries
    size_t pageSize = sysconf(_SC_PAGESIZE);
    size_t pageCount = (length + pageSize - 1) / pageSize;
    char * shmp = (char*)mmap(NULL, pageSize * pageCount,
                               read_only ? PROT_READ : PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);

    if (shmp == MAP_FAILED)
    {
        shm_unlink(shm_path);
        return NULL;
    }

    struct shmbuf* retv = (struct shmbuf*)malloc(sizeof (struct shmbuf));
    retv->fd = fd;
    retv->data = shmp;
    retv->buffer_size = length;
    retv->read_only = read_only;
    strncpy(retv->shm_path, shm_path, 1024);
    retv->pageSize = pageSize;
    retv->pageCount = pageCount;
    retv->frame_id = frame_id;
    retv->width = width;
    retv->height = height;
    return retv;
}

struct shmbuf* openShmBuffer(const char * shm_path, size_t length, bool read_only, int frame_id, int width, int height)
{
    int fd = shm_open(shm_path, read_only ? O_RDONLY : O_RDWR, 0);
    if (fd == -1)
    {
        return NULL;
    }

    size_t pageSize = sysconf(_SC_PAGESIZE);
    size_t pageCount = (length + pageSize - 1) / pageSize;
    char * shmp = (char*)mmap(NULL, pageSize * pageCount,
                               read_only ? PROT_READ : PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);

    // we didn't create this fd, so we'll close ours now because it's no longer needed
    shm_unlink(shm_path);
    if (shmp == MAP_FAILED)
    {
        return NULL;
    }

    struct shmbuf* retv = (struct shmbuf*)malloc(sizeof (struct shmbuf));
    retv->fd = -1;
    retv->data = shmp;
    retv->buffer_size = length;
    retv->read_only = read_only;
    strncpy(retv->shm_path, shm_path, 1024);
    retv->pageSize = pageSize;
    retv->pageCount = pageCount;
    retv->frame_id = frame_id;
    retv->width = width;
    retv->height = height;
    return retv;
}

void deleteShmBuffer(struct shmbuf* buffer)
{
    if(buffer)
    {
        munmap(buffer->data, buffer->pageSize * buffer->pageCount);

        if(buffer->created)
        {
            shm_unlink(buffer->shm_path);
        }
        buffer->data = NULL;
        free(buffer);
    }
}

#endif // SHM_MAP_H
