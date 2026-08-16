#ifndef RAG_BUILDER_MODULE_H
#define RAG_BUILDER_MODULE_H

#define RB_MODULE_ID_MAX       64
#define RB_MODULE_NAME_MAX     64
#define RB_MODULE_MIN_TESTS    10

#define RB_MODULE_API_MAJOR    1
#define RB_MODULE_API_MINOR    0

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
    RB_MODULE_ERR_AUDIT_FULL
} rb_module_result_t;

typedef struct
{
    unsigned int tests_executed;
    unsigned int tests_passed;
    unsigned int tests_failed;

    int negative_test_executed;
    int negative_test_passed;
} rb_module_qualification_result_t;

typedef rb_module_result_t (*rb_module_qualify_fn)(
    rb_module_qualification_result_t* result
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

    rb_module_qualify_fn qualify;
} rb_module_descriptor_t;

const char* rb_module_state_string(
    rb_module_state_t state
);

const char* rb_module_result_string(
    rb_module_result_t result
);

#endif