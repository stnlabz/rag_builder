#ifndef RAG_BUILDER_MODULE_H
#define RAG_BUILDER_MODULE_H

#define RB_MODULE_ID_MAX       64
#define RB_MODULE_NAME_MAX     64
#define RB_MODULE_PATH_MAX     1024
#define RB_MODULE_SHA256_HEX   65
#define RB_MODULE_MIN_TESTS    10

/*
 * Module ABI
 *
 * 1.2 adds deterministic execution_stage
 * to rb_module_descriptor_t.
 */
#define RB_MODULE_API_MAJOR    1
#define RB_MODULE_API_MINOR    2

#ifdef _WIN32
#define RB_MODULE_EXPORT __declspec(dllexport)
#else
#define RB_MODULE_EXPORT
#endif

typedef enum
{
    RB_MODULE_STATE_DISCOVERED = 0,
    RB_MODULE_STATE_UNVERIFIED,
    RB_MODULE_STATE_TESTING,
    RB_MODULE_STATE_QUALIFIED,
    RB_MODULE_STATE_ACTIVE,
    RB_MODULE_STATE_FAILED,
    RB_MODULE_STATE_QUARANTINED
} rb_module_state_t;

typedef enum
{
    RB_MODULE_OK = 0,
    RB_MODULE_ERR_INVALID_ARGUMENT,
    RB_MODULE_ERR_INVALID_IDENTITY,
    RB_MODULE_ERR_DUPLICATE,
    RB_MODULE_ERR_REGISTRY_FULL,
    RB_MODULE_ERR_NOT_FOUND,
    RB_MODULE_ERR_INCOMPATIBLE,
    RB_MODULE_ERR_INVALID_STATE,
    RB_MODULE_ERR_QUALIFICATION,
    RB_MODULE_ERR_NOT_QUALIFIED,
    RB_MODULE_ERR_NOT_AUTHORIZED,
    RB_MODULE_ERR_QUARANTINED,
    RB_MODULE_ERR_AUDIT_FULL,
    RB_MODULE_ERR_LOAD_FAILED,
    RB_MODULE_ERR_EXPORT_NOT_FOUND,
    RB_MODULE_ERR_EXECUTION
} rb_module_result_t;

typedef struct
{
    unsigned int tests_executed;
    unsigned int tests_passed;
    unsigned int tests_failed;

    int negative_test_executed;
    int negative_test_passed;
} rb_module_qualification_result_t;

typedef struct
{
    const char* source_path;
    const char* output_path;
} rb_module_execution_context_t;

typedef rb_module_result_t (*rb_module_qualify_fn)(
    rb_module_qualification_result_t* result
);

typedef rb_module_result_t (*rb_module_execute_fn)(
    const rb_module_execution_context_t* context
);

typedef void (*rb_module_shutdown_fn)(
    void
);

typedef struct
{
    char id[RB_MODULE_ID_MAX];
    char name[RB_MODULE_NAME_MAX];

    unsigned int version_major;
    unsigned int version_minor;
    unsigned int version_patch;

    unsigned int required_core_api_major;
    unsigned int required_core_api_minor;

    /*
     * Deterministic execution order.
     *
     * Lower stages execute before higher stages.
     * Modules sharing a stage execute by module ID.
     *
     * Stage 0 is invalid.
     */
    unsigned int execution_stage;

    rb_module_qualify_fn qualify;
    rb_module_execute_fn execute;
    rb_module_shutdown_fn shutdown;

} rb_module_descriptor_t;

typedef const rb_module_descriptor_t*
(*rb_module_get_descriptor_fn)(
    void
);

const char* rb_module_state_string(
    rb_module_state_t state
);

const char* rb_module_result_string(
    rb_module_result_t result
);

#endif