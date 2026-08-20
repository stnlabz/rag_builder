#ifndef RAG_BUILDER_MODULE_DISCOVERY_H
#define RAG_BUILDER_MODULE_DISCOVERY_H

#include <stddef.h>
#include "module_registry.h"

typedef struct
{
    size_t directories_examined;
    size_t modules_discovered;
    size_t modules_rejected;
    size_t unknown_modules;
} rb_module_discovery_report_t;

rb_module_result_t rb_module_discovery_scan(
    rb_module_registry_t* registry,
    const char* modules_path,
    rb_module_discovery_report_t* report
);

#endif
