#include <stdio.h>
#include <string.h>

#include "core.h"
#include "module_discovery.h"
#include "platform.h"
#include "version.h"


static rb_result_t rb_core_validate_environment(
    const rb_config_t* config
)
{
    rb_platform_result_t r;

    if (config == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    r =
        rb_platform_validate_readable_directory(
            config->source_path
        );

    if (r != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Source directory validation failed: %s\n",
            rb_platform_result_string(r)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Source directory: VALID / READABLE\n"
    );

    r =
        rb_platform_validate_writable_directory(
            config->output_path
        );

    if (r != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Output directory validation failed: %s\n",
            rb_platform_result_string(r)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Output directory: VALID / WRITABLE\n"
    );

    r =
        rb_platform_validate_readable_directory(
            config->modules_path
        );

    if (r != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[ERR] Modules directory validation failed: %s\n",
            rb_platform_result_string(r)
        );

        return RB_ERR_ENVIRONMENT;
    }

    printf(
        "[CORE] Modules directory: VALID / READABLE\n"
    );

    return RB_OK;
}


static void rb_core_process_discovered_modules(
    rb_core_t* core
)
{
    size_t i;

    for (i = 0;
        i < core->module_registry.count;
        i++)
    {
        rb_module_record_t* record;

        const rb_module_inventory_record_t*
            inventory_record;

        rb_module_result_t mr;

        rb_module_inventory_result_t ir;

        record =
            &core->module_registry.modules[i];

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

        printf(
            "[MODULE] Binary SHA-256: %s\n",
            record->binary_sha256
        );

        printf(
            "[MODULE] Execution stage: %u\n",
            record->descriptor.execution_stage
        );

        mr =
            rb_module_registry_verify(
                &core->module_registry,
                record->descriptor.id
            );

        if (mr != RB_MODULE_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Verification failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_result_string(mr)
            );

            continue;
        }

        inventory_record =
            rb_module_inventory_find(
                &core->module_inventory,
                &record->descriptor,
                record->binary_sha256
            );

        if (inventory_record != NULL)
        {
            mr =
                rb_module_registry_restore_qualification(
                    &core->module_registry,
                    record->descriptor.id,
                    &inventory_record->qualification
                );

            if (mr == RB_MODULE_OK)
            {
                printf(
                    "[MODULE] Qualification record: VALID (%s)\n",
                    record->descriptor.id
                );

                printf(
                    "[MODULE] State: QUALIFIED\n"
                );
            }
        }
        else
        {
            printf(
                "[MODULE] Qualification starting: %s\n",
                record->descriptor.id
            );

            mr =
                rb_module_registry_qualify(
                    &core->module_registry,
                    record->descriptor.id
                );

            if (mr != RB_MODULE_OK)
            {
                fprintf(
                    stderr,
                    "[MODULE] Qualification failed: %s (%s)\n",
                    record->descriptor.id,
                    rb_module_result_string(mr)
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
                record->qualification.
                    negative_test_passed
                    ? "PASS"
                    : "FAIL"
            );

            ir =
                rb_module_inventory_store(
                    &core->module_inventory,
                    &record->descriptor,
                    record->binary_sha256,
                    &record->qualification
                );

            if (ir !=
                RB_MODULE_INVENTORY_OK)
            {
                fprintf(
                    stderr,
                    "[MODULE] Qualification inventory write failed: %s (%s)\n",
                    record->descriptor.id,
                    rb_module_inventory_result_string(ir)
                );

                (void)rb_module_registry_fail(
                    &core->module_registry,
                    record->descriptor.id
                );

                continue;
            }

            printf(
                "[MODULE] Qualification record: STORED (%s)\n",
                record->descriptor.id
            );
        }

        mr =
            rb_module_registry_authorize_activation(
                &core->module_registry,
                record->descriptor.id
            );

        if (mr != RB_MODULE_OK)
        {
            continue;
        }

        mr =
            rb_module_registry_activate(
                &core->module_registry,
                record->descriptor.id
            );

        if (mr == RB_MODULE_OK)
        {
            printf(
                "[MODULE] Active: %s\n",
                record->descriptor.id
            );
        }
    }
}


/*
 * Return nonzero when candidate sorts after
 * the previously executed module.
 */
static int rb_core_execution_after(
    const rb_module_record_t* candidate,
    int has_previous,
    unsigned int previous_stage,
    const char* previous_id
)
{
    if (candidate == NULL)
    {
        return 0;
    }

    if (!has_previous)
    {
        return 1;
    }

    if (candidate->descriptor.execution_stage >
        previous_stage)
    {
        return 1;
    }

    if (candidate->descriptor.execution_stage <
        previous_stage)
    {
        return 0;
    }

    return strcmp(
        candidate->descriptor.id,
        previous_id
    ) > 0;
}


/*
 * Return nonzero when candidate sorts before
 * current according to deterministic module
 * execution order:
 *
 *     execution_stage ASC
 *     module ID ASC
 */
static int rb_core_execution_before(
    const rb_module_record_t* candidate,
    const rb_module_record_t* current
)
{
    if (candidate == NULL)
    {
        return 0;
    }

    if (current == NULL)
    {
        return 1;
    }

    if (candidate->descriptor.execution_stage <
        current->descriptor.execution_stage)
    {
        return 1;
    }

    if (candidate->descriptor.execution_stage >
        current->descriptor.execution_stage)
    {
        return 0;
    }

    return strcmp(
        candidate->descriptor.id,
        current->descriptor.id
    ) < 0;
}


