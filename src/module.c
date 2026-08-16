#include "module.h"

const char* rb_module_state_string(
    rb_module_state_t state
)
{
    switch (state)
    {
    case RB_MODULE_STATE_DISCOVERED:
        return "DISCOVERED";

    case RB_MODULE_STATE_UNVERIFIED:
        return "UNVERIFIED";

    case RB_MODULE_STATE_TESTING:
        return "TESTING";

    case RB_MODULE_STATE_QUALIFIED:
        return "QUALIFIED";

    case RB_MODULE_STATE_ACTIVE:
        return "ACTIVE";

    case RB_MODULE_STATE_FAILED:
        return "FAILED";

    case RB_MODULE_STATE_QUARANTINED:
        return "QUARANTINED";

    default:
        return "UNKNOWN";
    }
}

const char* rb_module_result_string(
    rb_module_result_t result
)
{
    switch (result)
    {
    case RB_MODULE_OK:
        return "OK";

    case RB_MODULE_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_MODULE_ERR_INVALID_IDENTITY:
        return "INVALID_IDENTITY";

    case RB_MODULE_ERR_DUPLICATE:
        return "DUPLICATE";

    case RB_MODULE_ERR_REGISTRY_FULL:
        return "REGISTRY_FULL";

    case RB_MODULE_ERR_NOT_FOUND:
        return "NOT_FOUND";

    case RB_MODULE_ERR_INCOMPATIBLE:
        return "INCOMPATIBLE";

    case RB_MODULE_ERR_INVALID_STATE:
        return "INVALID_STATE";

    case RB_MODULE_ERR_QUALIFICATION:
        return "QUALIFICATION_FAILED";

    case RB_MODULE_ERR_NOT_QUALIFIED:
        return "NOT_QUALIFIED";

    case RB_MODULE_ERR_NOT_AUTHORIZED:
        return "NOT_AUTHORIZED";

    case RB_MODULE_ERR_QUARANTINED:
        return "QUARANTINED";

    case RB_MODULE_ERR_AUDIT_FULL:
        return "AUDIT_FULL";

    default:
        return "UNKNOWN";
    }
}