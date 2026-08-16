#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "core.h"

int main(void)
{
    rb_core_t core = {
        RB_CORE_STATE_UNINITIALIZED,
        RB_OK
    };

    rb_config_t config;
    rb_config_result_t config_result;
    rb_result_t result;

    config_result = rb_config_load(
        "config/rag_builder.conf",
        &config
    );

    if (config_result != RB_CONFIG_OK)
    {
        fprintf(
            stderr,
            "[ERR] Configuration load failed: %s\n",
            rb_config_result_string(config_result)
        );

        return EXIT_FAILURE;
    }

    config_result = rb_config_validate(&config);

    if (config_result != RB_CONFIG_OK)
    {
        fprintf(
            stderr,
            "[ERR] Configuration validation failed: %s\n",
            rb_config_result_string(config_result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_init(&core, &config);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core initialization failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    result = rb_core_run(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core execution failed: %s\n",
            rb_result_string(result)
        );

        core.state = RB_CORE_STATE_FAILED;
        (void)rb_core_shutdown(&core);

        return EXIT_FAILURE;
    }

    result = rb_core_shutdown(&core);

    if (result != RB_OK)
    {
        fprintf(
            stderr,
            "[ERR] Core shutdown failed: %s\n",
            rb_result_string(result)
        );

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}