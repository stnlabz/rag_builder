#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>
#include "platform.h"

#pragma comment(lib, "bcrypt.lib")

rb_platform_result_t rb_platform_validate_directory(const char* path)
{
    DWORD attributes, error;
    if (path == NULL || path[0] == '\0') return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return RB_PLATFORM_ERR_NOT_FOUND;
        return RB_PLATFORM_ERR_ACCESS;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RB_PLATFORM_OK : RB_PLATFORM_ERR_NOT_DIRECTORY;
}

rb_platform_result_t rb_platform_validate_readable_directory(const char* path)
{
    HANDLE directory;
    rb_platform_result_t result = rb_platform_validate_directory(path);
    if (result != RB_PLATFORM_OK) return result;

    directory = CreateFileA(path, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (directory == INVALID_HANDLE_VALUE) return RB_PLATFORM_ERR_NOT_READABLE;
    CloseHandle(directory);
    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_validate_writable_directory(const char* path)
{
    char probe_path[RB_PLATFORM_PATH_MAX];
    HANDLE probe_file;
    DWORD pid;
    int written;
    rb_platform_result_t result = rb_platform_validate_directory(path);
    if (result != RB_PLATFORM_OK) return result;

    pid = GetCurrentProcessId();
    written = snprintf(probe_path, sizeof(probe_path),
        "%s\\.__rag_builder_write_test_%lu.tmp", path, (unsigned long)pid);
    if (written < 0 || (size_t)written >= sizeof(probe_path))
        return RB_PLATFORM_ERR_PATH_TOO_LONG;

    probe_file = CreateFileA(probe_path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (probe_file == INVALID_HANDLE_VALUE) return RB_PLATFORM_ERR_NOT_WRITABLE;
    CloseHandle(probe_file);
    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_file_iterator_open(
    rb_platform_file_iterator_t* iterator, const char* directory, const char* pattern)
{
    WIN32_FIND_DATAA data;
    HANDLE handle;
    int written;

    if (iterator == NULL || directory == NULL || directory[0] == '\0' ||
        pattern == NULL || pattern[0] == '\0')
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;

    memset(iterator, 0, sizeof(*iterator));
    if (sprintf_s(iterator->directory, sizeof(iterator->directory), "%s", directory) < 0)
        return RB_PLATFORM_ERR_PATH_TOO_LONG;

    written = snprintf(iterator->pattern, sizeof(iterator->pattern), "%s\\%s", directory, pattern);
    if (written < 0 || (size_t)written >= sizeof(iterator->pattern))
        return RB_PLATFORM_ERR_PATH_TOO_LONG;

    handle = FindFirstFileA(iterator->pattern, &data);
    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) return RB_PLATFORM_ERR_NO_MORE_FILES;
        if (error == ERROR_PATH_NOT_FOUND) return RB_PLATFORM_ERR_NOT_FOUND;
        return RB_PLATFORM_ERR_ACCESS;
    }

    FindClose(handle);
    iterator->handle = INVALID_HANDLE_VALUE;
    iterator->started = 0;
    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_file_iterator_next(
    rb_platform_file_iterator_t* iterator, char* path, size_t path_size)
{
    WIN32_FIND_DATAA data;
    HANDLE handle;
    BOOL found;
    int written;

    if (iterator == NULL || path == NULL || path_size == 0)
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;

    if (!iterator->started)
    {
        handle = FindFirstFileA(iterator->pattern, &data);
        if (handle == INVALID_HANDLE_VALUE)
            return GetLastError() == ERROR_FILE_NOT_FOUND ? RB_PLATFORM_ERR_NO_MORE_FILES : RB_PLATFORM_ERR_ACCESS;
        iterator->handle = handle;
        iterator->started = 1;
        found = TRUE;
    }
    else
    {
        handle = (HANDLE)iterator->handle;
        if (handle == INVALID_HANDLE_VALUE || handle == NULL) return RB_PLATFORM_ERR_NO_MORE_FILES;
        found = FindNextFileA(handle, &data);
    }

    while (found)
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            written = snprintf(path, path_size, "%s\\%s", iterator->directory, data.cFileName);
            return (written < 0 || (size_t)written >= path_size)
                ? RB_PLATFORM_ERR_PATH_TOO_LONG : RB_PLATFORM_OK;
        }
        found = FindNextFileA((HANDLE)iterator->handle, &data);
    }

    return GetLastError() == ERROR_NO_MORE_FILES ? RB_PLATFORM_ERR_NO_MORE_FILES : RB_PLATFORM_ERR_ACCESS;
}

void rb_platform_file_iterator_close(rb_platform_file_iterator_t* iterator)
{
    HANDLE handle;
    if (iterator == NULL) return;
    handle = (HANDLE)iterator->handle;
    if (iterator->started && handle != NULL && handle != INVALID_HANDLE_VALUE) FindClose(handle);
    memset(iterator, 0, sizeof(*iterator));
}

rb_platform_result_t rb_platform_read_file(
    const char* path, char* buffer, size_t buffer_size, size_t* bytes_read)
{
    FILE* file = NULL;
    long file_size;
    size_t read_count;

    if (path == NULL || path[0] == '\0' || buffer == NULL || buffer_size == 0 || bytes_read == NULL)
        return RB_PLATFORM_ERR_INVALID_ARGUMENT;
    *bytes_read = 0;

    if (fopen_s(&file, path, "rb") != 0 || file == NULL) return RB_PLATFORM_ERR_OPEN_FAILED;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return RB_PLATFORM_ERR_READ_FAILED; }
    file_size = ftell(file);
    if (file_size < 0) { fclose(file); return RB_PLATFORM_ERR_READ_FAILED; }
    if ((size_t)file_size >= buffer_size) { fclose(file); return RB_PLATFORM_ERR_BUFFER_TOO_SMALL; }
    if (fseek(file, 0, SEEK_SET) != 0) { fclose(file); return RB_PLATFORM_ERR_READ_FAILED; }

    read_count = fread(buffer, 1, (size_t)file_size, file);
    if (read_count != (size_t)file_size) { fclose(file); return RB_PLATFORM_ERR_READ_FAILED; }
    buffer[read_count] = '\0';
    *bytes_read = read_count;
    fclose(file);
    return RB_PLATFORM_OK;
}

