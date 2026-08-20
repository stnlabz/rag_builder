#ifndef RAG_BUILDER_CHUNKER_H
#define RAG_BUILDER_CHUNKER_H

#include "module.h"

#define RB_CHUNKER_MODULE_ID       "RB-CHUNKER"
#define RB_CHUNKER_MODULE_NAME     "Chunker"

#define RB_CHUNKER_VERSION_MAJOR   0
#define RB_CHUNKER_VERSION_MINOR   1
#define RB_CHUNKER_VERSION_PATCH   1

/*
 * Combined non-atomic retrieval chunks are limited
 * by UTF-8 byte count.
 *
 * This is not a tokenizer limit.
 *
 * Fenced code blocks and blockquotes remain atomic
 * even when their content exceeds this value.
 */
#define RB_CHUNKER_MAX_COMBINED_BYTES 1600

#endif