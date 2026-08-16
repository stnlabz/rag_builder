#include <stdio.h>
#include <string.h>

#include "core.h"
#include "markdown.h"
#include "module_discovery.h"
#include "platform.h"
#include "version.h"


#define RB_CORE_MARKDOWN_SOURCE_MAX (64U * 1024U)


static const char* rb_core_markdown_block_type_string(
    rb_markdown_block_type_t type
)
{
    switch (type)
    {
    case RB_MARKDOWN_BLOCK_HEADING:
        return "HEADING";

    case RB_MARKDOWN_BLOCK_PARAGRAPH:
        return "PARAGRAPH";

    case RB_MARKDOWN_BLOCK_UNORDERED_LIST_ITEM:
        return "UNORDERED_LIST_ITEM";

    case RB_MARKDOWN_BLOCK_ORDERED_LIST_ITEM:
        return "ORDERED_LIST_ITEM";

    default:
        return "UNKNOWN";
    }
}


static void rb_core_print_markdown_block(
    const char* source,
    const rb_markdown_block_t* block,
    size_t index
)
{
    if (source == NULL ||
        block == NULL)
    {
        return;
    }

    printf(
        "[MARKDOWN] Block %u\n",
        (unsigned int)(index + 1)
    );

    printf(
        "  Type: %s\n",
        rb_core_markdown_block_type_string(
            block->type
        )
    );

    if (block->type ==
        RB_MARKDOWN_BLOCK_HEADING)
    {
        printf(
            "  Level: %u\n",
            block->heading_level
        );
    }

    printf(
        "  Source: offset=%u length=%u\n",
        (unsigned int)block->source_offset,
        (unsigned int)block->source_length
    );

    printf(
        "  Content: offset=%u length=%u\n",
        (unsigned int)block->content_offset,
        (unsigned int)block->content_length
    );

    printf(
        "  Text: %.*s\n",
        (int)block->content_length,
        source + block->content_offset
    );
}


static rb_result_t rb_core_process_markdown_sources(
    rb_core_t* core
)
{
    const rb_module_record_t* module;

    rb_platform_file_iterator_t iterator;
    rb_platform_result_t platform_result;

    rb_module_result_t module_result;
    rb_markdown_result_t markdown_result;

    rb_markdown_document_t document;

    char path[RB_PLATFORM_PATH_MAX];
    char source[RB_CORE_MARKDOWN_SOURCE_MAX];

    char message[256];

    size_t bytes_read;
    size_t index;

    unsigned int documents_processed;

    if (core == NULL)
    {
        return RB_ERR_INVALID_ARGUMENT;
    }

    /*
     * Operational processing is impossible unless
     * RB-MARKDOWN has already passed Core governance.
     */
    module =
        rb_module_registry_find(
            &core->module_registry,
            "RB-MARKDOWN"
        );

    if (module == NULL)
    {
        fprintf(
            stderr,
            "[MARKDOWN] RB-MARKDOWN not discovered\n"
        );

        return RB_ERR_RUNTIME;
    }

    if (module->state !=
        RB_MODULE_STATE_QUALIFIED)
    {
        fprintf(
            stderr,
            "[MARKDOWN] RB-MARKDOWN unavailable: state=%s\n",
            rb_module_state_string(
                module->state
            )
        );

        return RB_ERR_RUNTIME;
    }


    /*
     * Qualification and activation are separate.
     * Core explicitly authorizes operational use.
     */
    module_result =
        rb_module_registry_authorize_activation(
            &core->module_registry,
            "RB-MARKDOWN"
        );

    if (module_result != RB_MODULE_OK)
    {
        fprintf(
            stderr,
            "[MARKDOWN] Activation authorization failed: %s\n",
            rb_module_result_string(
                module_result
            )
        );

        return RB_ERR_RUNTIME;
    }

    module_result =
        rb_module_registry_activate(
            &core->module_registry,
            "RB-MARKDOWN"
        );

    if (module_result != RB_MODULE_OK)
    {
        fprintf(
            stderr,
            "[MARKDOWN] Activation failed: %s\n",
            rb_module_result_string(
                module_result
            )
        );

        return RB_ERR_RUNTIME;
    }

    printf(
        "[MODULE] Active: RB-MARKDOWN\n"
    );

    (void)rb_log_write(
        &core->log,
        RB_LOG_INFO,
        "MODULE",
        "ACTIVE id=RB-MARKDOWN"
    );


    /*
     * Core owns source discovery.
     * Platform owns filesystem mechanics.
     * Markdown only receives source bytes.
     */
    platform_result =
        rb_platform_file_iterator_open(
            &iterator,
            core->config.source_path,
            "*.md"
        );

    if (platform_result ==
        RB_PLATFORM_ERR_NO_MORE_FILES)
    {
        printf(
            "[MARKDOWN] No Markdown source files found\n"
        );

        return RB_OK;
    }

    if (platform_result !=
        RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[MARKDOWN] Source enumeration failed: %s\n",
            rb_platform_result_string(
                platform_result
            )
        );

        return RB_ERR_RUNTIME;
    }

    documents_processed = 0;

    for (;;)
    {
        platform_result =
            rb_platform_file_iterator_next(
                &iterator,
                path,
                sizeof(path)
            );

        if (platform_result ==
            RB_PLATFORM_ERR_NO_MORE_FILES)
        {
            break;
        }

        if (platform_result !=
            RB_PLATFORM_OK)
        {
            fprintf(
                stderr,
                "[MARKDOWN] Source enumeration failed: %s\n",
                rb_platform_result_string(
                    platform_result
                )
            );

            rb_platform_file_iterator_close(
                &iterator
            );

            return RB_ERR_RUNTIME;
        }

        printf(
            "[MARKDOWN] Source: %s\n",
            path
        );

        bytes_read = 0;

        platform_result =
            rb_platform_read_file(
                path,
                source,
                sizeof(source),
                &bytes_read
            );

        if (platform_result !=
            RB_PLATFORM_OK)
        {
            fprintf(
                stderr,
                "[MARKDOWN] Read failed: %s (%s)\n",
                path,
                rb_platform_result_string(
                    platform_result
                )
            );

            rb_platform_file_iterator_close(
                &iterator
            );

            return RB_ERR_RUNTIME;
        }

        printf(
            "[MARKDOWN] Read: %u bytes\n",
            (unsigned int)bytes_read
        );

        memset(
            &document,
            0,
            sizeof(document)
        );

        markdown_result =
            rb_markdown_parse(
                source,
                bytes_read,
                &document
            );

        if (markdown_result !=
            RB_MARKDOWN_OK)
        {
            fprintf(
                stderr,
                "[MARKDOWN] Parse failed: %s (%s)\n",
                path,
                rb_markdown_result_string(
                    markdown_result
                )
            );

            rb_platform_file_iterator_close(
                &iterator
            );

            return RB_ERR_RUNTIME;
        }

        printf(
            "[MARKDOWN] Parse PASS: %u block(s)\n",
            (unsigned int)document.block_count
        );

        for (index = 0;
            index < document.block_count;
            index++)
        {
            rb_core_print_markdown_block(
                source,
                &document.blocks[index],
                index
            );
        }

        documents_processed++;

        (void)snprintf(
            message,
            sizeof(message),
            "PARSED path=%s bytes=%u blocks=%u",
            path,
            (unsigned int)bytes_read,
            (unsigned int)document.block_count
        );

        (void)rb_log_write(
            &core->log,
            RB_LOG_INFO,
            "MARKDOWN",
            message
        );
    }

    rb_platform_file_iterator_close(
        &iterator
    );

    printf(
        "[MARKDOWN] Documents processed: %u\n",
        documents_processed
    );

    return RB_OK;
}


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


