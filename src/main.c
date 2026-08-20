#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "config.h"
#include "core.h"

static int rb_path_is_absolute(
    const char* path
)
{
    if (path == NULL ||
        path[0] == '\0')
    {
        return 0;
    }

    /*
     * Drive-qualified Windows path:
     * C:\path
     * C:/path
     */
    if (((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/'))
    {
        return 1;
    }

    /*
     * UNC path:
     * \\server\share
     */
    if (path[0] == '\\' &&
        path[1] == '\\')
    {
        return 1;
    }

    return 0;
}


static int rb_build_path(
    const char* directory,
    const char* relative_path,
    char* output,
    size_t output_size
)
{
    size_t directory_length;
    int written;

    if (directory == NULL ||
        relative_path == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    directory_length = strlen(directory);

    if (directory_length == 0)
    {
        return 0;
    }

    if (directory[directory_length - 1] == '\\' ||
        directory[directory_length - 1] == '/')
    {
        written = snprintf(
            output,
            output_size,
            "%s%s",
            directory,
            relative_path
        );
    }
    else
    {
        written = snprintf(
            output,
            output_size,
            "%s\\%s",
            directory,
            relative_path
        );
    }

    return written > 0 &&
           (size_t)written < output_size;
}


static int rb_get_executable_directory(
    char* output,
    size_t output_size
)
{
    DWORD length;
    char* separator;

    if (output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    length = GetModuleFileNameA(
        NULL,
        output,
        (DWORD)output_size
    );

    if (length == 0 ||
        length >= output_size)
    {
        return 0;
    }

    separator = strrchr(
        output,
        '\\'
    );

    if (separator == NULL)
    {
        separator = strrchr(
            output,
            '/'
        );
    }

    if (separator == NULL)
    {
        return 0;
    }

    *separator = '\0';

    return output[0] != '\0';
}


int main(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_config_t config;
    rb_config_result_t config_result;
    rb_result_t result;

    char executable_directory[RB_PATH_MAX];
    char config_path[RB_PATH_MAX];
    char modules_path[RB_PATH_MAX];

    if (!rb_get_executable_directory(
            executable_directory,
            sizeof(executable_directory)
        ))
    {
        fprintf(
            stderr,
            "[ERR] Failed to resolve executable directory\n"
        );

        return EXIT_FAILURE;
    }

    if (!rb_build_path(
            executable_directory,
            "config\\rag_builder.conf",
            config_path,
            sizeof(config_path)
        ))
    {
        fprintf(
            stderr,
            "[ERR] Failed to resolve configuration path\n"
        );

        return EXIT_FAILURE;
    }

    config_result = rb_config_load(
        config_path,
        &config
    );

    if (config_result != RB_CONFIG_OK)
    {
        fprintf(
            stderr,
            "[ERR] Configuration load failed: %s\n",
            rb_config_result_string(config_result)
        );

        return EXIT_FAILURE;
    }

    /*
     * modules_path may remain relative in rag_builder.conf.
     *
     * Relative module paths are resolved against the directory containing
     * rag_builder.exe so the installed application behaves identically
     * regardless of the shell's current working directory.
     *
     * Absolute configured module paths are preserved unchanged.
     */
    if (!rb_path_is_absolute(
            config.modules_path
        ))
    {
        if (!rb_build_path(
                executable_directory,
                config.modules_path,
                modules_path,
                sizeof(modules_path)
            ))
        {
            fprintf(
                stderr,
                "[ERR] Failed to resolve modules path\n"
            );

            return EXIT_FAILURE;
        }

        if (snprintf(
                config.modules_path,
                sizeof(config.modules_path),
                "%s",
                modules_path
            ) < 0 ||
            strlen(modules_path) >=
                sizeof(config.modules_path))
        {
            fprintf(
                stderr,
                "[ERR] Resolved modules path is too long\n"
            );

            return EXIT_FAILURE;
        }
    }

    config_result = rb_config_validate(
        &config
    );

    if (config_result != RB_CONFIG_OK)
    {
        fprintf(
            stderr,
            "[ERR] Configuration validation failed: %s\n",
            rb_config_result_string(config_result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_init(
        &core,
        &config
    );

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core initialization failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_run(
        &core
    );

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core execution failed: %s\n",
            rb_result_string(result)
        );

        core.state = RB_CORE_STATE_FAILED;

        (void)rb_core_shutdown(
            &core
        );

        return EXIT_FAILURE;
    }

    result = rb_core_shutdown(
        &core
    );

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core shutdown failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