rb_platform_result_t rb_platform_sha256_file(const char* path, char* hex, size_t hex_size)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    PUCHAR object = NULL;
    DWORD object_length = 0, cb = 0, bytes_read = 0;
    unsigned char digest[32], buffer[8192];
    NTSTATUS status;
    size_t i;

    if (path == NULL || hex == NULL || hex_size < 65) return RB_PLATFORM_ERR_INVALID_ARGUMENT;

    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status < 0) goto fail;

    status = BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_length,
                               sizeof(object_length), &cb, 0);
    if (status < 0) goto fail;

    object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_length);
    if (object == NULL) goto fail;

    status = BCryptCreateHash(alg, &hash, object, object_length, NULL, 0, 0);
    if (status < 0) goto fail;

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto fail;

    for (;;)
    {
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL)) goto fail;
        if (bytes_read == 0) break;
        status = BCryptHashData(hash, buffer, bytes_read, 0);
        if (status < 0) goto fail;
    }

    status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
    if (status < 0) goto fail;

    for (i = 0; i < sizeof(digest); i++)
        sprintf_s(hex + (i * 2), hex_size - (i * 2), "%02x", digest[i]);
    hex[64] = '\0';

    CloseHandle(file);
    BCryptDestroyHash(hash);
    HeapFree(GetProcessHeap(), 0, object);
    BCryptCloseAlgorithmProvider(alg, 0);
    return RB_PLATFORM_OK;

fail:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash != NULL) BCryptDestroyHash(hash);
    if (object != NULL) HeapFree(GetProcessHeap(), 0, object);
    if (alg != NULL) BCryptCloseAlgorithmProvider(alg, 0);
    return RB_PLATFORM_ERR_HASH_FAILED;
}

const char* rb_platform_result_string(rb_platform_result_t result)
{
    switch (result)
    {
    case RB_PLATFORM_OK: return "OK";
    case RB_PLATFORM_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case RB_PLATFORM_ERR_NOT_FOUND: return "NOT_FOUND";
    case RB_PLATFORM_ERR_NOT_DIRECTORY: return "NOT_DIRECTORY";
    case RB_PLATFORM_ERR_ACCESS: return "ACCESS_ERROR";
    case RB_PLATFORM_ERR_NOT_READABLE: return "NOT_READABLE";
    case RB_PLATFORM_ERR_NOT_WRITABLE: return "NOT_WRITABLE";
    case RB_PLATFORM_ERR_PATH_TOO_LONG: return "PATH_TOO_LONG";
    case RB_PLATFORM_ERR_OPEN_FAILED: return "OPEN_FAILED";
    case RB_PLATFORM_ERR_READ_FAILED: return "READ_FAILED";
    case RB_PLATFORM_ERR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    case RB_PLATFORM_ERR_NO_MORE_FILES: return "NO_MORE_FILES";
    case RB_PLATFORM_ERR_HASH_FAILED: return "HASH_FAILED";
    default: return "UNKNOWN";
    }
}
