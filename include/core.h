#ifndef RAG_BUILDER_CORE_H
#define RAG_BUILDER_CORE_H

#include "config.h"
#include "log.h"
#include "module_catalog.h"
#include "module_inventory.h"
#include "module_registry.h"

typedef enum
{
    RB_CORE_STATE_UNINITIALIZED = 0,
    RB_CORE_STATE_INITIALIZING,
    RB_CORE_STATE_READY,
    RB_CORE_STATE_RUNNING,
    RB_CORE_STATE_STOPPING,
    RB_CORE_STATE_STOPPED,
    RB_CORE_STATE_FAILED

} rb_core_state_t;

typedef enum
{
    RB_OK = 0,
    RB_ERR_INVALID_ARGUMENT,
    RB_ERR_INVALID_STATE,
    RB_ERR_INITIALIZATION,
    RB_ERR_ENVIRONMENT,
    RB_ERR_LOGGING,
    RB_ERR_RUNTIME,
    RB_ERR_SHUTDOWN

} rb_result_t;

typedef struct
{
    rb_core_state_t state;
    rb_result_t last_result;

    rb_config_t config;

    rb_log_t log;

    rb_module_catalog_t module_catalog;
    rb_module_registry_t module_registry;
    rb_module_inventory_t module_inventory;

} rb_core_t;

rb_result_t rb_core_init(
    rb_core_t* core,
    const rb_config_t* config
);

rb_result_t rb_core_run(
    rb_core_t* core
);

rb_result_t rb_core_shutdown(
    rb_core_t* core
);

const char* rb_core_state_string(
    rb_core_state_t state
);

const char* rb_result_string(
    rb_result_t result
);

#endif