static void rb_core_process_discovered_modules(
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

        const rb_module_inventory_record_t*
            inventory_record;

        rb_module_result_t module_result;

        rb_module_inventory_result_t
            inventory_result;

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

        module_result =
            rb_module_registry_verify(
                &core->module_registry,
                record->descriptor.id
            );

        if (module_result != RB_MODULE_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Verification failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_result_string(module_result)
            );

            continue;
        }

        inventory_record =
            rb_module_inventory_find(
                &core->module_inventory,
                &record->descriptor
            );

        if (inventory_record != NULL)
        {
            module_result =
                rb_module_registry_restore_qualification(
                    &core->module_registry,
                    record->descriptor.id,
                    &inventory_record->qualification
                );

            if (module_result == RB_MODULE_OK)
            {
                printf(
                    "[MODULE] Qualification record: VALID (%s)\n",
                    record->descriptor.id
                );

                printf(
                    "[MODULE] State: QUALIFIED\n"
                );

                (void)snprintf(
                    message,
                    sizeof(message),
                    "QUALIFICATION_RESTORED id=%s tests=%u/%u negative=PASS",
                    record->descriptor.id,
                    inventory_record->qualification.tests_passed,
                    inventory_record->qualification.tests_executed
                );

                (void)rb_log_write(
                    &core->log,
                    RB_LOG_INFO,
                    "MODULE",
                    message
                );

                continue;
            }
        }

        printf(
            "[MODULE] Qualification starting: %s\n",
            record->descriptor.id
        );

        module_result =
            rb_module_registry_qualify(
                &core->module_registry,
                record->descriptor.id
            );

        if (module_result != RB_MODULE_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Qualification failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_result_string(module_result)
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

        inventory_result =
            rb_module_inventory_store(
                &core->module_inventory,
                &record->descriptor,
                &record->qualification
            );

        if (inventory_result !=
            RB_MODULE_INVENTORY_OK)
        {
            fprintf(
                stderr,
                "[MODULE] Qualification inventory write failed: %s (%s)\n",
                record->descriptor.id,
                rb_module_inventory_result_string(
                    inventory_result
                )
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

        (void)snprintf(
            message,
            sizeof(message),
            "QUALIFIED id=%s tests=%u/%u negative=PASS inventory=STORED",
            record->descriptor.id,
            record->qualification.tests_passed,
            record->qualification.tests_executed
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

    rb_module_catalog_init(
        &core->module_catalog
    );

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
            rb_log_result_string(log_result)
        );

        core->state =
            RB_CORE_STATE_FAILED;

        core->last_result =
            RB_ERR_LOGGING;

        return core->last_result;
    }

    inventory_result =
        rb_module_inventory_configure(
            &core->module_inventory,
            config->output_path
        );

    if (inventory_result !=
        RB_MODULE_INVENTORY_OK)
    {
        fprintf(
            stderr,
            "[ERR] Module inventory configuration failed: %s\n",
            rb_module_inventory_result_string(
                inventory_result
            )
        );

        core->state =
            RB_CORE_STATE_FAILED;

        core->last_result =
            RB_ERR_INITIALIZATION;

        return core->last_result;
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

            core->last_result =
                RB_ERR_INITIALIZATION;

            return core->last_result;
        }

        printf(
            "[CORE] Module inventory: INVALID - REQUALIFICATION REQUIRED\n"
        );
    }
    else
    {
        printf(
            "[CORE] Module inventory: %u record(s)\n",
            (unsigned int)
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
    rb_result_t processing_result;

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

    rb_core_process_discovered_modules(
        core
    );

    processing_result =
        rb_core_process_markdown_sources(
            core
        );

    if (processing_result != RB_OK)
    {
        core->last_result =
            processing_result;

        return core->last_result;
    }

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