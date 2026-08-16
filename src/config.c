/* src/config.c */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>

#include "config.h"

static void rb_config_clear(rb_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    config->source_path[0] = '\0';
    config->output_path[0] = '\0';
    config->log_level[0] = '\0';
}

static rb_config_result_t rb_config_copy_value(
    char *destination,
    size_t destination_size,
    const char *value
)
{
    size_t length;

    if (destination == NULL || value == NULL || destination_size == 0)
    {
        return RB_CONFIG_ERR_INVALID_ARGUMENT;
    }

    length = strlen(value);

    if (length >= destination_size)
    {
        return RB_CONFIG_ERR_VALUE_TOO_LONG;
    }

    memcpy(destination, value, length + 1);

    return RB_CONFIG_OK;
}

static void rb_config_trim_line(char *line)
{
    size_t length;

    if (line == NULL)
    {
        return;
    }

    length = strlen(line);

    while (length > 0 &&
           (line[length - 1] == '\n' ||
            line[length - 1] == '\r'))
    {
        line[length - 1] = '\0';
        length--;
    }
}

rb_config_result_t rb_config_load(
    const char *path,
    rb_config_t *config
)
{
    FILE *file;
    char line[1024];

    int source_seen = 0;
    int output_seen = 0;
    int log_level_seen = 0;

    if (path == NULL || config == NULL)
    {
        return RB_CONFIG_ERR_INVALID_ARGUMENT;
    }

    rb_config_clear(config);

    file = fopen(path, "r");

    if (file == NULL)
    {
        return RB_CONFIG_ERR_OPEN_FAILED;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *separator;
        char *key;
        char *value;
        rb_config_result_t result;

        rb_config_trim_line(line);

        if (line[0] == '\0')
        {
            continue;
        }

        if (line[0] == '#')
        {
            continue;
        }

        separator = strchr(line, '=');

        if (separator == NULL)
        {
            fclose(file);
            return RB_CONFIG_ERR_INVALID_FORMAT;
        }

        *separator = '\0';

        key = line;
        value = separator + 1;

        if (strcmp(key, "source_path") == 0)
        {
            if (source_seen)
            {
                fclose(file);
                return RB_CONFIG_ERR_INVALID_FORMAT;
            }

            result = rb_config_copy_value(
                config->source_path,
                sizeof(config->source_path),
                value
            );

            if (result != RB_CONFIG_OK)
            {
                fclose(file);
                return result;
            }

            source_seen = 1;
        }
        else if (strcmp(key, "output_path") == 0)
        {
            if (output_seen)
            {
                fclose(file);
                return RB_CONFIG_ERR_INVALID_FORMAT;
            }

            result = rb_config_copy_value(
                config->output_path,
                sizeof(config->output_path),
                value
            );

            if (result != RB_CONFIG_OK)
            {
                fclose(file);
                return result;
            }

            output_seen = 1;
        }
        else if (strcmp(key, "log_level") == 0)
        {
            if (log_level_seen)
            {
                fclose(file);
                return RB_CONFIG_ERR_INVALID_FORMAT;
            }

            result = rb_config_copy_value(
                config->log_level,
                sizeof(config->log_level),
                value
            );

            if (result != RB_CONFIG_OK)
            {
                fclose(file);
                return result;
            }

            log_level_seen = 1;
        }
        else
        {
            fclose(file);
            return RB_CONFIG_ERR_INVALID_FORMAT;
        }
    }

    if (ferror(file))
    {
        fclose(file);
        return RB_CONFIG_ERR_READ_FAILED;
    }

    fclose(file);

    if (!source_seen || !output_seen || !log_level_seen)
    {
        return RB_CONFIG_ERR_MISSING_FIELD;
    }

    printf("[CONFIG] Loaded: %s\n", path);
    printf("[CONFIG] Source: %s\n", config->source_path);
    printf("[CONFIG] Output: %s\n", config->output_path);
    printf("[CONFIG] Log level: %s\n", config->log_level);

    return RB_CONFIG_OK;
}

rb_config_result_t rb_config_validate(
    const rb_config_t *config
)
{
    if (config == NULL)
    {
        return RB_CONFIG_ERR_INVALID_ARGUMENT;
    }

    if (config->source_path[0] == '\0' ||
        config->output_path[0] == '\0' ||
        config->log_level[0] == '\0')
    {
        return RB_CONFIG_ERR_MISSING_FIELD;
    }

    if (strcmp(config->log_level, "INFO") != 0 &&
        strcmp(config->log_level, "WARN") != 0 &&
        strcmp(config->log_level, "ERROR") != 0 &&
        strcmp(config->log_level, "DEBUG") != 0)
    {
        return RB_CONFIG_ERR_INVALID_FORMAT;
    }

    return RB_CONFIG_OK;
}

const char *rb_config_result_string(
    rb_config_result_t result
)
{
    switch (result)
    {
        case RB_CONFIG_OK:
            return "OK";

        case RB_CONFIG_ERR_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";

        case RB_CONFIG_ERR_OPEN_FAILED:
            return "OPEN_FAILED";

        case RB_CONFIG_ERR_READ_FAILED:
            return "READ_FAILED";

        case RB_CONFIG_ERR_INVALID_FORMAT:
            return "INVALID_FORMAT";

        case RB_CONFIG_ERR_MISSING_FIELD:
            return "MISSING_FIELD";

        case RB_CONFIG_ERR_VALUE_TOO_LONG:
            return "VALUE_TOO_LONG";

        default:
            return "UNKNOWN";
    }
}