static rb_result_t rb_core_execute_active_modules(
    rb_core_t* core
)
{
    rb_module_execution_context_t context;

    unsigned int previous_stage = 0;

    char previous_id[
        RB_MODULE_ID_MAX
    ];

    int has_previous = 0;

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    memset(
        previous_id,
        0,
        sizeof(previous_id)
    );

    context.source_path =
        core->config.source_path;

    context.output_path =
        core->config.output_path;

    /*
     * Selection execution rather than registry-order
     * execution.
     *
     * Each pass finds the smallest ACTIVE module
     * tuple greater than the module executed on the
     * previous pass:
     *
     *     (execution_stage, module_id)
     *
     * This makes execution deterministic regardless
     * of filesystem or discovery order.
     */
    for (;;)
    {
        rb_module_record_t* selected = NULL;

        size_t i;

        for (i = 0;
            i < core->module_registry.count;
            i++)
        {
            rb_module_record_t* candidate;

            candidate =
                &core->module_registry.modules[i];

            if (candidate->state !=
                RB_MODULE_STATE_ACTIVE)
            {
                continue;
            }

            if (!rb_core_execution_after(
                candidate,
                has_previous,
                previous_stage,
                previous_id
            ))
            {
                continue;
            }

            if (rb_core_execution_before(
                candidate,
                selected
            ))
            {
                selected =
                    candidate;
            }
        }

        if (selected == NULL)
        {
            break;
        }

        printf(
            "[MODULE] Execute stage %u: %s\n",
            selected->descriptor.execution_stage,
            selected->descriptor.id
        );

        {
            rb_module_result_t result;

            result =
                selected->descriptor.execute(
                    &context
                );

            if (result !=
                RB_MODULE_OK)
            {
                fprintf(
                    stderr,
                    "[MODULE] Execution failed: %s (%s)\n",
                    selected->descriptor.id,
                    rb_module_result_string(result)
                );

                (void)rb_module_registry_fail(
                    &core->module_registry,
                    selected->descriptor.id
                );

                return RB_ERR_RUNTIME;
            }
        }

        previous_stage =
            selected->descriptor.execution_stage;

        strcpy_s(
            previous_id,
            sizeof(previous_id),
            selected->descriptor.id
        );

        has_previous =
            1;
    }

    return RB_OK;
}


rb_result_t rb_core_init(
    rb_core_t* core,
    const rb_config_t* config
)
{
    rb_result_t result;

    rb_log_result_t log_result;

    rb_module_inventory_result_t
        inventory_result;

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

    rb_module_registry_init(
        &core->module_registry
    );

    rb_module_inventory_init(
        &core->module_inventory
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

    log_result =
        rb_log_init(
            &core->log,
            config->output_path,
            config->log_level
        );

    if (log_result !=
        RB_LOG_OK)
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

        return core->last_result =
            RB_ERR_LOGGING;
    }

    inventory_result =
        rb_module_inventory_configure(
            &core->module_inventory,
            config->output_path
        );

    if (inventory_result !=
        RB_MODULE_INVENTORY_OK)
    {
        core->state =
            RB_CORE_STATE_FAILED;

        return core->last_result =
            RB_ERR_INITIALIZATION;
    }

    inventory_result =
        rb_module_inventory_load(
            &core->module_inventory
        );

    if (inventory_result !=
        RB_MODULE_INVENTORY_OK)
    {
        rb_module_inventory_init(
            &core->module_inventory
        );

        if (rb_module_inventory_configure(
            &core->module_inventory,
            config->output_path
        ) !=
            RB_MODULE_INVENTORY_OK)
        {
            core->state =
                RB_CORE_STATE_FAILED;

            return core->last_result =
                RB_ERR_INITIALIZATION;
        }

        printf(
            "[CORE] Module inventory: INVALID - REQUALIFICATION REQUIRED\n"
        );
    }
    else
    {
        printf(
            "[CORE] Module inventory: %u record(s)\n",
            (unsigned)
            core->module_inventory.count
        );
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

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    if (core->state !=
        RB_CORE_STATE_READY)
    {
        return core->last_result =
            RB_ERR_INVALID_STATE;
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
            core->config.modules_path,
            &report
        );

    if (result !=
        RB_MODULE_OK)
    {
        fprintf(
            stderr,
            "[MODULE] Discovery scan failed: %s\n",
            rb_module_result_string(
                result
            )
        );

        return core->last_result =
            RB_ERR_RUNTIME;
    }

    printf(
        "[MODULE] Discovery scan: %u discovered, %u rejected, %u unknown\n",
        (unsigned)
        report.modules_discovered,
        (unsigned)
        report.modules_rejected,
        (unsigned)
        report.unknown_modules
    );

    rb_core_process_discovered_modules(
        core
    );

    core->last_result =
        rb_core_execute_active_modules(
            core
        );

    return core->last_result;
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
        return core->last_result =
            RB_ERR_INVALID_STATE;
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

    rb_module_registry_unload_all(
        &core->module_registry
    );

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

        if (log_result !=
            RB_LOG_OK)
        {
            return core->last_result =
                RB_ERR_SHUTDOWN;
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