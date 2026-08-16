/* include/config.h */

#ifndef RAG_BUILDER_CONFIG_H
#define RAG_BUILDER_CONFIG_H

#include <stddef.h>

#define RB_PATH_MAX 512
#define RB_LOG_LEVEL_MAX 32

typedef struct
{
    char source_path[RB_PATH_MAX];
    char output_path[RB_PATH_MAX];
    char log_level[RB_LOG_LEVEL_MAX];
} rb_config_t;

typedef enum
{
    RB_CONFIG_OK = 0,
    RB_CONFIG_ERR_INVALID_ARGUMENT,
    RB_CONFIG_ERR_OPEN_FAILED,
    RB_CONFIG_ERR_READ_FAILED,
    RB_CONFIG_ERR_INVALID_FORMAT,
    RB_CONFIG_ERR_MISSING_FIELD,
    RB_CONFIG_ERR_VALUE_TOO_LONG
} rb_config_result_t;

rb_config_result_t rb_config_load(
    const char *path,
    rb_config_t *config
);

rb_config_result_t rb_config_validate(
    const rb_config_t *config
);

const char *rb_config_result_string(
    rb_config_result_t result
);

#endif