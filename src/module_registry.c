#include <string.h>
#include <windows.h>

#include "module_registry.h"


static int rb_module_text_valid(
    const char* text,
    size_t capacity
)
{
    size_t length;

    if (text == NULL ||
        capacity == 0)
    {
        return 0;
    }

    length =
        strlen(text);

    return length > 0 &&
        length < capacity;
}


static rb_module_record_t*
rb_module_registry_find_mutable(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    size_t i;

    if (registry == NULL ||
        module_id == NULL)
    {
        return NULL;
    }

    for (i = 0;
        i < registry->count;
        i++)
    {
        if (strcmp(
            registry->modules[i].descriptor.id,
            module_id
        ) == 0)
        {
            return &registry->modules[i];
        }
    }

    return NULL;
}


static rb_module_result_t rb_module_audit(
    rb_module_registry_t* registry,
    const rb_module_record_t* module,
    rb_module_audit_event_t event,
    rb_module_state_t previous_state,
    rb_module_result_t result
)
{
    rb_module_audit_entry_t* entry;
    size_t length;

    if (registry == NULL ||
        module == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (registry->audit_count >=
        RB_MODULE_AUDIT_MAX)
    {
        return RB_MODULE_ERR_AUDIT_FULL;
    }

    entry =
        &registry->audit[
            registry->audit_count
        ];

    memset(
        entry,
        0,
        sizeof(*entry)
    );

    entry->sequence =
        registry->next_sequence++;

    entry->event =
        event;

    entry->previous_state =
        previous_state;

    entry->resulting_state =
        module->state;

    entry->result =
        result;

    length =
        strlen(
            module->descriptor.id
        );

    if (length >=
        sizeof(
            entry->module_id
        ))
    {
        return RB_MODULE_ERR_INVALID_IDENTITY;
    }

    memcpy(
        entry->module_id,
        module->descriptor.id,
        length + 1
    );

    registry->audit_count++;

    return RB_MODULE_OK;
}


void rb_module_registry_init(
    rb_module_registry_t* registry
)
{
    if (registry == NULL)
    {
        return;
    }

    memset(
        registry,
        0,
        sizeof(*registry)
    );

    registry->next_sequence =
        1;
}


rb_module_result_t rb_module_registry_discover(
    rb_module_registry_t* registry,
    const rb_module_descriptor_t* descriptor,
    void* library_handle,
    const char* binary_path,
    const char* binary_sha256
)
{
    rb_module_record_t* record;

    rb_module_result_t audit_result;

    if (registry == NULL ||
        descriptor == NULL ||
        library_handle == NULL ||
        binary_path == NULL ||
        binary_sha256 == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    /*
     * Descriptor identity validation.
     *
     * execution_stage 0 is reserved as invalid.
     */
    if (!rb_module_text_valid(
            descriptor->id,
            sizeof(descriptor->id)
        ) ||
        !rb_module_text_valid(
            descriptor->name,
            sizeof(descriptor->name)
        ) ||
        descriptor->execution_stage == 0 ||
        descriptor->qualify == NULL ||
        descriptor->execute == NULL)
    {
        return RB_MODULE_ERR_INVALID_IDENTITY;
    }

    if (strlen(binary_path) >=
            RB_MODULE_PATH_MAX ||
        strlen(binary_sha256) != 64)
    {
        return RB_MODULE_ERR_INVALID_IDENTITY;
    }

    if (rb_module_registry_find_mutable(
        registry,
        descriptor->id
    ) != NULL)
    {
        return RB_MODULE_ERR_DUPLICATE;
    }

    if (registry->count >=
        RB_MODULE_REGISTRY_MAX)
    {
        return RB_MODULE_ERR_REGISTRY_FULL;
    }

    record =
        &registry->modules[
            registry->count
        ];

    memset(
        record,
        0,
        sizeof(*record)
    );

    record->descriptor =
        *descriptor;

    record->library_handle =
        library_handle;

    strcpy_s(
        record->binary_path,
        sizeof(record->binary_path),
        binary_path
    );

    strcpy_s(
        record->binary_sha256,
        sizeof(record->binary_sha256),
        binary_sha256
    );

    record->state =
        RB_MODULE_STATE_DISCOVERED;

    audit_result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_DISCOVERED,
            RB_MODULE_STATE_DISCOVERED,
            RB_MODULE_OK
        );

    if (audit_result !=
        RB_MODULE_OK)
    {
        memset(
            record,
            0,
            sizeof(*record)
        );

        return audit_result;
    }

    registry->count++;

    return RB_MODULE_OK;
}


rb_module_result_t rb_module_registry_verify(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    rb_module_result_t audit_result;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    if (record->state ==
        RB_MODULE_STATE_QUARANTINED)
    {
        return RB_MODULE_ERR_QUARANTINED;
    }

    if (record->state !=
        RB_MODULE_STATE_DISCOVERED)
    {
        return RB_MODULE_ERR_INVALID_STATE;
    }

    previous =
        record->state;

    /*
     * Major API version must match exactly.
     *
     * Modules may require an API minor version
     * less than or equal to the Core-supported
     * minor version.
     */
    if (record->descriptor.required_core_api_major !=
            RB_MODULE_API_MAJOR ||
        record->descriptor.required_core_api_minor >
            RB_MODULE_API_MINOR)
    {
        record->state =
            RB_MODULE_STATE_FAILED;

        (void)rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_FAILED,
            previous,
            RB_MODULE_ERR_INCOMPATIBLE
        );

        return RB_MODULE_ERR_INCOMPATIBLE;
    }

    /*
     * Stage zero is never executable.
     */
    if (record->descriptor.execution_stage == 0)
    {
        record->state =
            RB_MODULE_STATE_FAILED;

        (void)rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_FAILED,
            previous,
            RB_MODULE_ERR_INVALID_IDENTITY
        );

        return RB_MODULE_ERR_INVALID_IDENTITY;
    }

    record->state =
        RB_MODULE_STATE_UNVERIFIED;

    audit_result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_VERIFIED,
            previous,
            RB_MODULE_OK
        );

    if (audit_result !=
        RB_MODULE_OK)
    {
        record->state =
            previous;
    }

    return audit_result;
}


