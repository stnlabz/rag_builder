#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "module_discovery.h"
#include "platform.h"

#define RB_DISCOVERY_PATH_MAX 1024
#define RB_MODULE_CONF_NAME "module.conf"
#define RB_MODULE_DESCRIPTOR_EXPORT "rb_module_get_descriptor"

typedef struct
{
    char id[RB_MODULE_ID_MAX];
    char binary[RB_MODULE_PATH_MAX];
} rb_module_declaration_t;

static int rb_join_path(char* out, size_t size, const char* left, const char* right)
{
    int written;
    if (out == NULL || left == NULL || right == NULL || size == 0) return 0;
    written = snprintf(out, size, "%s\\%s", left, right);
    return written >= 0 && (size_t)written < size;
}

static int rb_read_declaration(const char* path, rb_module_declaration_t* declaration)
{
    FILE* file = NULL;
    char line[512];
    int id_seen = 0, binary_seen = 0;

    if (path == NULL || declaration == NULL) return 0;
    memset(declaration, 0, sizeof(*declaration));

    if (fopen_s(&file, path, "r") != 0 || file == NULL) return 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char* separator;
        char* key;
        char* value;
        size_t length = strlen(line);

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';

        if (line[0] == '\0' || line[0] == '#') continue;

        separator = strchr(line, '=');
        if (separator == NULL) { fclose(file); return 0; }
        *separator = '\0';
        key = line;
        value = separator + 1;

        if (strcmp(key, "id") == 0)
        {
            if (id_seen || value[0] == '\0' || strlen(value) >= sizeof(declaration->id))
            { fclose(file); return 0; }
            strcpy_s(declaration->id, sizeof(declaration->id), value);
            id_seen = 1;
        }
        else if (strcmp(key, "binary") == 0)
        {
            if (binary_seen || value[0] == '\0' || strlen(value) >= sizeof(declaration->binary))
            { fclose(file); return 0; }
            strcpy_s(declaration->binary, sizeof(declaration->binary), value);
            binary_seen = 1;
        }
        else
        {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return id_seen && binary_seen;
}

rb_module_result_t rb_module_discovery_scan(
    rb_module_registry_t* registry,
    const char* modules_path,
    rb_module_discovery_report_t* report)
{
    char search_path[RB_DISCOVERY_PATH_MAX];
    WIN32_FIND_DATAA find_data;
    HANDLE search;

    if (registry == NULL || modules_path == NULL || report == NULL)
        return RB_MODULE_ERR_INVALID_ARGUMENT;

    memset(report, 0, sizeof(*report));
    if (snprintf(search_path, sizeof(search_path), "%s\\*", modules_path) < 0)
        return RB_MODULE_ERR_INVALID_ARGUMENT;

    search = FindFirstFileA(search_path, &find_data);
    if (search == INVALID_HANDLE_VALUE) return RB_MODULE_ERR_NOT_FOUND;

    do
    {
        char directory_path[RB_DISCOVERY_PATH_MAX];
        char config_path[RB_DISCOVERY_PATH_MAX];
        char binary_path[RB_DISCOVERY_PATH_MAX];
        char sha256[RB_MODULE_SHA256_HEX];
        rb_module_declaration_t declaration;
        HMODULE library;
        rb_module_get_descriptor_fn get_descriptor;
        const rb_module_descriptor_t* descriptor;
        rb_module_result_t result;

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;

        report->directories_examined++;

        if (!rb_join_path(directory_path, sizeof(directory_path), modules_path, find_data.cFileName) ||
            !rb_join_path(config_path, sizeof(config_path), directory_path, RB_MODULE_CONF_NAME) ||
            !rb_read_declaration(config_path, &declaration) ||
            !rb_join_path(binary_path, sizeof(binary_path), directory_path, declaration.binary))
        {
            report->modules_rejected++;
            continue;
        }

        if (rb_platform_sha256_file(binary_path, sha256, sizeof(sha256)) != RB_PLATFORM_OK)
        {
            report->modules_rejected++;
            continue;
        }

        library = LoadLibraryA(binary_path);
        if (library == NULL)
        {
            report->modules_rejected++;
            continue;
        }

        get_descriptor = (rb_module_get_descriptor_fn)GetProcAddress(library, RB_MODULE_DESCRIPTOR_EXPORT);
        if (get_descriptor == NULL)
        {
            FreeLibrary(library);
            report->modules_rejected++;
            continue;
        }

        descriptor = get_descriptor();
        if (descriptor == NULL || strcmp(declaration.id, descriptor->id) != 0)
        {
            FreeLibrary(library);
            report->modules_rejected++;
            continue;
        }

        result = rb_module_registry_discover(
            registry, descriptor, library, binary_path, sha256);

        if (result == RB_MODULE_ERR_DUPLICATE)
        {
            FreeLibrary(library);
            continue;
        }

        if (result != RB_MODULE_OK)
        {
            FreeLibrary(library);
            report->modules_rejected++;
            continue;
        }

        report->modules_discovered++;

    } while (FindNextFileA(search, &find_data));

    FindClose(search);
    return RB_MODULE_OK;
}
