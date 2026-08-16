#include <stdio.h>

#include "core.h"
#include "markdown.h"
#include "module_discovery.h"
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

    platform_result =
        rb_platform_validate_readable_directory(
            config->modules_path
        );

    if (platform_result != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Modules directory validation failed: %s\n",
            rb_platform_result_string(platform_result)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Modules directory: VALID / READABLE\n"
    );

    return RB_OK;
}


static rb_result_t rb_core_register_builtin_modules(
    rb_core_t* core
)
{
    rb_module_result_t result;

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    result = rb_module_catalog_register(
        &core->module_catalog,
        rb_markdown_get_descriptor()
    );

    if (result != RB_MODULE_OK)
    {
        fprintf(
            stderr,
            "[ERR] Unable to register Markdown module: %s\n",
            rb_module_result_string(result)
        );

        return RB_ERR_INITIALIZATION;
    }

    return RB_OK;
}


static void rb_core_qualify_discovered_modules(
    rb_core_t* core
)
{
    size_t index;

    if (core == NULL)
    {
        return;
    }

    for (index = 0;
        index < core->module_registry.count;
        index++)
    {
        rb_module_record_t* record;
        rb_module_result_t result;

        char message[256];

        record =
            &core->module_registry.modules[index];

        if (record->state !=
            RB_MODULE_STATE_DISCOVERED)
        {
            continue;
        }

        printf(
            "[MODULE] Discovered: %s (%s)\n",
            record->descriptor.name,
            record->descriptor.id
        );

        (void)snprintf(
            message,
            sizeof(message),
            "DISCOVERED id=%s name=%s",
            record->descriptor.id,
            record->descriptor.name
        );

        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "MODULE",
            message
        );

        result =
            rb_module_registry_verify(
                &core->module_registry,
                record->descriptor.id
            );

        if (result != RB_MODULE_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Verification failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_result_string(result)
            );

            (void)snprintf(
                message,
                sizeof(message),
                "VERIFICATION_FAILED id=%s result=%s",
                record->descriptor.id,
                rb_module_result_string(result)
            );

            (void)rb_log_write(
                &core->log,
                RB_LOG_ERROR,
                "MODULE",
                message
            );

            continue;
        }

        printf(
            "[MODULE] Qualification starting: %s\n",
            record->descriptor.id
        );

        (void)snprintf(
            message,
            sizeof(message),
            "QUALIFICATION_STARTED id=%s",
            record->descriptor.id
        );

        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "MODULE",
            message
        );

        result =
            rb_module_registry_qualify(
                &core->module_registry,
                record->descriptor.id
            );

        if (result != RB_MODULE_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Qualification failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_result_string(result)
            );

            (void)snprintf(
                message,
                sizeof(message),
                "QUALIFICATION_FAILED id=%s result=%s",
                record->descriptor.id,
                rb_module_result_string(result)
            );

            (void)rb_log_write(
                &core->log,
                RB_LOG_ERROR,
                "MODULE",
                message
            );

            continue;
        }

        printf(
            "[MODULE] Qualification PASS: %s (%u/%u)\n",
            record->descriptor.id,
            record->qualification.tests_passed,
            record->qualification.tests_executed
        );

        printf(
            "[MODULE] Negative validation: %s\n",
            record->qualification.negative_test_passed
            ? "PASS"
            : "FAIL"
        );

        (void)snprintf(
            message,
            sizeof(message),
            "QUALIFIED id=%s tests=%u/%u negative=%s",
            record->descriptor.id,
            record->qualification.tests_passed,
            record->qualification.tests_executed,
            record->qualification.negative_test_passed
            ? "PASS"
            : "FAIL"
        );

        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "MODULE",
            message
        );
    }
}