rb_module_result_t rb_module_registry_qualify(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    rb_module_result_t module_result;
    rb_module_result_t audit_result;

    rb_module_qualification_result_t report;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    if (record->state ==
        RB_MODULE_STATE_QUARANTINED)
    {
        return RB_MODULE_ERR_QUARANTINED;
    }

    if (record->state !=
        RB_MODULE_STATE_UNVERIFIED)
    {
        return RB_MODULE_ERR_INVALID_STATE;
    }

    previous =
        record->state;

    record->state =
        RB_MODULE_STATE_TESTING;

    audit_result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_TESTING,
            previous,
            RB_MODULE_OK
        );

    if (audit_result !=
        RB_MODULE_OK)
    {
        record->state =
            previous;

        return audit_result;
    }

    memset(
        &report,
        0,
        sizeof(report)
    );

    module_result =
        record->descriptor.qualify(
            &report
        );

    record->qualification =
        report;

    previous =
        record->state;

    if (module_result !=
            RB_MODULE_OK ||
        report.tests_executed <
            RB_MODULE_MIN_TESTS ||
        report.tests_passed !=
            report.tests_executed ||
        report.tests_failed != 0 ||
        report.tests_passed +
            report.tests_failed !=
            report.tests_executed ||
        !report.negative_test_executed ||
        !report.negative_test_passed)
    {
        record->state =
            RB_MODULE_STATE_FAILED;

        record->activation_authorized =
            0;

        (void)rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_FAILED,
            previous,
            RB_MODULE_ERR_QUALIFICATION
        );

        return RB_MODULE_ERR_QUALIFICATION;
    }

    record->state =
        RB_MODULE_STATE_QUALIFIED;

    audit_result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_QUALIFIED,
            previous,
            RB_MODULE_OK
        );

    if (audit_result !=
        RB_MODULE_OK)
    {
        record->state =
            RB_MODULE_STATE_FAILED;
    }

    return audit_result;
}


