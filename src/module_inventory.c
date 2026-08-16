#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>

#include "module_inventory.h"

#define RB_MODULE_INVENTORY_FILENAME \
    "module_inventory.conf"


static int rb_module_inventory_record_valid(
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

    if (record->qualification.tests_executed <
        RB_MODULE_MIN_TESTS)
    {
        return 0;
    }

    if (record->qualification.tests_passed !=
        record->qualification.tests_executed)
    {
        return 0;
    }

    if (record->qualification.tests_failed != 0)
    {
        return 0;
    }

    if (!record->qualification.negative_test_executed ||
        !record->qualification.negative_test_passed)
    {
        return 0;
    }

    return 1;
}


void rb_module_inventory_init(
    rb_module_inventory_t* inventory
)
{
    if (inventory == NULL)
    {
        return;
    }

    memset(
        inventory,
        0,
        sizeof(*inventory)
    );
}


rb_module_inventory_result_t
rb_module_inventory_configure(
    rb_module_inventory_t* inventory,
    const char* output_path
)
{
    int written;

    if (inventory == NULL ||
        output_path == NULL ||
        output_path[0] == '\0')
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

    if (written < 0 ||
        (size_t)written >= sizeof(inventory->path))
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
    FILE* file;
    char line[512];

    if (inventory == NULL ||
        inventory->path[0] == '\0')
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    inventory->count = 0;

    file = fopen(
        inventory->path,
        "r"
    );

    /*
     * Missing inventory is not an error.
     * It simply means no module has yet established
     * persistent qualification.
     */
    if (file == NULL)
    {
        return RB_MODULE_INVENTORY_OK;
    }

    while (fgets(
        line,
        sizeof(line),
        file
    ) != NULL)
    {
        rb_module_inventory_record_t record;

        int fields;

        memset(
            &record,
            0,
            sizeof(record)
        );

        fields = sscanf(
            line,
            "%63[^|]|%u|%u|%u|%u|%u|%u|%u|%u|%d|%d",
            record.module_id,

            &record.version_major,
            &record.version_minor,
            &record.version_patch,

            &record.core_api_major,
            &record.core_api_minor,

            &record.qualification.tests_executed,
            &record.qualification.tests_passed,
            &record.qualification.tests_failed,

            &record.qualification.negative_test_executed,
            &record.qualification.negative_test_passed
        );

        if (fields != 11)
        {
            fclose(file);

            inventory->count = 0;

            return RB_MODULE_INVENTORY_ERR_INVALID_FORMAT;
        }

        if (!rb_module_inventory_record_valid(
            &record
        ))
        {
            fclose(file);

            inventory->count = 0;

            return RB_MODULE_INVENTORY_ERR_INVALID_FORMAT;
        }

        if (inventory->count >=
            RB_MODULE_INVENTORY_MAX)
        {
            fclose(file);

            inventory->count = 0;

            return RB_MODULE_INVENTORY_ERR_FULL;
        }

        inventory->records[
            inventory->count
        ] = record;

        inventory->count++;
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
    FILE* file;
    size_t index;

    if (inventory == NULL ||
        inventory->path[0] == '\0')
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    file = fopen(
        inventory->path,
        "w"
    );

    if (file == NULL)
    {
        return RB_MODULE_INVENTORY_ERR_OPEN_FAILED;
    }

    for (index = 0;
         index < inventory->count;
         index++)
    {
        const rb_module_inventory_record_t* record;

        int written;

        record =
            &inventory->records[index];

        written = fprintf(
            file,
            "%s|%u|%u|%u|%u|%u|%u|%u|%u|%d|%d\n",

            record->module_id,

            record->version_major,
            record->version_minor,
            record->version_patch,

            record->core_api_major,
            record->core_api_minor,

            record->qualification.tests_executed,
            record->qualification.tests_passed,
            record->qualification.tests_failed,

            record->qualification.negative_test_executed,
            record->qualification.negative_test_passed
        );

        if (written < 0)
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
    const rb_module_qualification_result_t* qualification
)
{
    rb_module_inventory_record_t* record;

    size_t index;
    size_t length;

    if (inventory == NULL ||
        descriptor == NULL ||
        qualification == NULL)
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    record = NULL;

    for (index = 0;
         index < inventory->count;
         index++)
    {
        if (strcmp(
            inventory->records[index].module_id,
            descriptor->id
        ) == 0)
        {
            record =
                &inventory->records[index];

            break;
        }
    }

    if (record == NULL)
    {
        if (inventory->count >=
            RB_MODULE_INVENTORY_MAX)
        {
            return RB_MODULE_INVENTORY_ERR_FULL;
        }

        record =
            &inventory->records[
                inventory->count
            ];

        inventory->count++;
    }

    memset(
        record,
        0,
        sizeof(*record)
    );

    length = strlen(
        descriptor->id
    );

    if (length == 0 ||
        length >= sizeof(record->module_id))
    {
        return RB_MODULE_INVENTORY_ERR_INVALID_ARGUMENT;
    }

    memcpy(
        record->module_id,
        descriptor->id,
        length + 1
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

    if (!rb_module_inventory_record_valid(
        record
    ))
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
    const rb_module_descriptor_t* descriptor
)
{
    size_t index;

    if (inventory == NULL ||
        descriptor == NULL)
    {
        return NULL;
    }

    for (index = 0;
         index < inventory->count;
         index++)
    {
        const rb_module_inventory_record_t* record;

        record =
            &inventory->records[index];

        if (strcmp(
            record->module_id,
            descriptor->id
        ) != 0)
        {
            continue;
        }

        if (record->version_major !=
                descriptor->version_major ||
            record->version_minor !=
                descriptor->version_minor ||
            record->version_patch !=
                descriptor->version_patch)
        {
            return NULL;
        }

        if (record->core_api_major !=
                descriptor->required_core_api_major ||
            record->core_api_minor !=
                descriptor->required_core_api_minor)
        {
            return NULL;
        }

        if (!rb_module_inventory_record_valid(
            record
        ))
        {
            return NULL;
        }

        return record;
    }

    return NULL;
}


const char* rb_module_inventory_result_string(
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