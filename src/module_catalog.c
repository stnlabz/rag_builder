#include <string.h>

#include "module_catalog.h"

void rb_module_catalog_init(
    rb_module_catalog_t* catalog
)
{
    if (catalog == NULL)
    {
        return;
    }

    memset(
        catalog,
        0,
        sizeof(*catalog)
    );
}

rb_module_result_t rb_module_catalog_register(
    rb_module_catalog_t* catalog,
    const rb_module_descriptor_t* descriptor
)
{
    size_t index;

    if (catalog == NULL ||
        descriptor == NULL ||
        descriptor->id[0] == '\0' ||
        descriptor->name[0] == '\0' ||
        descriptor->qualify == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    for (index = 0;
         index < catalog->count;
         index++)
    {
        if (strcmp(
            catalog->entries[index]->id,
            descriptor->id
        ) == 0)
        {
            return RB_MODULE_ERR_DUPLICATE;
        }
    }

    if (catalog->count >=
        RB_MODULE_CATALOG_MAX)
    {
        return RB_MODULE_ERR_REGISTRY_FULL;
    }

    catalog->entries[
        catalog->count
    ] = descriptor;

    catalog->count++;

    return RB_MODULE_OK;
}

const rb_module_descriptor_t* rb_module_catalog_find(
    const rb_module_catalog_t* catalog,
    const char* module_id
)
{
    size_t index;

    if (catalog == NULL ||
        module_id == NULL)
    {
        return NULL;
    }

    for (index = 0;
         index < catalog->count;
         index++)
    {
        if (strcmp(
            catalog->entries[index]->id,
            module_id
        ) == 0)
        {
            return catalog->entries[index];
        }
    }

    return NULL;
}