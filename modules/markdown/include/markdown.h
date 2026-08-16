#ifndef RAG_BUILDER_MARKDOWN_H
#define RAG_BUILDER_MARKDOWN_H

#include <stddef.h>

#include "module.h"

#define RB_MARKDOWN_MODULE_ID      "RB-MARKDOWN"
#define RB_MARKDOWN_MODULE_NAME    "Markdown"

#define RB_MARKDOWN_VERSION_MAJOR  0
#define RB_MARKDOWN_VERSION_MINOR  1
#define RB_MARKDOWN_VERSION_PATCH  0

#define RB_MARKDOWN_BLOCK_MAX      1024

typedef enum
{
    RB_MARKDOWN_OK = 0,

    RB_MARKDOWN_ERR_INVALID_ARGUMENT,
    RB_MARKDOWN_ERR_EMPTY_INPUT,
    RB_MARKDOWN_ERR_TOO_MANY_BLOCKS,
    RB_MARKDOWN_ERR_INVALID_FORMAT

} rb_markdown_result_t;

typedef enum
{
    RB_MARKDOWN_BLOCK_UNKNOWN = 0,
    RB_MARKDOWN_BLOCK_HEADING,
    RB_MARKDOWN_BLOCK_PARAGRAPH,
    RB_MARKDOWN_BLOCK_UNORDERED_LIST_ITEM,
    RB_MARKDOWN_BLOCK_ORDERED_LIST_ITEM

} rb_markdown_block_type_t;

typedef struct
{
    rb_markdown_block_type_t type;

    unsigned int heading_level;

    /*
     * Full source span for the block.
     */
    size_t source_offset;
    size_t source_length;

    /*
     * Content span inside the source.
     *
     * Example:
     *
     * "# Heading"
     *
     * source_offset  = 0
     * source_length  = 9
     *
     * content_offset = 2
     * content_length = 7
     */
    size_t content_offset;
    size_t content_length;

} rb_markdown_block_t;

typedef struct
{
    rb_markdown_block_t blocks[RB_MARKDOWN_BLOCK_MAX];

    size_t block_count;

} rb_markdown_document_t;

rb_markdown_result_t rb_markdown_parse(
    const char* source,
    size_t source_length,
    rb_markdown_document_t* document
);

const rb_module_descriptor_t*
rb_markdown_get_descriptor(void);

const char* rb_markdown_result_string(
    rb_markdown_result_t result
);

const char* rb_markdown_block_type_string(
    rb_markdown_block_type_t type
);

#endif