rb_result_t rb_core_init(
    rb_core_t* core,
    const rb_config_t* config
)
{
    rb_result_t result;
    rb_log_result_t log_result;

    if (core == NULL ||
        config == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state !=
        RB_CORE_STATE_UNINITIALIZED)
    {
        core->last_result =
            RB_ERR_INVALID_STATE;

        return core->last_result;
    }

    core->state =
        RB_CORE_STATE_INITIALIZING;

    core->config =
        *config;

    rb_module_catalog_init(
        &core->module_catalog
    );

    rb_module_registry_init(
        &core->module_registry
    );

    printf(
        "%s %d.%d.%d %s\n",
        RB_NAME,
        RB_VERSION_MAJOR,
        RB_VERSION_MINOR,
        RB_VERSION_PATCH,
        RB_VERSION_STATUS
    );

    printf(
        "[CORE] Initializing...\n"
    );

    result =
        rb_core_validate_environment(
            config
        );

    if (result != RB_OK)
    {
        core->state =
            RB_CORE_STATE_FAILED;

        core->last_result =
            result;

        printf(
            "[CORE] State: %s\n",
            rb_core_state_string(
                core->state
            )
        );

        return result;
    }

    /*
     * Register implementations compiled into this build.
     *
     * Registration does not discover, qualify, or activate
     * a module. Filesystem presence is still required.
     */
    result =
        rb_core_register_builtin_modules(
            core
        );

    if (result != RB_OK)
    {
        core->state =
            RB_CORE_STATE_FAILED;

        core->last_result =
            result;

        printf(
            "[CORE] State: %s\n",
            rb_core_state_string(
                core->state
            )
        );

        return result;
    }

    log_result =
        rb_log_init(
            &core->log,
            config->output_path,
            config->log_level
        );

    if (log_result != RB_LOG_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core logging initialization failed: %s\n",
            rb_log_result_string(
                log_result
            )
        );

        core->state =
            RB_CORE_STATE_FAILED;

        core->last_result =
            RB_ERR_LOGGING;

        printf(
            "[CORE] State: %s\n",
            rb_core_state_string(
                core->state
            )
        );

        return core->last_result;
    }

    (void)rb_log_write(
        &core->log,
        RB_LOG_INFO,
        "CORE",
        "INITIALIZING"
    );

    core->state =
        RB_CORE_STATE_READY;

    core->last_result =
        RB_OK;

    (void)rb_log_write(
        &core->log,
        RB_LOG_INFO,
        "CORE",
        "READY"
    );

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(
            core->state
        )
    );

    return RB_OK;
}


rb_result_t rb_core_run(
    rb_core_t* core
)
{
    rb_module_discovery_report_t report;
    rb_module_result_t result;

    char message[256];

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state !=
        RB_CORE_STATE_READY)
    {
        core->last_result =
            RB_ERR_INVALID_STATE;

        return core->last_result;
    }

    core->state =
        RB_CORE_STATE_RUNNING;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(
            core->state
        )
    );

    (void)rb_log_write(
        &core->log,
        RB_LOG_INFO,
        "CORE",
        "RUNNING"
    );

    result =
        rb_module_discovery_scan(
            &core->module_registry,
            &core->module_catalog,
            core->config.modules_path,
            &report
        );

    if (result != RB_MODULE_OK)
    {
        fprintf(
            stderr,
            "[MODULE] Discovery scan failed: %s\n",
            rb_module_result_string(result)
        );

        (void)snprintf(
            message,
            sizeof(message),
            "DISCOVERY_FAILED result=%s",
            rb_module_result_string(result)
        );

        (void)rb_log_write(
            &core->log,
            RB_LOG_ERROR,
            "MODULE",
            message
        );

        core->last_result =
            RB_ERR_RUNTIME;

        return core->last_result;
    }

    printf(
        "[MODULE] Discovery scan: %u discovered, %u rejected, %u unknown\n",
        (unsigned int)
        report.modules_discovered,
        (unsigned int)
        report.modules_rejected,
        (unsigned int)
        report.unknown_modules
    );

    (void)snprintf(
        message,
        sizeof(message),
        "DISCOVERY_COMPLETE discovered=%u rejected=%u unknown=%u",
        (unsigned int)
        report.modules_discovered,
        (unsigned int)
        report.modules_rejected,
        (unsigned int)
        report.unknown_modules
    );

    (void)rb_log_write(
        &core->log,
        RB_LOG_INFO,
        "MODULE",
        message
    );

    rb_core_qualify_discovered_modules(
        core
    );

    core->last_result =
        RB_OK;

    return RB_OK;
}


rb_result_t rb_core_shutdown(
    rb_core_t* core
)
{
    rb_log_result_t log_result;

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state !=
        RB_CORE_STATE_RUNNING &&
        core->state !=
        RB_CORE_STATE_READY &&
        core->state !=
        RB_CORE_STATE_FAILED)
    {
        core->last_result =
            RB_ERR_INVALID_STATE;

        return core->last_result;
    }

    core->state =
        RB_CORE_STATE_STOPPING;

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(
            core->state
        )
    );

    if (core->log.initialized)
    {
        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "CORE",
            "STOPPING"
        );
    }

    core->state =
        RB_CORE_STATE_STOPPED;

    core->last_result =
        RB_OK;

    if (core->log.initialized)
    {
        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "CORE",
            "STOPPED"
        );

        log_result =
            rb_log_close(
                &core->log
            );

        if (log_result != RB_LOG_OK)
        {
            core->last_result =
                RB_ERR_SHUTDOWN;

            return core->last_result;
        }
    }

    printf(
        "[CORE] State: %s\n",
        rb_core_state_string(
            core->state
        )
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

    case RB_ERR_LOGGING:
        return "LOGGING_ERROR";

    case RB_ERR_RUNTIME:
        return "RUNTIME_ERROR";

    case RB_ERR_SHUTDOWN:
        return "SHUTDOWN_ERROR";

    default:
        return "UNKNOWN";
    }
}