#ifndef RAG_BUILDER_PLATFORM_H
#define RAG_BUILDER_PLATFORM_H

#include <stddef.h>

#define RB_PLATFORM_PATH_MAX 1024

typedef enum
{
    RB_PLATFORM_OK = 0,
    RB_PLATFORM_ERR_INVALID_ARGUMENT,
    RB_PLATFORM_ERR_NOT_FOUND,
    RB_PLATFORM_ERR_NOT_DIRECTORY,
    RB_PLATFORM_ERR_ACCESS,
    RB_PLATFORM_ERR_NOT_READABLE,
    RB_PLATFORM_ERR_NOT_WRITABLE,
    RB_PLATFORM_ERR_PATH_TOO_LONG,
    RB_PLATFORM_ERR_OPEN_FAILED,
    RB_PLATFORM_ERR_READ_FAILED,
    RB_PLATFORM_ERR_BUFFER_TOO_SMALL,
    RB_PLATFORM_ERR_NO_MORE_FILES

} rb_platform_result_t;

typedef struct
{
    void* handle;

    char directory[RB_PLATFORM_PATH_MAX];
    char pattern[RB_PLATFORM_PATH_MAX];

    int started;

} rb_platform_file_iterator_t;

rb_platform_result_t rb_platform_validate_directory(
    const char* path
);

rb_platform_result_t rb_platform_validate_readable_directory(
    const char* path
);

rb_platform_result_t rb_platform_validate_writable_directory(
    const char* path
);

rb_platform_result_t rb_platform_file_iterator_open(
    rb_platform_file_iterator_t* iterator,
    const char* directory,
    const char* pattern
);

rb_platform_result_t rb_platform_file_iterator_next(
    rb_platform_file_iterator_t* iterator,
    char* path,
    size_t path_size
);

void rb_platform_file_iterator_close(
    rb_platform_file_iterator_t* iterator
);

rb_platform_result_t rb_platform_read_file(
    const char* path,
    char* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

const char* rb_platform_result_string(
    rb_platform_result_t result
);

#endif