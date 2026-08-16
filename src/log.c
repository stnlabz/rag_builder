#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"

static rb_log_result_t rb_log_parse_level(
    const char* level,
    rb_log_level_t* result
)
{
    if (level == NULL || result == NULL)
    {
        return RB_LOG_ERR_INVALID_ARGUMENT;
    }

    if (strcmp(level, "DEBUG") == 0)
    {
        *result = RB_LOG_DEBUG;
        return RB_LOG_OK;
    }

    if (strcmp(level, "INFO") == 0)
    {
        *result = RB_LOG_INFO;
        return RB_LOG_OK;
    }

    if (strcmp(level, "WARN") == 0)
    {
        *result = RB_LOG_WARN;
        return RB_LOG_OK;
    }

    if (strcmp(level, "ERROR") == 0)
    {
        *result = RB_LOG_ERROR;
        return RB_LOG_OK;
    }

    return RB_LOG_ERR_INVALID_LEVEL;
}

rb_log_result_t rb_log_init(
    rb_log_t* log,
    const char* output_path,
    const char* level
)
{
    int written;
    rb_log_result_t result;

    if (log == NULL ||
        output_path == NULL ||
        level == NULL)
    {
        return RB_LOG_ERR_INVALID_ARGUMENT;
    }

    memset(
        log,
        0,
        sizeof(*log)
    );

    result = rb_log_parse_level(
        level,
        &log->minimum_level
    );

    if (result != RB_LOG_OK)
    {
        return result;
    }

    written = snprintf(
        log->path,
        sizeof(log->path),
        "%s\\rag_builder.log",
        output_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(log->path))
    {
        return RB_LOG_ERR_PATH_TOO_LONG;
    }

    log->file = fopen(
        log->path,
        "a"
    );

    if (log->file == NULL)
    {
        return RB_LOG_ERR_OPEN_FAILED;
    }

    log->initialized = 1;

    return RB_LOG_OK;
}

rb_log_result_t rb_log_write(
    rb_log_t* log,
    rb_log_level_t level,
    const char* component,
    const char* message
)
{
    time_t now;
    struct tm* local_time;

    char timestamp[32];

    int result;

    if (log == NULL ||
        component == NULL ||
        message == NULL)
    {
        return RB_LOG_ERR_INVALID_ARGUMENT;
    }

    if (!log->initialized ||
        log->file == NULL)
    {
        return RB_LOG_ERR_NOT_INITIALIZED;
    }

    if (level < log->minimum_level)
    {
        return RB_LOG_OK;
    }

    now = time(NULL);

    local_time = localtime(&now);

    if (local_time == NULL)
    {
        return RB_LOG_ERR_WRITE_FAILED;
    }

    if (strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%dT%H:%M:%S",
        local_time
    ) == 0)
    {
        return RB_LOG_ERR_WRITE_FAILED;
    }

    result = fprintf(
        log->file,
        "%s %s %s %s\n",
        timestamp,
        rb_log_level_string(level),
        component,
        message
    );

    if (result < 0)
    {
        return RB_LOG_ERR_WRITE_FAILED;
    }

    if (fflush(log->file) != 0)
    {
        return RB_LOG_ERR_WRITE_FAILED;
    }

    return RB_LOG_OK;
}

rb_log_result_t rb_log_close(
    rb_log_t* log
)
{
    if (log == NULL)
    {
        return RB_LOG_ERR_INVALID_ARGUMENT;
    }

    if (!log->initialized)
    {
        return RB_LOG_OK;
    }

    if (log->file != NULL)
    {
        if (fclose(log->file) != 0)
        {
            log->file = NULL;
            log->initialized = 0;

            return RB_LOG_ERR_WRITE_FAILED;
        }
    }

    log->file = NULL;
    log->initialized = 0;

    return RB_LOG_OK;
}

const char* rb_log_level_string(
    rb_log_level_t level
)
{
    switch (level)
    {
    case RB_LOG_DEBUG:
        return "DEBUG";

    case RB_LOG_INFO:
        return "INFO";

    case RB_LOG_WARN:
        return "WARN";

    case RB_LOG_ERROR:
        return "ERROR";

    default:
        return "UNKNOWN";
    }
}

const char* rb_log_result_string(
    rb_log_result_t result
)
{
    switch (result)
    {
    case RB_LOG_OK:
        return "OK";

    case RB_LOG_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_LOG_ERR_INVALID_LEVEL:
        return "INVALID_LEVEL";

    case RB_LOG_ERR_PATH_TOO_LONG:
        return "PATH_TOO_LONG";

    case RB_LOG_ERR_OPEN_FAILED:
        return "OPEN_FAILED";

    case RB_LOG_ERR_WRITE_FAILED:
        return "WRITE_FAILED";

    case RB_LOG_ERR_NOT_INITIALIZED:
        return "NOT_INITIALIZED";

    default:
        return "UNKNOWN";
    }
}