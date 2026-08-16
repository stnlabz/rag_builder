#ifndef RAG_BUILDER_ACL_FIXTURE_WIN_H
#define RAG_BUILDER_ACL_FIXTURE_WIN_H

int rb_test_acl_create_unreadable_directory(
    const char *path
);

int rb_test_acl_create_unwritable_directory(
    const char *path
);

int rb_test_acl_cleanup_directory(
    const char *path
);

#endif