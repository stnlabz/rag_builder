#ifndef RAG_BUILDER_MODULE_INVENTORY_H
#define RAG_BUILDER_MODULE_INVENTORY_H

#include <stddef.h>
#include "module.h"

#define RB_MODULE_INVENTORY_MAX 32
#define RB_MODULE_INVENTORY_PATH_MAX 1024

typedef enum
{
    RB_MODULE_INVENTORY_OK = 0,
    RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT,
    RB_MODULE_INVENTORY_ERR_PATH_TOO_LONG,
    RB_MODULE_INVENTORY_ERR_OPEN_FAILED,
    RB_MODULE_INVENTORY_ERR_READ_FAILED,
    RB_MODULE_INVENTORY_ERR_WRITE_FAILED,
    RB_MODULE_INVENTORY_ERR_INVALID_FORMAT,
    RB_MODULE_INVENTORY_ERR_FULL
} rb_module_inventory_result_t;

typedef struct
{
    char module_id[RB_MODULE_ID_MAX];
    unsigned int version_major, version_minor, version_patch;
    unsigned int core_api_major, core_api_minor;
    char binary_sha256[RB_MODULE_SHA256_HEX];
    rb_module_qualification_result_t qualification;
} rb_module_inventory_record_t;

typedef struct
{
    rb_module_inventory_record_t records[RB_MODULE_INVENTORY_MAX];
    size_t count;
    char path[RB_MODULE_INVENTORY_PATH_MAX];
} rb_module_inventory_t;

void rb_module_inventory_init(rb_module_inventory_t* inventory);
rb_module_inventory_result_t rb_module_inventory_configure(rb_module_inventory_t* inventory, const char* output_path);
rb_module_inventory_result_t rb_module_inventory_load(rb_module_inventory_t* inventory);
rb_module_inventory_result_t rb_module_inventory_store(
    rb_module_inventory_t* inventory,
    const rb_module_descriptor_t* descriptor,
    const char* binary_sha256,
    const rb_module_qualification_result_t* qualification
);
const rb_module_inventory_record_t* rb_module_inventory_find(
    const rb_module_inventory_t* inventory,
    const rb_module_descriptor_t* descriptor,
    const char* binary_sha256
);
const char* rb_module_inventory_result_string(rb_module_inventory_result_t result);

#endif
