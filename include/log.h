#ifndef RAG_BUILDER_LOG_H
#define RAG_BUILDER_LOG_H

#include <stdio.h>

#define RB_LOG_PATH_MAX 1024

typedef enum
{
    RB_LOG_DEBUG = 0,
    RB_LOG_INFO,
    RB_LOG_WARN,
    RB_LOG_ERROR
} rb_log_level_t;

typedef enum
{
    RB_LOG_OK = 0,
    RB_LOG_ERR_INVALID_ARGUMENT,
    RB_LOG_ERR_INVALID_LEVEL,
    RB_LOG_ERR_PATH_TOO_LONG,
    RB_LOG_ERR_OPEN_FAILED,
    RB_LOG_ERR_WRITE_FAILED,
    RB_LOG_ERR_NOT_INITIALIZED
} rb_log_result_t;

typedef struct
{
    FILE* file;
    rb_log_level_t minimum_level;

    char path[RB_LOG_PATH_MAX];

    int initialized;
} rb_log_t;

rb_log_result_t rb_log_init(
    rb_log_t* log,
    const char* output_path,
    const char* level
);

rb_log_result_t rb_log_write(
    rb_log_t* log,
    rb_log_level_t level,
    const char* component,
    const char* message
);

rb_log_result_t rb_log_close(
    rb_log_t* log
);

const char* rb_log_level_string(
    rb_log_level_t level
);

const char* rb_log_result_string(
    rb_log_result_t result
);

#endif