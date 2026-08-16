/* tests/core_state_test.c */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>

#include "acl_fixture_win.h"
#include "config.h"
#include "core.h"
#include "platform.h"

#define TEST_CONFIG_VALID        "rb_test_valid.conf"
#define TEST_CONFIG_MISSING      "rb_test_missing.conf"
#define TEST_CONFIG_NO_FIELD     "rb_test_no_field.conf"
#define TEST_CONFIG_UNKNOWN      "rb_test_unknown.conf"
#define TEST_CONFIG_DUPLICATE    "rb_test_duplicate.conf"
#define TEST_CONFIG_OVERLENGTH   "rb_test_overlength.conf"
#define TEST_CONFIG_BAD_LEVEL    "rb_test_bad_level.conf"

#define TEST_SOURCE_UNREADABLE   "rb_test_unreadable_source"
#define TEST_OUTPUT_UNWRITABLE   "rb_test_unwritable_output"


static int write_test_file(
    const char* path,
    const char* content
)
{
    FILE* file;

    if (path == NULL || content == NULL)
    {
        return EXIT_FAILURE;
    }

    file = fopen(path, "w");

    if (file == NULL)
    {
        fprintf(
            stderr,
            "[FAIL] Unable to create test fixture: %s\n",
            path
        );

        return EXIT_FAILURE;
    }

    if (fputs(content, file) == EOF)
    {
        fclose(file);

        fprintf(
            stderr,
            "[FAIL] Unable to write test fixture: %s\n",
            path
        );

        return EXIT_FAILURE;
    }

    if (fclose(file) != 0)
    {
        fprintf(
            stderr,
            "[FAIL] Unable to close test fixture: %s\n",
            path
        );

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


static void remove_test_file(
    const char* path
)
{
    if (path != NULL)
    {
        (void)remove(path);
    }
}


static int test_q01_q04_lifecycle(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_config_t config = {
        ".",
        ".",
        "INFO"
    };

    rb_result_t result;

    result = rb_core_init(&core, &config);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q01 core initialization failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q01 core initialized successfully\n"
    );

    if (core.state != RB_CORE_STATE_READY)
    {
        fprintf(
            stderr,
            "[FAIL] Q02 expected READY, received: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q02 core reached READY after initialization\n"
    );

    result = rb_core_run(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q03 core execution failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    if (core.state != RB_CORE_STATE_RUNNING)
    {
        fprintf(
            stderr,
            "[FAIL] Q03 expected RUNNING, received: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q03 core transitioned READY -> RUNNING\n"
    );

    result = rb_core_shutdown(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q04 core shutdown failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    if (core.state != RB_CORE_STATE_STOPPED)
    {
        fprintf(
            stderr,
            "[FAIL] Q04 expected STOPPED, received: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q04 controlled shutdown completed: STOPPED\n"
    );

    return EXIT_SUCCESS;
}


static int test_q05_invalid_transition(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_result_t result;

    result = rb_core_run(&core);

    if (result != RB_ERR_INVALID_STATE)
    {
        fprintf(
            stderr,
            "[FAIL] Q05 expected INVALID_STATE, received: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    if (core.state != RB_CORE_STATE_UNINITIALIZED)
    {
        fprintf(
            stderr,
            "[FAIL] Q05 core state changed unexpectedly: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q05 invalid state transition rejected: %s\n",
        rb_result_string(result)
    );

    printf(
        "[PASS] Q05 core state preserved: %s\n",
        rb_core_state_string(core.state)
    );

    return EXIT_SUCCESS;
}


static int test_q06_valid_config(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_VALID,
        "source_path=.\n"
        "output_path=.\n"
        "log_level=INFO\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_VALID,
        &config
    );

    if (result != RB_CONFIG_OK)
    {
        remove_test_file(TEST_CONFIG_VALID);

        fprintf(
            stderr,
            "[FAIL] Q06 valid configuration rejected: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_config_validate(&config);

    remove_test_file(TEST_CONFIG_VALID);

    if (result != RB_CONFIG_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q06 valid configuration failed validation: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q06 valid configuration loaded and validated\n"
    );

    return EXIT_SUCCESS;
}


static int test_q07_missing_config(void)
{
    rb_config_t config;
    rb_config_result_t result;

    remove_test_file(TEST_CONFIG_MISSING);

    result = rb_config_load(
        TEST_CONFIG_MISSING,
        &config
    );

    if (result != RB_CONFIG_ERR_OPEN_FAILED)
    {
        fprintf(
            stderr,
            "[FAIL] Q07 expected OPEN_FAILED, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q07 missing configuration rejected: OPEN_FAILED\n"
    );

    return EXIT_SUCCESS;
}


static int test_q08_missing_required_field(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_NO_FIELD,
        "source_path=.\n"
        "output_path=.\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_NO_FIELD,
        &config
    );

    remove_test_file(TEST_CONFIG_NO_FIELD);

    if (result != RB_CONFIG_ERR_MISSING_FIELD)
    {
        fprintf(
            stderr,
            "[FAIL] Q08 expected MISSING_FIELD, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q08 missing required field rejected: MISSING_FIELD\n"
    );

    return EXIT_SUCCESS;
}


static int test_q09_unknown_key(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_UNKNOWN,
        "source_path=.\n"
        "output_path=.\n"
        "log_level=INFO\n"
        "banana=YES\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_UNKNOWN,
        &config
    );

    remove_test_file(TEST_CONFIG_UNKNOWN);

    if (result != RB_CONFIG_ERR_INVALID_FORMAT)
    {
        fprintf(
            stderr,
            "[FAIL] Q09 expected INVALID_FORMAT, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q09 unknown configuration key rejected: INVALID_FORMAT\n"
    );

    return EXIT_SUCCESS;
}


static int test_q10_duplicate_key(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_DUPLICATE,
        "source_path=.\n"
        "output_path=.\n"
        "log_level=INFO\n"
        "log_level=INFO\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_DUPLICATE,
        &config
    );

    remove_test_file(TEST_CONFIG_DUPLICATE);

    if (result != RB_CONFIG_ERR_INVALID_FORMAT)
    {
        fprintf(
            stderr,
            "[FAIL] Q10 expected INVALID_FORMAT, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q10 duplicate configuration key rejected: INVALID_FORMAT\n"
    );

    return EXIT_SUCCESS;
}


static int test_q11_overlength_value(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_OVERLENGTH,
        "source_path=.\n"
        "output_path=.\n"
        "log_level=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_OVERLENGTH,
        &config
    );

    remove_test_file(TEST_CONFIG_OVERLENGTH);

    if (result != RB_CONFIG_ERR_VALUE_TOO_LONG)
    {
        fprintf(
            stderr,
            "[FAIL] Q11 expected VALUE_TOO_LONG, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q11 overlength configuration value rejected: VALUE_TOO_LONG\n"
    );

    return EXIT_SUCCESS;
}


static int test_q12_unsupported_log_level(void)
{
    rb_config_t config;
    rb_config_result_t result;

    if (write_test_file(
        TEST_CONFIG_BAD_LEVEL,
        "source_path=.\n"
        "output_path=.\n"
        "log_level=CHAOS\n"
    ) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    result = rb_config_load(
        TEST_CONFIG_BAD_LEVEL,
        &config
    );

    if (result != RB_CONFIG_OK)
    {
        remove_test_file(TEST_CONFIG_BAD_LEVEL);

        fprintf(
            stderr,
            "[FAIL] Q12 configuration could not be loaded: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_config_validate(&config);

    remove_test_file(TEST_CONFIG_BAD_LEVEL);

    if (result != RB_CONFIG_ERR_INVALID_FORMAT)
    {
        fprintf(
            stderr,
            "[FAIL] Q12 expected INVALID_FORMAT, received: %s\n",
            rb_config_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q12 unsupported log level rejected: INVALID_FORMAT\n"
    );

    return EXIT_SUCCESS;
}


static int test_q13_valid_source(void)
{
    rb_platform_result_t result;

    result = rb_platform_validate_readable_directory(".");

    if (result != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q13 valid source rejected: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q13 valid readable source accepted\n"
    );

    return EXIT_SUCCESS;
}


static int test_q14_missing_source(void)
{
    rb_platform_result_t result;

    result = rb_platform_validate_readable_directory(
        "rb_test_source_does_not_exist"
    );

    if (result != RB_PLATFORM_ERR_NOT_FOUND)
    {
        fprintf(
            stderr,
            "[FAIL] Q14 expected NOT_FOUND, received: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q14 missing source rejected: NOT_FOUND\n"
    );

    return EXIT_SUCCESS;
}


static int test_q15_unreadable_source(void)
{
    rb_platform_result_t result;

    if (!rb_test_acl_create_unreadable_directory(
        TEST_SOURCE_UNREADABLE
    ))
    {
        fprintf(
            stderr,
            "[FAIL] Q15 could not create unreadable source fixture\n"
        );

        return EXIT_FAILURE;
    }

    result = rb_platform_validate_readable_directory(
        TEST_SOURCE_UNREADABLE
    );

    if (!rb_test_acl_cleanup_directory(
        TEST_SOURCE_UNREADABLE
    ))
    {
        fprintf(
            stderr,
            "[FAIL] Q15 fixture cleanup failed\n"
        );

        return EXIT_FAILURE;
    }

    if (result != RB_PLATFORM_ERR_NOT_READABLE)
    {
        fprintf(
            stderr,
            "[FAIL] Q15 expected NOT_READABLE, received: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q15 unreadable source rejected: NOT_READABLE\n"
    );

    return EXIT_SUCCESS;
}


static int test_q16_valid_output(void)
{
    rb_platform_result_t result;

    result = rb_platform_validate_writable_directory(".");

    if (result != RB_PLATFORM_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q16 valid output rejected: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q16 valid writable output accepted\n"
    );

    return EXIT_SUCCESS;
}


static int test_q17_missing_output(void)
{
    rb_platform_result_t result;

    result = rb_platform_validate_writable_directory(
        "rb_test_output_does_not_exist"
    );

    if (result != RB_PLATFORM_ERR_NOT_FOUND)
    {
        fprintf(
            stderr,
            "[FAIL] Q17 expected NOT_FOUND, received: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q17 missing output rejected: NOT_FOUND\n"
    );

    return EXIT_SUCCESS;
}


static int test_q18_unwritable_output(void)
{
    rb_platform_result_t result;

    if (!rb_test_acl_create_unwritable_directory(
        TEST_OUTPUT_UNWRITABLE
    ))
    {
        fprintf(
            stderr,
            "[FAIL] Q18 could not create unwritable output fixture\n"
        );

        return EXIT_FAILURE;
    }

    result = rb_platform_validate_writable_directory(
        TEST_OUTPUT_UNWRITABLE
    );

    if (!rb_test_acl_cleanup_directory(
        TEST_OUTPUT_UNWRITABLE
    ))
    {
        fprintf(
            stderr,
            "[FAIL] Q18 fixture cleanup failed\n"
        );

        return EXIT_FAILURE;
    }

    if (result != RB_PLATFORM_ERR_NOT_WRITABLE)
    {
        fprintf(
            stderr,
            "[FAIL] Q18 expected NOT_WRITABLE, received: %s\n",
            rb_platform_result_string(result)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q18 unwritable output rejected: NOT_WRITABLE\n"
    );

    return EXIT_SUCCESS;
}


static int test_q19_environment_failure(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_config_t config = {
        "rb_test_source_does_not_exist",
        ".",
        "INFO"
    };

    rb_result_t result;

    result = rb_core_init(&core, &config);

    if (result != RB_ERR_ENVIRONMENT)
    {
        fprintf(
            stderr,
            "[FAIL] Q19 expected ENVIRONMENT_ERROR, received: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    if (core.state != RB_CORE_STATE_FAILED)
    {
        fprintf(
            stderr,
            "[FAIL] Q19 expected FAILED state, received: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q19 environment failure returned ENVIRONMENT_ERROR\n"
    );

    printf(
        "[PASS] Q19 core entered FAILED state\n"
    );

    return EXIT_SUCCESS;
}


static int test_q20_complete_lifecycle(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_config_t config = {
        ".",
        ".",
        "INFO"
    };

    rb_result_t result;

    result = rb_core_init(&core, &config);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q20 initialization failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_run(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q20 execution failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_shutdown(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[FAIL] Q20 shutdown failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    if (core.state != RB_CORE_STATE_STOPPED)
    {
        fprintf(
            stderr,
            "[FAIL] Q20 expected STOPPED, received: %s\n",
            rb_core_state_string(core.state)
        );

        return EXIT_FAILURE;
    }

    printf(
        "[PASS] Q20 complete lifecycle exited cleanly\n"
    );

    return EXIT_SUCCESS;
}


int main(void)
{
    int failures = 0;

    printf(
        "rag_builder Core Qualification Tests\n"
    );

    printf(
        "------------------------------------\n"
    );

    if (test_q01_q04_lifecycle() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q05_invalid_transition() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q06_valid_config() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q07_missing_config() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q08_missing_required_field() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q09_unknown_key() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q10_duplicate_key() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q11_overlength_value() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q12_unsupported_log_level() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q13_valid_source() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q14_missing_source() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q15_unreadable_source() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q16_valid_output() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q17_missing_output() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q18_unwritable_output() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q19_environment_failure() != EXIT_SUCCESS)
    {
        failures++;
    }

    if (test_q20_complete_lifecycle() != EXIT_SUCCESS)
    {
        failures++;
    }

    printf(
        "------------------------------------\n"
    );

    if (failures != 0)
    {
        printf(
            "[RESULT] FAILED - %d test group(s) failed\n",
            failures
        );

        return EXIT_FAILURE;
    }

    printf(
        "[RESULT] PASS - Q01 through Q20\n"
    );

    return EXIT_SUCCESS;
}