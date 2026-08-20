#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>

#include "module_inventory.h"

#define RB_MODULE_INVENTORY_FILENAME "module_inventory.conf"


static int
rb_hash_valid(
    const char* hash
)
{
    size_t i;

    if (hash == NULL || strlen(hash) != 64)
    {
        return 0;
    }

    for (i = 0; i < 64; i++)
    {
        if (!(
            (hash[i] >= '0' && hash[i] <= '9') ||
            (hash[i] >= 'a' && hash[i] <= 'f')
            ))
        {
            return 0;
        }
    }

    return 1;
}


static int
rb_record_valid(
    const rb_module_inventory_record_t* record
)
{
    if (record == NULL)
    {
        return 0;
    }

    if (record->module_id[0] == '\0')
    {
        return 0;
    }

    if (!rb_hash_valid(record->binary_sha256))
    {
        return 0;
    }

    if (record->qualification.tests_executed < RB_MODULE_MIN_TESTS)
    {
        return 0;
    }

    if (
        record->qualification.tests_passed !=
        record->qualification.tests_executed
        )
    {
        return 0;
    }

    if (record->qualification.tests_failed != 0)
    {
        return 0;
    }

    if (
        record->qualification.tests_passed +
        record->qualification.tests_failed !=
        record->qualification.tests_executed
        )
    {
        return 0;
    }

    if (!record->qualification.negative_test_executed)
    {
        return 0;
    }

    if (!record->qualification.negative_test_passed)
    {
        return 0;
    }

    return 1;
}


void
rb_module_inventory_init(
    rb_module_inventory_t* inventory
)
{
    if (inventory != NULL)
    {
        memset(
            inventory,
            0,
            sizeof(*inventory)
        );
    }
}


rb_module_inventory_result_t
rb_module_inventory_configure(
    rb_module_inventory_t* inventory,
    const char* output_path
)
{
    int written;

    if (
        inventory == NULL ||
        output_path == NULL ||
        output_path[0] == '\0'
        )
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    written = snprintf(
        inventory->path,
        sizeof(inventory->path),
        "%s\\%s",
        output_path,
        RB_MODULE_INVENTORY_FILENAME
    );

    if (
        written < 0 ||
        (size_t)written >= sizeof(inventory->path)
        )
    {
        return RB_MODULE_INVENTORY_ERR_PATH_TOO_LONG;
    }

    return RB_MODULE_INVENTORY_OK;
}


rb_module_inventory_result_t
rb_module_inventory_load(
    rb_module_inventory_t* inventory
)
{
    FILE* file = NULL;
    char line[768];

    if (
        inventory == NULL ||
        inventory->path[0] == '\0'
        )
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    inventory->count = 0;

    if (
        fopen_s(
            &file,
            inventory->path,
            "r"
        ) != 0 ||
        file == NULL
        )
    {
        return RB_MODULE_INVENTORY_OK;
    }

    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
        )
    {
        rb_module_inventory_record_t record;
        int fields;

        memset(
            &record,
            0,
            sizeof(record)
        );

        fields = sscanf_s(
            line,
            "%63[^|]|%u|%u|%u|%u|%u|%64[^|]|%u|%u|%u|%d|%d",
            record.module_id,
            (unsigned)sizeof(record.module_id),
            &record.version_major,
            &record.version_minor,
            &record.version_patch,
            &record.core_api_major,
            &record.core_api_minor,
            record.binary_sha256,
            (unsigned)sizeof(record.binary_sha256),
            &record.qualification.tests_executed,
            &record.qualification.tests_passed,
            &record.qualification.tests_failed,
            &record.qualification.negative_test_executed,
            &record.qualification.negative_test_passed
        );

        if (
            fields != 12 ||
            !rb_record_valid(&record) ||
            inventory->count >= RB_MODULE_INVENTORY_MAX
            )
        {
            fclose(file);

            inventory->count = 0;

            if (
                fields != 12 ||
                !rb_record_valid(&record)
                )
            {
                return RB_MODULE_INVENTORY_ERR_INVALID_FORMAT;
            }

            return RB_MODULE_INVENTORY_ERR_FULL;
        }

        inventory->records[inventory->count++] = record;
    }

    if (ferror(file))
    {
        fclose(file);

        inventory->count = 0;

        return RB_MODULE_INVENTORY_ERR_READ_FAILED;
    }

    fclose(file);

    return RB_MODULE_INVENTORY_OK;
}


