#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>

#include "acl_fixture_win.h"

#pragma comment(lib, "advapi32.lib")


static int rb_test_apply_dacl(
    const char *path,
    const char *sddl
)
{
    PSECURITY_DESCRIPTOR descriptor = NULL;
    BOOL dacl_present;
    BOOL dacl_defaulted;
    PACL dacl = NULL;
    DWORD result;

    if (path == NULL || sddl == NULL)
    {
        return 0;
    }

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
        sddl,
        SDDL_REVISION_1,
        &descriptor,
        NULL
    ))
    {
        return 0;
    }

    if (!GetSecurityDescriptorDacl(
        descriptor,
        &dacl_present,
        &dacl,
        &dacl_defaulted
    ))
    {
        LocalFree(descriptor);
        return 0;
    }

    if (!dacl_present)
    {
        LocalFree(descriptor);
        return 0;
    }

    result = SetNamedSecurityInfoA(
        (LPSTR)path,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION |
        PROTECTED_DACL_SECURITY_INFORMATION,
        NULL,
        NULL,
        dacl,
        NULL
    );

    LocalFree(descriptor);

    return result == ERROR_SUCCESS;
}


int rb_test_acl_create_unreadable_directory(
    const char *path
)
{
    if (path == NULL)
    {
        return 0;
    }

    RemoveDirectoryA(path);

    if (!CreateDirectoryA(path, NULL))
    {
        return 0;
    }

    /*
     * Deny Everyone full access.
     */
    if (!rb_test_apply_dacl(
        path,
        "D:P(D;;FA;;;WD)"
    ))
    {
        RemoveDirectoryA(path);
        return 0;
    }

    return 1;
}


int rb_test_acl_create_unwritable_directory(
    const char *path
)
{
    if (path == NULL)
    {
        return 0;
    }

    RemoveDirectoryA(path);

    if (!CreateDirectoryA(path, NULL))
    {
        return 0;
    }

    /*
     * Allow Everyone read/list.
     * Deny creation/writing inside the directory.
     */
    if (!rb_test_apply_dacl(
        path,
        "D:P"
        "(D;;FW;;;WD)"
        "(A;;FRFX;;;WD)"
    ))
    {
        RemoveDirectoryA(path);
        return 0;
    }

    return 1;
}


int rb_test_acl_cleanup_directory(
    const char *path
)
{
    /*
     * Restore broad access before removal.
     */
    if (!rb_test_apply_dacl(
        path,
        "D:P(A;;FA;;;WD)"
    ))
    {
        return 0;
    }

    return RemoveDirectoryA(path) != 0;
}