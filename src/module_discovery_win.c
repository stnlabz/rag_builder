#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "module_discovery.h"

#define RB_DISCOVERY_PATH_MAX 1024
#define RB_MODULE_CONF_NAME   "module.conf"

static int rb_discovery_join_path(
    char* output,
    size_t output_size,
    const char* left,
    const char* right
)
{
    int written;

    if (output == NULL ||
        left == NULL ||
        right == NULL ||
        output_size == 0)
    {
        return 0;
    }

    written = snprintf(
        output,
        output_size,
        "%s\\%s",
        left,
        right
    );

    if (written < 0 ||
        (size_t)written >= output_size)
    {
        return 0;
    }

    return 1;
}

static int rb_discovery_read_module_id(
    const char* config_path,
    char* module_id,
    size_t module_id_size
)
{
    FILE* file;
    char line[512];
    int id_seen = 0;

    if (config_path == NULL ||
        module_id == NULL ||
        module_id_size == 0)
    {
        return 0;
    }

    file = fopen(
        config_path,
        "r"
    );

    if (file == NULL)
    {
        return 0;
    }

    while (fgets(
        line,
        sizeof(line),
        file
    ) != NULL)
    {
        char* separator;
        char* key;
        char* value;
        size_t length;

        length = strlen(line);

        while (length > 0 &&
            (line[length - 1] == '\n' ||
             line[length - 1] == '\r'))
        {
            line[length - 1] = '\0';
            length--;
        }

        if (line[0] == '\0' ||
            line[0] == '#')
        {
            continue;
        }

        separator = strchr(
            line,
            '='
        );

        if (separator == NULL)
        {
            fclose(file);
            return 0;
        }

        *separator = '\0';

        key = line;
        value = separator + 1;

        if (strcmp(key, "id") != 0)
        {
            fclose(file);
            return 0;
        }

        if (id_seen)
        {
            fclose(file);
            return 0;
        }

        length = strlen(value);

        if (length == 0 ||
            length >= module_id_size)
        {
            fclose(file);
            return 0;
        }

        memcpy(
            module_id,
            value,
            length + 1
        );

        id_seen = 1;
    }

    fclose(file);

    return id_seen;
}

rb_module_result_t rb_module_discovery_scan(
    rb_module_registry_t* registry,
    const rb_module_catalog_t* catalog,
    const char* modules_path,
    rb_module_discovery_report_t* report
)
{
    char search_path[RB_DISCOVERY_PATH_MAX];
    char directory_path[RB_DISCOVERY_PATH_MAX];
    char config_path[RB_DISCOVERY_PATH_MAX];
    char module_id[RB_MODULE_ID_MAX];

    WIN32_FIND_DATAA find_data;
    HANDLE search;

    int written;

    if (registry == NULL ||
        catalog == NULL ||
        modules_path == NULL ||
        report == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    memset(
        report,
        0,
        sizeof(*report)
    );

    written = snprintf(
        search_path,
        sizeof(search_path),
        "%s\\*",
        modules_path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(search_path))
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    search = FindFirstFileA(
        search_path,
        &find_data
    );

    if (search == INVALID_HANDLE_VALUE)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    do
    {
        const rb_module_descriptor_t*
            descriptor;

        rb_module_result_t result;

        if ((find_data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            continue;
        }

        if (strcmp(
            find_data.cFileName,
            "."
        ) == 0 ||
            strcmp(
                find_data.cFileName,
                ".."
            ) == 0)
        {
            continue;
        }

        report->directories_examined++;

        if (!rb_discovery_join_path(
            directory_path,
            sizeof(directory_path),
            modules_path,
            find_data.cFileName
        ))
        {
            report->modules_rejected++;
            continue;
        }

        if (!rb_discovery_join_path(
            config_path,
            sizeof(config_path),
            directory_path,
            RB_MODULE_CONF_NAME
        ))
        {
            report->modules_rejected++;
            continue;
        }

        if (!rb_discovery_read_module_id(
            config_path,
            module_id,
            sizeof(module_id)
        ))
        {
            /*
             * Not every directory under modules/
             * must necessarily declare a valid module.
             */
            report->modules_rejected++;
            continue;
        }

        descriptor = rb_module_catalog_find(
            catalog,
            module_id
        );

        if (descriptor == NULL)
        {
            /*
             * Filesystem declaration exists but no
             * statically linked implementation exists.
             *
             * Nothing is executed.
             */
            report->unknown_modules++;
            report->modules_rejected++;

            continue;
        }

        result = rb_module_registry_discover(
            registry,
            descriptor
        );

        if (result == RB_MODULE_ERR_DUPLICATE)
        {
            /*
             * Already known to Core.
             */
            continue;
        }

        if (result != RB_MODULE_OK)
        {
            report->modules_rejected++;
            continue;
        }

        report->modules_discovered++;

    } while (FindNextFileA(
        search,
        &find_data
    ));

    FindClose(search);

    return RB_MODULE_OK;
}