static rb_module_inventory_result_t
rb_module_inventory_save(
    const rb_module_inventory_t* inventory
)
{
    FILE* file = NULL;
    size_t i;

    if (
        inventory == NULL ||
        inventory->path[0] == '\0'
        )
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    if (
        fopen_s(
            &file,
            inventory->path,
            "w"
        ) != 0 ||
        file == NULL
        )
    {
        return RB_MODULE_INVENTORY_ERR_OPEN_FAILED;
    }

    for (i = 0; i < inventory->count; i++)
    {
        const rb_module_inventory_record_t* record;

        record = &inventory->records[i];

        if (
            fprintf(
                file,
                "%s|%u|%u|%u|%u|%u|%s|%u|%u|%u|%d|%d\n",
                record->module_id,
                record->version_major,
                record->version_minor,
                record->version_patch,
                record->core_api_major,
                record->core_api_minor,
                record->binary_sha256,
                record->qualification.tests_executed,
                record->qualification.tests_passed,
                record->qualification.tests_failed,
                record->qualification.negative_test_executed,
                record->qualification.negative_test_passed
            ) < 0
            )
        {
            fclose(file);

            return RB_MODULE_INVENTORY_ERR_WRITE_FAILED;
        }
    }

    if (fflush(file) != 0)
    {
        fclose(file);

        return RB_MODULE_INVENTORY_ERR_WRITE_FAILED;
    }

    if (fclose(file) != 0)
    {
        return RB_MODULE_INVENTORY_ERR_WRITE_FAILED;
    }

    return RB_MODULE_INVENTORY_OK;
}


rb_module_inventory_result_t
rb_module_inventory_store(
    rb_module_inventory_t* inventory,
    const rb_module_descriptor_t* descriptor,
    const char* binary_sha256,
    const rb_module_qualification_result_t* qualification
)
{
    rb_module_inventory_record_t* record = NULL;
    size_t i;

    if (
        inventory == NULL ||
        descriptor == NULL ||
        binary_sha256 == NULL ||
        qualification == NULL
        )
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    for (i = 0; i < inventory->count; i++)
    {
        if (
            strcmp(
                inventory->records[i].module_id,
                descriptor->id
            ) == 0
            )
        {
            record = &inventory->records[i];
            break;
        }
    }

    if (record == NULL)
    {
        if (
            inventory->count >=
            RB_MODULE_INVENTORY_MAX
            )
        {
            return RB_MODULE_INVENTORY_ERR_FULL;
        }

        record =
            &inventory->records[
                inventory->count++
            ];
    }

    memset(
        record,
        0,
        sizeof(*record)
    );

    strcpy_s(
        record->module_id,
        sizeof(record->module_id),
        descriptor->id
    );

    strcpy_s(
        record->binary_sha256,
        sizeof(record->binary_sha256),
        binary_sha256
    );

    record->version_major =
        descriptor->version_major;

    record->version_minor =
        descriptor->version_minor;

    record->version_patch =
        descriptor->version_patch;

    record->core_api_major =
        descriptor->required_core_api_major;

    record->core_api_minor =
        descriptor->required_core_api_minor;

    record->qualification =
        *qualification;

    if (!rb_record_valid(record))
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_FORMAT;
    }

    return rb_module_inventory_save(
        inventory
    );
}


const rb_module_inventory_record_t*
rb_module_inventory_find(
    const rb_module_inventory_t* inventory,
    const rb_module_descriptor_t* descriptor,
    const char* binary_sha256
)
{
    size_t i;

    if (
        inventory == NULL ||
        descriptor == NULL ||
        binary_sha256 == NULL
        )
    {
        return NULL;
    }

    for (i = 0; i < inventory->count; i++)
    {
        const rb_module_inventory_record_t* record;

        record = &inventory->records[i];

        if (
            strcmp(
                record->module_id,
                descriptor->id
            ) != 0
            )
        {
            continue;
        }

        if (
            record->version_major !=
            descriptor->version_major ||
            record->version_minor !=
            descriptor->version_minor ||
            record->version_patch !=
            descriptor->version_patch ||
            record->core_api_major !=
            descriptor->required_core_api_major ||
            record->core_api_minor !=
            descriptor->required_core_api_minor ||
            strcmp(
                record->binary_sha256,
                binary_sha256
            ) != 0 ||
            !rb_record_valid(record)
            )
        {
            return NULL;
        }

        return record;
    }

    return NULL;
}


const char*
rb_module_inventory_result_string(
    rb_module_inventory_result_t result
)
{
    switch (result)
    {
    case RB_MODULE_INVENTORY_OK:
        return "OK";

    case RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_MODULE_INVENTORY_ERR_PATH_TOO_LONG:
        return "PATH_TOO_LONG";

    case RB_MODULE_INVENTORY_ERR_OPEN_FAILED:
        return "OPEN_FAILED";

    case RB_MODULE_INVENTORY_ERR_READ_FAILED:
        return "READ_FAILED";

    case RB_MODULE_INVENTORY_ERR_WRITE_FAILED:
        return "WRITE_FAILED";

    case RB_MODULE_INVENTORY_ERR_INVALID_FORMAT:
        return "INVALID_FORMAT";

    case RB_MODULE_INVENTORY_ERR_FULL:
        return "FULL";

    default:
        return "UNKNOWN";
    }
}