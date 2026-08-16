#ifndef RAG_BUILDER_MODULE_CATALOG_H
#define RAG_BUILDER_MODULE_CATALOG_H

#include <stddef.h>

#include "module.h"

#define RB_MODULE_CATALOG_MAX 32

typedef struct
{
    const rb_module_descriptor_t*
        entries[RB_MODULE_CATALOG_MAX];

    size_t count;
} rb_module_catalog_t;

void rb_module_catalog_init(
    rb_module_catalog_t* catalog
);

rb_module_result_t rb_module_catalog_register(
    rb_module_catalog_t* catalog,
    const rb_module_descriptor_t* descriptor
);

const rb_module_descriptor_t* rb_module_catalog_find(
    const rb_module_catalog_t* catalog,
    const char* module_id
);

#endif