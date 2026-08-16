#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "platform.h"


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


rb_platform_result_t rb_platform_file_iterator_open(
    rb_platform_file_iterator_t* iterator,
    const char* directory,
    const char* pattern
)
{
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    int written;

    if (iterator == NULL ||
        directory == NULL ||
        directory[0] == '\0' ||
        pattern == NULL ||
        pattern[0] == '\0')
    {
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    }

    memset(
        iterator,
        0,
        sizeof(*iterator)
    );

    written = snprintf(
        iterator->directory,
        sizeof(iterator->directory),
        "%s",
        directory
    );

    if (written < 0 ||
        (size_t)written >= sizeof(iterator->directory))
    {
        return RB_PLATFORM_ERR_PATH_TOO_LONG;
    }

    written = snprintf(
        iterator->pattern,
        sizeof(iterator->pattern),
        "%s\\%s",
        directory,
        pattern
    );

    if (written < 0 ||
        (size_t)written >= sizeof(iterator->pattern))
    {
        return RB_PLATFORM_ERR_PATH_TOO_LONG;
    }

    handle = FindFirstFileA(
        iterator->pattern,
        &find_data
    );

    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();

        if (error == ERROR_FILE_NOT_FOUND)
        {
            return RB_PLATFORM_ERR_NO_MORE_FILES;
        }

        if (error == ERROR_PATH_NOT_FOUND)
        {
            return RB_PLATFORM_ERR_NOT_FOUND;
        }

        return RB_PLATFORM_ERR_ACCESS;
    }

    /*
     * We deliberately close this first probe.
     *
     * next() owns enumeration from the beginning.
     * This keeps iterator behavior deterministic
     * and avoids storing WIN32_FIND_DATA in the
     * public platform structure.
     */
    FindClose(handle);

    iterator->handle =
        INVALID_HANDLE_VALUE;

    iterator->started =
        0;

    return RB_PLATFORM_OK;
}


rb_platform_result_t rb_platform_file_iterator_next(
    rb_platform_file_iterator_t* iterator,
    char* path,
    size_t path_size
)
{
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    BOOL found;
    int written;

    if (iterator == NULL ||
        path == NULL ||
        path_size == 0)
    {
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    }

    if (!iterator->started)
    {
        handle = FindFirstFileA(
            iterator->pattern,
            &find_data
        );

        if (handle == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();

            if (error == ERROR_FILE_NOT_FOUND)
            {
                return RB_PLATFORM_ERR_NO_MORE_FILES;
            }

            return RB_PLATFORM_ERR_ACCESS;
        }

        iterator->handle =
            handle;

        iterator->started =
            1;

        found = TRUE;
    }
    else
    {
        handle =
            (HANDLE)iterator->handle;

        if (handle == INVALID_HANDLE_VALUE ||
            handle == NULL)
        {
            return RB_PLATFORM_ERR_NO_MORE_FILES;
        }

        found = FindNextFileA(
            handle,
            &find_data
        );
    }

    while (found)
    {
        if ((find_data.dwFileAttributes &
             FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            written = snprintf(
                path,
                path_size,
                "%s\\%s",
                iterator->directory,
                find_data.cFileName
            );

            if (written < 0 ||
                (size_t)written >= path_size)
            {
                return RB_PLATFORM_ERR_PATH_TOO_LONG;
            }

            return RB_PLATFORM_OK;
        }

        found = FindNextFileA(
            (HANDLE)iterator->handle,
            &find_data
        );
    }

    if (GetLastError() == ERROR_NO_MORE_FILES)
    {
        return RB_PLATFORM_ERR_NO_MORE_FILES;
    }

    return RB_PLATFORM_ERR_ACCESS;
}


void rb_platform_file_iterator_close(
    rb_platform_file_iterator_t* iterator
)
{
    HANDLE handle;

    if (iterator == NULL)
    {
        return;
    }

    handle =
        (HANDLE)iterator->handle;

    if (iterator->started &&
        handle != NULL &&
        handle != INVALID_HANDLE_VALUE)
    {
        FindClose(handle);
    }

    memset(
        iterator,
        0,
        sizeof(*iterator)
    );
}


rb_platform_result_t rb_platform_read_file(
    const char* path,
    char* buffer,
    size_t buffer_size,
    size_t* bytes_read
)
{
    FILE* file;
    long file_size;
    size_t read_count;

    if (path == NULL ||
        path[0] == '\0' ||
        buffer == NULL ||
        buffer_size == 0 ||
        bytes_read == NULL)
    {
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    }

    *bytes_read = 0;

    file = fopen(
        path,
        "rb"
    );

    if (file == NULL)
    {
        return RB_PLATFORM_ERR_OPEN_FAILED;
    }

    if (fseek(
        file,
        0,
        SEEK_END
    ) != 0)
    {
        fclose(file);

        return RB_PLATFORM_ERR_READ_FAILED;
    }

    file_size = ftell(file);

    if (file_size < 0)
    {
        fclose(file);

        return RB_PLATFORM_ERR_READ_FAILED;
    }

    if ((size_t)file_size >= buffer_size)
    {
        fclose(file);

        return RB_PLATFORM_ERR_BUFFER_TOO_SMALL;
    }

    if (fseek(
        file,
        0,
        SEEK_SET
    ) != 0)
    {
        fclose(file);

        return RB_PLATFORM_ERR_READ_FAILED;
    }

    read_count = fread(
        buffer,
        1,
        (size_t)file_size,
        file
    );

    if (read_count !=
        (size_t)file_size)
    {
        fclose(file);

        return RB_PLATFORM_ERR_READ_FAILED;
    }

    buffer[read_count] = '\0';

    *bytes_read =
        read_count;

    fclose(file);

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

    case RB_PLATFORM_ERR_OPEN_FAILED:
        return "OPEN_FAILED";

    case RB_PLATFORM_ERR_READ_FAILED:
        return "READ_FAILED";

    case RB_PLATFORM_ERR_BUFFER_TOO_SMALL:
        return "BUFFER_TOO_SMALL";

    case RB_PLATFORM_ERR_NO_MORE_FILES:
        return "NO_MORE_FILES";

    default:
        return "UNKNOWN";
    }
}