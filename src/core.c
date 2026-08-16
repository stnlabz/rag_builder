#include <stdio.h>

#include "core.h"
#include "platform.h"
#include "version.h"

static rb_result_t rb_core_validate_environment(
    const rb_config_t* config
)
{
    rb_platform_result_t platform_result;

    if (config == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    platform_result =
        rb_platform_validate_readable_directory(
            config->source_path
        );

    if (platform_result != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Source directory validation failed: %s\n",
            rb_platform_result_string(platform_result)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Source directory: VALID / READABLE\n"
    );

    platform_result =
        rb_platform_validate_writable_directory(
            config->output_path
        );

    if (platform_result != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Output directory validation failed: %s\n",
            rb_platform_result_string(platform_result)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Output directory: VALID / WRITABLE\n"
    );

    return RB_OK;
}

rb_result_t rb_core_init(
    rb_core_t* core,
    const rb_config_t* config
)
{
    rb_result_t result;

    if (core == NULL || config == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state != RB_CORE_STATE_UNINITIALIZED)
    {
        core->last_result = RB_ERR_INVALID_STATE;
        return core->last_result;
    }

    core->state = RB_CORE_STATE_INITIALIZING;

    printf(
        "%s %d.%d.%d %s\n",
        RB_NAME,
        RB_VERSION_MAJOR,
        RB_VERSION_MINOR,
        RB_VERSION_PATCH,
        RB_VERSION_STATUS
    );

    printf("[CORE] Initializing...\n");

    result = rb_core_validate_environment(config);

    if (result != RB_OK)
    {
        core->state = RB_CORE_STATE_FAILED;
        core->last_result = result;

        return result;
    }

    core->state = RB_CORE_STATE_READY;
    core->last_result = RB_OK;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(core->state)
    );

    return RB_OK;
}

rb_result_t rb_core_run(
    rb_core_t* core
)
{
    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state != RB_CORE_STATE_READY)
    {
        core->last_result = RB_ERR_INVALID_STATE;
        return core->last_result;
    }

    core->state = RB_CORE_STATE_RUNNING;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(core->state)
    );

    core->last_result = RB_OK;

    return RB_OK;
}

rb_result_t rb_core_shutdown(
    rb_core_t* core
)
{
    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state != RB_CORE_STATE_RUNNING &&
        core->state != RB_CORE_STATE_READY &&
        core->state != RB_CORE_STATE_FAILED)
    {
        core->last_result = RB_ERR_INVALID_STATE;
        return core->last_result;
    }

    core->state = RB_CORE_STATE_STOPPING;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(core->state)
    );

    core->state = RB_CORE_STATE_STOPPED;
    core->last_result = RB_OK;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(core->state)
    );

    return RB_OK;
}

const char* rb_core_state_string(
    rb_core_state_t state
)
{
    switch (state)
    {
    case RB_CORE_STATE_UNINITIALIZED:
        return "UNINITIALIZED";

    case RB_CORE_STATE_INITIALIZING:
        return "INITIALIZING";

    case RB_CORE_STATE_READY:
        return "READY";

    case RB_CORE_STATE_RUNNING:
        return "RUNNING";

    case RB_CORE_STATE_STOPPING:
        return "STOPPING";

    case RB_CORE_STATE_STOPPED:
        return "STOPPED";

    case RB_CORE_STATE_FAILED:
        return "FAILED";

    default:
        return "UNKNOWN";
    }
}

const char* rb_result_string(
    rb_result_t result
)
{
    switch (result)
    {
    case RB_OK:
        return "OK";

    case RB_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_ERR_INVALID_STATE:
        return "INVALID_STATE";

    case RB_ERR_INITIALIZATION:
        return "INITIALIZATION_ERROR";

    case RB_ERR_ENVIRONMENT:
        return "ENVIRONMENT_ERROR";

    case RB_ERR_RUNTIME:
        return "RUNTIME_ERROR";

    case RB_ERR_SHUTDOWN:
        return "SHUTDOWN_ERROR";

    default:
        return "UNKNOWN";
    }
}