#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "markdown.h"

static int rb_markdown_is_line_end(
    char value
)
{
    return value == '\r' ||
        value == '\n' ||
        value == '\0';
}


static size_t rb_markdown_skip_line_end(
    const char* source,
    size_t source_length,
    size_t offset
)
{
    if (offset >= source_length)
    {
        return offset;
    }

    if (source[offset] == '\r')
    {
        offset++;

        if (offset < source_length &&
            source[offset] == '\n')
        {
            offset++;
        }

        return offset;
    }

    if (source[offset] == '\n')
    {
        offset++;
    }

    return offset;
}


static int rb_markdown_is_blank_line(
    const char* source,
    size_t start,
    size_t end
)
{
    size_t index;

    for (index = start;
        index < end;
        index++)
    {
        if (source[index] != ' ' &&
            source[index] != '\t')
        {
            return 0;
        }
    }

    return 1;
}


static rb_markdown_result_t rb_markdown_add_block(
    rb_markdown_document_t* document,
    rb_markdown_block_type_t type,
    unsigned int heading_level,
    size_t source_offset,
    size_t source_length,
    size_t content_offset,
    size_t content_length
)
{
    rb_markdown_block_t* block;

    if (document == NULL)
    {
        return RB_MARKDOWN_ERR_INVALID_ARGUMENT;
    }

    if (document->block_count >=
        RB_MARKDOWN_BLOCK_MAX)
    {
        return RB_MARKDOWN_ERR_TOO_MANY_BLOCKS;
    }

    block =
        &document->blocks[
            document->block_count
        ];

    memset(
        block,
        0,
        sizeof(*block)
    );

    block->type =
        type;

    block->heading_level =
        heading_level;

    block->source_offset =
        source_offset;

    block->source_length =
        source_length;

    block->content_offset =
        content_offset;

    block->content_length =
        content_length;

    document->block_count++;

    return RB_MARKDOWN_OK;
}


static unsigned int rb_markdown_heading_level(
    const char* source,
    size_t start,
    size_t end,
    size_t* content_start
)
{
    size_t index;
    unsigned int level = 0;

    if (source == NULL ||
        content_start == NULL)
    {
        return 0;
    }

    index = start;

    while (index < end &&
        source[index] == '#' &&
        level < 6)
    {
        level++;
        index++;
    }

    if (level == 0 ||
        index >= end ||
        source[index] != ' ')
    {
        return 0;
    }

    index++;

    if (index >= end)
    {
        return 0;
    }

    *content_start =
        index;

    return level;
}


static int rb_markdown_unordered_list(
    const char* source,
    size_t start,
    size_t end,
    size_t* content_start
)
{
    if (source == NULL ||
        content_start == NULL)
    {
        return 0;
    }

    if ((end - start) < 3)
    {
        return 0;
    }

    if ((source[start] == '-' ||
        source[start] == '*' ||
        source[start] == '+') &&
        source[start + 1] == ' ')
    {
        *content_start =
            start + 2;

        return 1;
    }

    return 0;
}


static int rb_markdown_ordered_list(
    const char* source,
    size_t start,
    size_t end,
    size_t* content_start
)
{
    size_t index;

    if (source == NULL ||
        content_start == NULL)
    {
        return 0;
    }

    index =
        start;

    if (index >= end ||
        !isdigit(
            (unsigned char)
            source[index]
        ))
    {
        return 0;
    }

    while (index < end &&
        isdigit(
            (unsigned char)
            source[index]
        ))
    {
        index++;
    }

    if (index >= end ||
        source[index] != '.')
    {
        return 0;
    }

    index++;

    if (index >= end ||
        source[index] != ' ')
    {
        return 0;
    }

    index++;

    if (index >= end)
    {
        return 0;
    }

    *content_start =
        index;

    return 1;
}


rb_markdown_result_t rb_markdown_parse(
    const char* source,
    size_t source_length,
    rb_markdown_document_t* document
)
{
    size_t offset;

    if (source == NULL ||
        document == NULL)
    {
        return RB_MARKDOWN_ERR_INVALID_ARGUMENT;
    }

    memset(
        document,
        0,
        sizeof(*document)
    );

    if (source_length == 0)
    {
        return RB_MARKDOWN_ERR_EMPTY_INPUT;
    }

    offset = 0;

    while (offset < source_length)
    {
        size_t line_start;
        size_t line_end;
        size_t content_start;

        unsigned int heading_level;

        rb_markdown_result_t result;

        line_start =
            offset;

        line_end =
            offset;

        while (line_end < source_length &&
            !rb_markdown_is_line_end(
                source[line_end]
            ))
        {
            line_end++;
        }

        if (rb_markdown_is_blank_line(
            source,
            line_start,
            line_end
        ))
        {
            offset =
                rb_markdown_skip_line_end(
                    source,
                    source_length,
                    line_end
                );

            continue;
        }

        content_start =
            line_start;

        heading_level =
            rb_markdown_heading_level(
                source,
                line_start,
                line_end,
                &content_start
            );

        if (heading_level != 0)
        {
            result =
                rb_markdown_add_block(
                    document,
                    RB_MARKDOWN_BLOCK_HEADING,
                    heading_level,

                    line_start,
                    line_end - line_start,

                    content_start,
                    line_end - content_start
                );
        }
        else if (rb_markdown_unordered_list(
            source,
            line_start,
            line_end,
            &content_start
        ))
        {
            result =
                rb_markdown_add_block(
                    document,
                    RB_MARKDOWN_BLOCK_UNORDERED_LIST_ITEM,
                    0,

                    line_start,
                    line_end - line_start,

                    content_start,
                    line_end - content_start
                );
        }
        else if (rb_markdown_ordered_list(
            source,
            line_start,
            line_end,
            &content_start
        ))
        {
            result =
                rb_markdown_add_block(
                    document,
                    RB_MARKDOWN_BLOCK_ORDERED_LIST_ITEM,
                    0,

                    line_start,
                    line_end - line_start,

                    content_start,
                    line_end - content_start
                );
        }
        else
        {
            result =
                rb_markdown_add_block(
                    document,
                    RB_MARKDOWN_BLOCK_PARAGRAPH,
                    0,

                    line_start,
                    line_end - line_start,

                    line_start,
                    line_end - line_start
                );
        }

        if (result != RB_MARKDOWN_OK)
        {
            return result;
        }

        offset =
            rb_markdown_skip_line_end(
                source,
                source_length,
                line_end
            );
    }

    if (document->block_count == 0)
    {
        return RB_MARKDOWN_ERR_EMPTY_INPUT;
    }

    return RB_MARKDOWN_OK;
}


