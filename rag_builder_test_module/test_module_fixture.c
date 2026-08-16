#include "module.h"

static rb_module_result_t rb_test_module_qualify(
    rb_module_qualification_result_t* result
)
{
    unsigned int tests_passed = 0;

    if (result == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    /*
     * Ten deterministic fixture tests.
     */
    if (1 == 1) tests_passed++;
    if (2 == 2) tests_passed++;
    if (3 == 3) tests_passed++;
    if (4 == 4) tests_passed++;
    if (5 == 5) tests_passed++;
    if (6 == 6) tests_passed++;
    if (7 == 7) tests_passed++;
    if (8 == 8) tests_passed++;
    if (9 == 9) tests_passed++;
    if (10 == 10) tests_passed++;

    result->tests_executed = 10;
    result->tests_passed = tests_passed;
    result->tests_failed =
        result->tests_executed -
        result->tests_passed;

    /*
     * Intentional negative validation.
     */
    result->negative_test_executed = 1;

    if (0 != 1)
    {
        result->negative_test_passed = 1;
    }
    else
    {
        result->negative_test_passed = 0;
    }

    if (result->tests_failed != 0 ||
        !result->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}

static const rb_module_descriptor_t descriptor =
{
    "RB-TEST-MODULE",
    "Qualification Fixture",

    0,
    1,
    0,

    RB_MODULE_API_MAJOR,
    RB_MODULE_API_MINOR,

    rb_test_module_qualify
};

__declspec(dllexport)
const rb_module_descriptor_t*
rb_module_get_descriptor(void)
{
    return &descriptor;
}