rb_module_result_t
rb_module_registry_restore_qualification(
    rb_module_registry_t* registry,
    const char* module_id,
    const rb_module_qualification_result_t* qualification
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    rb_module_result_t audit_result;

    if (registry == NULL ||
        module_id == NULL ||
        qualification == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    if (record->state ==
        RB_MODULE_STATE_QUARANTINED)
    {
        return RB_MODULE_ERR_QUARANTINED;
    }

    if (record->state !=
        RB_MODULE_STATE_UNVERIFIED)
    {
        return RB_MODULE_ERR_INVALID_STATE;
    }

    if (qualification->tests_executed <
            RB_MODULE_MIN_TESTS ||
        qualification->tests_passed !=
            qualification->tests_executed ||
        qualification->tests_failed != 0 ||
        qualification->tests_passed +
            qualification->tests_failed !=
            qualification->tests_executed ||
        !qualification->negative_test_executed ||
        !qualification->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    previous =
        record->state;

    record->qualification =
        *qualification;

    record->activation_authorized =
        0;

    record->state =
        RB_MODULE_STATE_QUALIFIED;

    audit_result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_QUALIFIED,
            previous,
            RB_MODULE_OK
        );

    if (audit_result !=
        RB_MODULE_OK)
    {
        record->state =
            previous;

        memset(
            &record->qualification,
            0,
            sizeof(
                record->qualification
            )
        );
    }

    return audit_result;
}


rb_module_result_t
rb_module_registry_authorize_activation(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_result_t result;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    if (record->state ==
        RB_MODULE_STATE_QUARANTINED)
    {
        return RB_MODULE_ERR_QUARANTINED;
    }

    if (record->state !=
        RB_MODULE_STATE_QUALIFIED)
    {
        return RB_MODULE_ERR_NOT_QUALIFIED;
    }

    record->activation_authorized =
        1;

    result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_AUTHORIZED,
            record->state,
            RB_MODULE_OK
        );

    if (result !=
        RB_MODULE_OK)
    {
        record->activation_authorized =
            0;
    }

    return result;
}


rb_module_result_t rb_module_registry_activate(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    rb_module_result_t result;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    if (record->state ==
        RB_MODULE_STATE_QUARANTINED)
    {
        return RB_MODULE_ERR_QUARANTINED;
    }

    if (record->state !=
        RB_MODULE_STATE_QUALIFIED)
    {
        return RB_MODULE_ERR_NOT_QUALIFIED;
    }

    if (!record->
        activation_authorized)
    {
        return RB_MODULE_ERR_NOT_AUTHORIZED;
    }

    previous =
        record->state;

    record->state =
        RB_MODULE_STATE_ACTIVE;

    result =
        rb_module_audit(
            registry,
            record,
            RB_MODULE_AUDIT_ACTIVE,
            previous,
            RB_MODULE_OK
        );

    if (result !=
        RB_MODULE_OK)
    {
        record->state =
            previous;
    }

    return result;
}


rb_module_result_t rb_module_registry_fail(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    previous =
        record->state;

    record->state =
        RB_MODULE_STATE_FAILED;

    record->activation_authorized =
        0;

    return rb_module_audit(
        registry,
        record,
        RB_MODULE_AUDIT_FAILED,
        previous,
        RB_MODULE_OK
    );
}


rb_module_result_t
rb_module_registry_quarantine(
    rb_module_registry_t* registry,
    const char* module_id
)
{
    rb_module_record_t* record;

    rb_module_state_t previous;

    record =
        rb_module_registry_find_mutable(
            registry,
            module_id
        );

    if (record == NULL)
    {
        return RB_MODULE_ERR_NOT_FOUND;
    }

    previous =
        record->state;

    record->state =
        RB_MODULE_STATE_QUARANTINED;

    record->activation_authorized =
        0;

    return rb_module_audit(
        registry,
        record,
        RB_MODULE_AUDIT_QUARANTINED,
        previous,
        RB_MODULE_OK
    );
}


const rb_module_record_t*
rb_module_registry_find(
    const rb_module_registry_t* registry,
    const char* module_id
)
{
    size_t i;

    if (registry == NULL ||
        module_id == NULL)
    {
        return NULL;
    }

    for (i = 0;
        i < registry->count;
        i++)
    {
        if (strcmp(
            registry->modules[i].descriptor.id,
            module_id
        ) == 0)
        {
            return &registry->modules[i];
        }
    }

    return NULL;
}


void rb_module_registry_unload_all(
    rb_module_registry_t* registry
)
{
    size_t i;

    if (registry == NULL)
    {
        return;
    }

    for (i = 0;
        i < registry->count;
        i++)
    {
        rb_module_record_t* record;

        record =
            &registry->modules[i];

        if (record->library_handle !=
            NULL)
        {
            if (record->descriptor.shutdown !=
                NULL)
            {
                record->descriptor.shutdown();
            }

            FreeLibrary(
                (HMODULE)
                record->library_handle
            );

            record->library_handle =
                NULL;
        }
    }
}