/*
 * Module qualification
 *
 * The module executes these tests.
 * Core invokes the suite and evaluates
 * the resulting evidence.
 */
static rb_module_result_t rb_markdown_qualify(
    rb_module_qualification_result_t* result
)
{
    rb_markdown_document_t document;

    rb_markdown_result_t parse_result;

    unsigned int passed = 0;

    const char* source;

    if (result == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    memset(
        result,
        0,
        sizeof(*result)
    );

    /*
     * Test 01
     * Valid heading recognized.
     */
    source =
        "# Heading";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_HEADING &&
        document.blocks[0].heading_level == 1)
    {
        passed++;
    }


    /*
     * Test 02
     * Heading hierarchy preserved.
     */
    source =
        "### Heading";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].heading_level == 3)
    {
        passed++;
    }


    /*
     * Test 03
     * Paragraph recognized.
     */
    source =
        "Paragraph text";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_PARAGRAPH)
    {
        passed++;
    }


    /*
     * Test 04
     * Unordered list recognized.
     */
    source =
        "- Item";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_UNORDERED_LIST_ITEM)
    {
        passed++;
    }


    /*
     * Test 05
     * Ordered list recognized.
     */
    source =
        "1. Item";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_ORDERED_LIST_ITEM)
    {
        passed++;
    }


    /*
     * Test 06
     * Source order preserved.
     */
    source =
        "# Heading\nParagraph";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 2 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_HEADING &&
        document.blocks[1].type ==
        RB_MARKDOWN_BLOCK_PARAGRAPH)
    {
        passed++;
    }


    /*
     * Test 07
     * Source offset preserved.
     */
    source =
        "# Heading\nParagraph";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 2 &&
        document.blocks[0].source_offset == 0 &&
        document.blocks[1].source_offset == 10)
    {
        passed++;
    }


    /*
     * Test 08
     * Content offset preserved.
     */
    source =
        "## Test Heading";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].content_offset == 3 &&
        document.blocks[0].content_length == 12)
    {
        passed++;
    }


    /*
     * Test 09
     * Blank lines ignored deterministically.
     */
    source =
        "# Heading\n\nParagraph";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 2)
    {
        passed++;
    }


    /*
     * Test 10
     * Minimal valid Markdown handled.
     */
    source =
        "x";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_PARAGRAPH &&
        document.blocks[0].content_offset == 0 &&
        document.blocks[0].content_length == 1)
    {
        passed++;
    }


    result->tests_executed =
        10;

    result->tests_passed =
        passed;

    result->tests_failed =
        result->tests_executed -
        result->tests_passed;


    /*
     * Mandatory intentional negative validation.
     *
     * Empty input must fail safely.
     */
    result->negative_test_executed =
        1;

    parse_result =
        rb_markdown_parse(
            "",
            0,
            &document
        );

    if (parse_result ==
        RB_MARKDOWN_ERR_EMPTY_INPUT)
    {
        result->negative_test_passed =
            1;
    }
    else
    {
        result->negative_test_passed =
            0;
    }


    if (result->tests_failed != 0 ||
        !result->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}


static const rb_module_descriptor_t
rb_markdown_descriptor =
{
    RB_MARKDOWN_MODULE_ID,
    RB_MARKDOWN_MODULE_NAME,

    RB_MARKDOWN_VERSION_MAJOR,
    RB_MARKDOWN_VERSION_MINOR,
    RB_MARKDOWN_VERSION_PATCH,

    RB_MODULE_API_MAJOR,
    RB_MODULE_API_MINOR,

    rb_markdown_qualify
};


const rb_module_descriptor_t*
rb_markdown_get_descriptor(void)
{
    return &rb_markdown_descriptor;
}


const char* rb_markdown_result_string(
    rb_markdown_result_t result
)
{
    switch (result)
    {
    case RB_MARKDOWN_OK:
        return "OK";

    case RB_MARKDOWN_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_MARKDOWN_ERR_EMPTY_INPUT:
        return "EMPTY_INPUT";

    case RB_MARKDOWN_ERR_TOO_MANY_BLOCKS:
        return "TOO_MANY_BLOCKS";

    case RB_MARKDOWN_ERR_INVALID_FORMAT:
        return "INVALID_FORMAT";

    default:
        return "UNKNOWN";
    }
}


const char* rb_markdown_block_type_string(
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

    case RB_MARKDOWN_BLOCK_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}