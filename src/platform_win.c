#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "platform.h"

#define RB_PLATFORM_PATH_MAX 1024

rb_platform_result_t rb_platform_validate_directory(
    const char* path
)
{
    DWORD attributes;
    DWORD error;

    if (path == NULL || path[0] == '\0')
    {
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    }

    attributes = GetFileAttributesA(path);

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        error = GetLastError();

        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_PATH_NOT_FOUND)
        {
            return RB_PLATFORM_ERR_NOT_FOUND;
        }

        return RB_PLATFORM_ERR_ACCESS;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return RB_PLATFORM_ERR_NOT_DIRECTORY;
    }

    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_validate_readable_directory(
    const char* path
)
{
    HANDLE directory;
    rb_platform_result_t result;

    result = rb_platform_validate_directory(path);

    if (result != RB_PLATFORM_OK)
    {
        return result;
    }

    directory = CreateFileA(
        path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ |
        FILE_SHARE_WRITE |
        FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (directory == INVALID_HANDLE_VALUE)
    {
        return RB_PLATFORM_ERR_NOT_READABLE;
    }

    CloseHandle(directory);

    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_validate_writable_directory(
    const char* path
)
{
    char probe_path[RB_PLATFORM_PATH_MAX];
    HANDLE probe_file;
    DWORD process_id;
    int written;
    rb_platform_result_t result;

    result = rb_platform_validate_directory(path);

    if (result != RB_PLATFORM_OK)
    {
        return result;
    }

    process_id = GetCurrentProcessId();

    written = snprintf(
        probe_path,
        sizeof(probe_path),
        "%s\\.__rag_builder_write_test_%lu.tmp",
        path,
        (unsigned long)process_id
    );

    if (written < 0 ||
        (size_t)written >= sizeof(probe_path))
    {
        return RB_PLATFORM_ERR_PATH_TOO_LONG;
    }

    probe_file = CreateFileA(
        probe_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY |
        FILE_FLAG_DELETE_ON_CLOSE,
        NULL
    );

    if (probe_file == INVALID_HANDLE_VALUE)
    {
        return RB_PLATFORM_ERR_NOT_WRITABLE;
    }

    CloseHandle(probe_file);

    return RB_PLATFORM_OK;
}

const char* rb_platform_result_string(
    rb_platform_result_t result
)
{
    switch (result)
    {
    case RB_PLATFORM_OK:
        return "OK";

    case RB_PLATFORM_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_PLATFORM_ERR_NOT_FOUND:
        return "NOT_FOUND";

    case RB_PLATFORM_ERR_NOT_DIRECTORY:
        return "NOT_DIRECTORY";

    case RB_PLATFORM_ERR_ACCESS:
        return "ACCESS_ERROR";

    case RB_PLATFORM_ERR_NOT_READABLE:
        return "NOT_READABLE";

    case RB_PLATFORM_ERR_NOT_WRITABLE:
        return "NOT_WRITABLE";

    case RB_PLATFORM_ERR_PATH_TOO_LONG:
        return "PATH_TOO_LONG";

    default:
        return "UNKNOWN";
    }
}