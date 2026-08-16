#ifndef RAG_BUILDER_PLATFORM_H
#define RAG_BUILDER_PLATFORM_H

typedef enum
{
    RB_PLATFORM_OK = 0,
    RB_PLATFORM_ERR_INVALID_ARGUMENT,
    RB_PLATFORM_ERR_NOT_FOUND,
    RB_PLATFORM_ERR_NOT_DIRECTORY,
    RB_PLATFORM_ERR_ACCESS,
    RB_PLATFORM_ERR_NOT_READABLE,
    RB_PLATFORM_ERR_NOT_WRITABLE,
    RB_PLATFORM_ERR_PATH_TOO_LONG
} rb_platform_result_t;

rb_platform_result_t rb_platform_validate_directory(
    const char* path
);

rb_platform_result_t rb_platform_validate_readable_directory(
    const char* path
);

rb_platform_result_t rb_platform_validate_writable_directory(
    const char* path
);

const char* rb_platform_result_string(
    rb_platform_result_t result
);

#endif