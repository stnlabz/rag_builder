#ifndef RAG_BUILDER_MODULE_REGISTRY_H
#define RAG_BUILDER_MODULE_REGISTRY_H

#include <stddef.h>
#include "module.h"

#define RB_MODULE_REGISTRY_MAX 32
#define RB_MODULE_AUDIT_MAX    128

typedef enum
{
    RB_MODULE_AUDIT_DISCOVERED = 0,
    RB_MODULE_AUDIT_VERIFIED,
    RB_MODULE_AUDIT_TESTING,
    RB_MODULE_AUDIT_QUALIFIED,
    RB_MODULE_AUDIT_FAILED,
    RB_MODULE_AUDIT_AUTHORIZED,
    RB_MODULE_AUDIT_ACTIVE,
    RB_MODULE_AUDIT_QUARANTINED
} rb_module_audit_event_t;

typedef struct
{
    rb_module_descriptor_t descriptor;
    void* library_handle;
    char binary_path[RB_MODULE_PATH_MAX];
    char binary_sha256[RB_MODULE_SHA256_HEX];

    rb_module_state_t state;
    rb_module_qualification_result_t qualification;
    int activation_authorized;
} rb_module_record_t;

typedef struct
{
    unsigned long sequence;
    char module_id[RB_MODULE_ID_MAX];
    rb_module_audit_event_t event;
    rb_module_state_t previous_state;
    rb_module_state_t resulting_state;
    rb_module_result_t result;
} rb_module_audit_entry_t;

typedef struct
{
    rb_module_record_t modules[RB_MODULE_REGISTRY_MAX];
    size_t count;
    rb_module_audit_entry_t audit[RB_MODULE_AUDIT_MAX];
    size_t audit_count;
    unsigned long next_sequence;
} rb_module_registry_t;

void rb_module_registry_init(rb_module_registry_t* registry);

rb_module_result_t rb_module_registry_discover(
    rb_module_registry_t* registry,
    const rb_module_descriptor_t* descriptor,
    void* library_handle,
    const char* binary_path,
    const char* binary_sha256
);

rb_module_result_t rb_module_registry_verify(rb_module_registry_t* registry, const char* module_id);
rb_module_result_t rb_module_registry_qualify(rb_module_registry_t* registry, const char* module_id);
rb_module_result_t rb_module_registry_restore_qualification(
    rb_module_registry_t* registry,
    const char* module_id,
    const rb_module_qualification_result_t* qualification
);
rb_module_result_t rb_module_registry_authorize_activation(rb_module_registry_t* registry, const char* module_id);
rb_module_result_t rb_module_registry_activate(rb_module_registry_t* registry, const char* module_id);
rb_module_result_t rb_module_registry_fail(rb_module_registry_t* registry, const char* module_id);
rb_module_result_t rb_module_registry_quarantine(rb_module_registry_t* registry, const char* module_id);
const rb_module_record_t* rb_module_registry_find(const rb_module_registry_t* registry, const char* module_id);
void rb_module_registry_unload_all(rb_module_registry_t* registry);

#endif
