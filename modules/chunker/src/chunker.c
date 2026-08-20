#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "chunker.h"

#define RB_CHUNKER_INPUT_SUFFIX          ".rag.json"
#define RB_CHUNKER_OUTPUT_SUFFIX         ".chunks.json"

#define RB_CHUNKER_EXPECTED_FORMAT       "RB-MARKDOWN"
#define RB_CHUNKER_EXPECTED_FORMAT_VER   2

#define RB_CHUNKER_PATH_MAX              RB_MODULE_PATH_MAX
#define RB_CHUNKER_INITIAL_CAPACITY      16


typedef enum
{
    RB_CHUNKER_OK = 0,

    RB_CHUNKER_ERR_INVALID_ARGUMENT,
    RB_CHUNKER_ERR_MEMORY,
    RB_CHUNKER_ERR_OPEN_FAILED,
    RB_CHUNKER_ERR_READ_FAILED,
    RB_CHUNKER_ERR_WRITE_FAILED,
    RB_CHUNKER_ERR_INVALID_JSON,
    RB_CHUNKER_ERR_INVALID_CONTRACT,
    RB_CHUNKER_ERR_INVALID_STRUCTURE,
    RB_CHUNKER_ERR_PATH_TOO_LONG

} rb_chunker_result_t;


typedef enum
{
    RB_CHUNKER_BLOCK_UNKNOWN = 0,
    RB_CHUNKER_BLOCK_HEADING,
    RB_CHUNKER_BLOCK_PARAGRAPH,
    RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM,
    RB_CHUNKER_BLOCK_ORDERED_LIST_ITEM,
    RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK,
    RB_CHUNKER_BLOCK_BLOCKQUOTE

} rb_chunker_block_type_t;


typedef struct
{
    size_t index;

    rb_chunker_block_type_t type;

    unsigned int heading_level;

    int has_parent_heading;
    size_t parent_heading_index;

    size_t source_offset;
    size_t source_length;

    size_t content_offset;
    size_t content_length;

    char* info;
    char* text;

} rb_chunker_source_block_t;


typedef struct
{
    char source_filename[RB_CHUNKER_PATH_MAX];

    size_t source_size;

    rb_chunker_source_block_t* blocks;
    size_t block_count;
    size_t block_capacity;

} rb_chunker_source_document_t;


typedef struct
{
    size_t index;

    size_t* source_blocks;
    size_t source_block_count;
    size_t source_block_capacity;

    rb_chunker_block_type_t* block_types;
    size_t block_type_count;
    size_t block_type_capacity;

    int has_parent_heading;
    size_t parent_heading_index;

    size_t source_offset_start;
    size_t source_offset_end;

    char* text;
    size_t text_length;
    size_t text_capacity;

} rb_chunker_chunk_t;


typedef struct
{
    rb_chunker_chunk_t* chunks;

    size_t chunk_count;
    size_t chunk_capacity;

} rb_chunker_chunk_document_t;


typedef struct
{
    const char* data;

    size_t length;
    size_t position;

} rb_json_parser_t;


/*
 * ------------------------------------------------------------
 * Utility
 * ------------------------------------------------------------
 */

static char* rb_chunker_strdup(
    const char* source
)
{
    size_t length;
    char* copy;

    if (source == NULL)
    {
        return NULL;
    }

    length = strlen(source);

    copy = (char*)malloc(length + 1);

    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(
        copy,
        source,
        length + 1
    );

    return copy;
}


static size_t rb_chunker_utf8_character_count(
    const char* text
)
{
    const unsigned char* cursor;
    size_t count;

    if (text == NULL)
    {
        return 0;
    }

    cursor = (const unsigned char*)text;
    count = 0;

    while (*cursor != '\0')
    {
        if ((*cursor & 0xC0) != 0x80)
        {
            count++;
        }

        cursor++;
    }

    return count;
}


static const char* rb_chunker_block_type_string(
    rb_chunker_block_type_t type
)
{
    switch (type)
    {
    case RB_CHUNKER_BLOCK_HEADING:
        return "HEADING";

    case RB_CHUNKER_BLOCK_PARAGRAPH:
        return "PARAGRAPH";

    case RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM:
        return "UNORDERED_LIST_ITEM";

    case RB_CHUNKER_BLOCK_ORDERED_LIST_ITEM:
        return "ORDERED_LIST_ITEM";

    case RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK:
        return "FENCED_CODE_BLOCK";

    case RB_CHUNKER_BLOCK_BLOCKQUOTE:
        return "BLOCKQUOTE";

    case RB_CHUNKER_BLOCK_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}


static rb_chunker_block_type_t rb_chunker_block_type_from_string(
    const char* value
)
{
    if (value == NULL)
    {
        return RB_CHUNKER_BLOCK_UNKNOWN;
    }

    if (strcmp(value, "HEADING") == 0)
    {
        return RB_CHUNKER_BLOCK_HEADING;
    }

    if (strcmp(value, "PARAGRAPH") == 0)
    {
        return RB_CHUNKER_BLOCK_PARAGRAPH;
    }

    if (strcmp(value, "UNORDERED_LIST_ITEM") == 0)
    {
        return RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM;
    }

    if (strcmp(value, "ORDERED_LIST_ITEM") == 0)
    {
        return RB_CHUNKER_BLOCK_ORDERED_LIST_ITEM;
    }

    if (strcmp(value, "FENCED_CODE_BLOCK") == 0)
    {
        return RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK;
    }

    if (strcmp(value, "BLOCKQUOTE") == 0)
    {
        return RB_CHUNKER_BLOCK_BLOCKQUOTE;
    }

    return RB_CHUNKER_BLOCK_UNKNOWN;
}


/*
 * ------------------------------------------------------------
 * Source document memory
 * ------------------------------------------------------------
 */

static void rb_chunker_source_document_init(
    rb_chunker_source_document_t* document
)
{
    if (document == NULL)
    {
        return;
    }

    memset(
        document,
        0,
        sizeof(*document)
    );
}


static void rb_chunker_source_document_free(
    rb_chunker_source_document_t* document
)
{
    size_t index;

    if (document == NULL)
    {
        return;
    }

    for (index = 0;
        index < document->block_count;
        index++)
    {
        free(document->blocks[index].info);
        free(document->blocks[index].text);
    }

    free(document->blocks);

    memset(
        document,
        0,
        sizeof(*document)
    );
}


static rb_chunker_result_t rb_chunker_source_document_reserve(
    rb_chunker_source_document_t* document,
    size_t required
)
{
    rb_chunker_source_block_t* resized;
    size_t capacity;

    if (document == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (required <= document->block_capacity)
    {
        return RB_CHUNKER_OK;
    }

    capacity = document->block_capacity;

    if (capacity == 0)
    {
        capacity = RB_CHUNKER_INITIAL_CAPACITY;
    }

    while (capacity < required)
    {
        if (capacity > ((size_t)-1) / 2)
        {
            return RB_CHUNKER_ERR_MEMORY;
        }

        capacity *= 2;
    }

    resized =
        (rb_chunker_source_block_t*)realloc(
            document->blocks,
            capacity * sizeof(rb_chunker_source_block_t)
        );

    if (resized == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    memset(
        resized + document->block_capacity,
        0,
        (capacity - document->block_capacity) *
        sizeof(rb_chunker_source_block_t)
    );

    document->blocks = resized;
    document->block_capacity = capacity;

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_source_document_add_block(
    rb_chunker_source_document_t* document,
    const rb_chunker_source_block_t* block
)
{
    rb_chunker_result_t result;

    if (document == NULL ||
        block == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    result =
        rb_chunker_source_document_reserve(
            document,
            document->block_count + 1
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    document->blocks[
        document->block_count
    ] = *block;

    document->block_count++;

    return RB_CHUNKER_OK;
}


/*
 * ------------------------------------------------------------
 * Chunk memory
 * ------------------------------------------------------------
 */

static void rb_chunker_chunk_init(
    rb_chunker_chunk_t* chunk
)
{
    if (chunk == NULL)
    {
        return;
    }

    memset(
        chunk,
        0,
        sizeof(*chunk)
    );
}


static void rb_chunker_chunk_free(
    rb_chunker_chunk_t* chunk
)
{
    if (chunk == NULL)
    {
        return;
    }

    free(chunk->source_blocks);
    free(chunk->block_types);
    free(chunk->text);

    memset(
        chunk,
        0,
        sizeof(*chunk)
    );
}


static void rb_chunker_chunk_document_init(
    rb_chunker_chunk_document_t* document
)
{
    if (document == NULL)
    {
        return;
    }

    memset(
        document,
        0,
        sizeof(*document)
    );
}


static void rb_chunker_chunk_document_free(
    rb_chunker_chunk_document_t* document
)
{
    size_t index;

    if (document == NULL)
    {
        return;
    }

    for (index = 0;
        index < document->chunk_count;
        index++)
    {
        rb_chunker_chunk_free(
            &document->chunks[index]
        );
    }

    free(document->chunks);

    memset(
        document,
        0,
        sizeof(*document)
    );
}


static rb_chunker_result_t rb_chunker_chunk_document_reserve(
    rb_chunker_chunk_document_t* document,
    size_t required
)
{
    rb_chunker_chunk_t* resized;
    size_t capacity;

    if (document == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (required <= document->chunk_capacity)
    {
        return RB_CHUNKER_OK;
    }

    capacity = document->chunk_capacity;

    if (capacity == 0)
    {
        capacity = RB_CHUNKER_INITIAL_CAPACITY;
    }

    while (capacity < required)
    {
        if (capacity > ((size_t)-1) / 2)
        {
            return RB_CHUNKER_ERR_MEMORY;
        }

        capacity *= 2;
    }

    resized =
        (rb_chunker_chunk_t*)realloc(
            document->chunks,
            capacity * sizeof(rb_chunker_chunk_t)
        );

    if (resized == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    memset(
        resized + document->chunk_capacity,
        0,
        (capacity - document->chunk_capacity) *
        sizeof(rb_chunker_chunk_t)
    );

    document->chunks = resized;
    document->chunk_capacity = capacity;

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_chunk_reserve_blocks(
    rb_chunker_chunk_t* chunk,
    size_t required
)
{
    size_t* resized_blocks;
    rb_chunker_block_type_t* resized_types;
    size_t capacity;

    if (chunk == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (required <= chunk->source_block_capacity)
    {
        return RB_CHUNKER_OK;
    }

    capacity = chunk->source_block_capacity;

    if (capacity == 0)
    {
        capacity = 8;
    }

    while (capacity < required)
    {
        if (capacity > ((size_t)-1) / 2)
        {
            return RB_CHUNKER_ERR_MEMORY;
        }

        capacity *= 2;
    }

    resized_blocks =
        (size_t*)realloc(
            chunk->source_blocks,
            capacity * sizeof(size_t)
        );

    if (resized_blocks == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    chunk->source_blocks = resized_blocks;

    resized_types =
        (rb_chunker_block_type_t*)realloc(
            chunk->block_types,
            capacity * sizeof(rb_chunker_block_type_t)
        );

    if (resized_types == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    chunk->block_types = resized_types;

    chunk->source_block_capacity = capacity;
    chunk->block_type_capacity = capacity;

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_chunk_reserve_text(
    rb_chunker_chunk_t* chunk,
    size_t required
)
{
    char* resized;
    size_t capacity;

    if (chunk == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (required <= chunk->text_capacity)
    {
        return RB_CHUNKER_OK;
    }

    capacity = chunk->text_capacity;

    if (capacity == 0)
    {
        capacity = 256;
    }

    while (capacity < required)
    {
        if (capacity > ((size_t)-1) / 2)
        {
            return RB_CHUNKER_ERR_MEMORY;
        }

        capacity *= 2;
    }

    resized =
        (char*)realloc(
            chunk->text,
            capacity
        );

    if (resized == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    chunk->text = resized;
    chunk->text_capacity = capacity;

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_chunk_append_text(
    rb_chunker_chunk_t* chunk,
    const char* text,
    const char* separator
)
{
    size_t separator_length;
    size_t text_length;
    size_t required;

    rb_chunker_result_t result;

    if (chunk == NULL ||
        text == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    separator_length =
        separator != NULL
        ? strlen(separator)
        : 0;

    text_length = strlen(text);

    required =
        chunk->text_length +
        separator_length +
        text_length +
        1;

    result =
        rb_chunker_chunk_reserve_text(
            chunk,
            required
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    if (separator_length != 0)
    {
        memcpy(
            chunk->text +
            chunk->text_length,
            separator,
            separator_length
        );

        chunk->text_length +=
            separator_length;
    }

    memcpy(
        chunk->text +
        chunk->text_length,
        text,
        text_length
    );

    chunk->text_length +=
        text_length;

    chunk->text[
        chunk->text_length
    ] = '\0';

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_chunk_add_source_block(
    rb_chunker_chunk_t* chunk,
    const rb_chunker_source_block_t* block
)
{
    rb_chunker_result_t result;

    if (chunk == NULL ||
        block == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    result =
        rb_chunker_chunk_reserve_blocks(
            chunk,
            chunk->source_block_count + 1
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    chunk->source_blocks[
        chunk->source_block_count
    ] = block->index;

    chunk->block_types[
        chunk->block_type_count
    ] = block->type;

    chunk->source_block_count++;
    chunk->block_type_count++;

    return RB_CHUNKER_OK;
}


/*
 * ------------------------------------------------------------
 * JSON parser
 * ------------------------------------------------------------
 */

static void rb_json_skip_whitespace(
    rb_json_parser_t* parser
)
{
    if (parser == NULL)
    {
        return;
    }

    while (parser->position <
        parser->length &&
        isspace(
            (unsigned char)
            parser->data[
                parser->position
            ]
        ))
    {
        parser->position++;
    }
}


static int rb_json_expect(
    rb_json_parser_t* parser,
    char expected
)
{
    if (parser == NULL)
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position >= parser->length ||
        parser->data[parser->position] != expected)
    {
        return 0;
    }

    parser->position++;

    return 1;
}


static int rb_json_match_literal(
    rb_json_parser_t* parser,
    const char* literal
)
{
    size_t length;

    if (parser == NULL ||
        literal == NULL)
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    length = strlen(literal);

    if (parser->position + length >
        parser->length)
    {
        return 0;
    }

    if (strncmp(
        parser->data +
        parser->position,
        literal,
        length
    ) != 0)
    {
        return 0;
    }

    parser->position += length;

    return 1;
}


static int rb_json_hex_value(
    char value
)
{
    if (value >= '0' &&
        value <= '9')
    {
        return value - '0';
    }

    if (value >= 'a' &&
        value <= 'f')
    {
        return value - 'a' + 10;
    }

    if (value >= 'A' &&
        value <= 'F')
    {
        return value - 'A' + 10;
    }

    return -1;
}


static int rb_json_reserve_output(
    char** buffer,
    size_t* capacity,
    size_t required
)
{
    size_t new_capacity;
    char* resized;

    if (buffer == NULL ||
        capacity == NULL)
    {
        return 0;
    }

    if (required <= *capacity)
    {
        return 1;
    }

    new_capacity =
        *capacity == 0
        ? 64
        : *capacity;

    while (new_capacity < required)
    {
        if (new_capacity >
            ((size_t)-1) / 2)
        {
            return 0;
        }

        new_capacity *= 2;
    }

    resized =
        (char*)realloc(
            *buffer,
            new_capacity
        );

    if (resized == NULL)
    {
        return 0;
    }

    *buffer = resized;
    *capacity = new_capacity;

    return 1;
}


static int rb_json_append_raw_byte(
    char** buffer,
    size_t* length,
    size_t* capacity,
    unsigned char value
)
{
    if (buffer == NULL ||
        length == NULL ||
        capacity == NULL)
    {
        return 0;
    }

    if (!rb_json_reserve_output(
        buffer,
        capacity,
        *length + 2
    ))
    {
        return 0;
    }

    (*buffer)[
        (*length)++
    ] = (char)value;

    (*buffer)[
        *length
    ] = '\0';

    return 1;
}


static int rb_json_append_utf8_codepoint(
    char** buffer,
    size_t* length,
    size_t* capacity,
    unsigned int codepoint
)
{
    unsigned char bytes[4];
    size_t count;
    size_t index;

    if (buffer == NULL ||
        length == NULL ||
        capacity == NULL)
    {
        return 0;
    }

    if (codepoint <= 0x7F)
    {
        bytes[0] =
            (unsigned char)codepoint;

        count = 1;
    }
    else if (codepoint <= 0x7FF)
    {
        bytes[0] =
            (unsigned char)
            (0xC0 |
             (codepoint >> 6));

        bytes[1] =
            (unsigned char)
            (0x80 |
             (codepoint & 0x3F));

        count = 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        if (codepoint >= 0xD800 &&
            codepoint <= 0xDFFF)
        {
            return 0;
        }

        bytes[0] =
            (unsigned char)
            (0xE0 |
             (codepoint >> 12));

        bytes[1] =
            (unsigned char)
            (0x80 |
             ((codepoint >> 6) &
              0x3F));

        bytes[2] =
            (unsigned char)
            (0x80 |
             (codepoint &
              0x3F));

        count = 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        bytes[0] =
            (unsigned char)
            (0xF0 |
             (codepoint >> 18));

        bytes[1] =
            (unsigned char)
            (0x80 |
             ((codepoint >> 12) &
              0x3F));

        bytes[2] =
            (unsigned char)
            (0x80 |
             ((codepoint >> 6) &
              0x3F));

        bytes[3] =
            (unsigned char)
            (0x80 |
             (codepoint &
              0x3F));

        count = 4;
    }
    else
    {
        return 0;
    }

    if (!rb_json_reserve_output(
        buffer,
        capacity,
        *length + count + 1
    ))
    {
        return 0;
    }

    for (index = 0;
        index < count;
        index++)
    {
        (*buffer)[
            (*length)++
        ] = (char)bytes[index];
    }

    (*buffer)[
        *length
    ] = '\0';

    return 1;
}


static int rb_json_parse_u16(
    rb_json_parser_t* parser,
    unsigned int* value
)
{
    unsigned int result = 0;
    int digit;
    int count;

    if (parser == NULL ||
        value == NULL)
    {
        return 0;
    }

    for (count = 0;
        count < 4;
        count++)
    {
        if (parser->position >=
            parser->length)
        {
            return 0;
        }

        digit =
            rb_json_hex_value(
                parser->data[
                    parser->position++
                ]
            );

        if (digit < 0)
        {
            return 0;
        }

        result =
            (result << 4) |
            (unsigned int)digit;
    }

    *value = result;

    return 1;
}


static char* rb_json_parse_string(
    rb_json_parser_t* parser
)
{
    char* output = NULL;

    size_t output_length = 0;
    size_t output_capacity = 0;

    if (parser == NULL)
    {
        return NULL;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position >=
        parser->length ||
        parser->data[
            parser->position
        ] != '"')
    {
        return NULL;
    }

    parser->position++;

    while (parser->position <
        parser->length)
    {
        unsigned char value;

        value =
            (unsigned char)
            parser->data[
                parser->position++
            ];

        if (value == '"')
        {
            if (output == NULL)
            {
                output =
                    (char*)malloc(1);

                if (output == NULL)
                {
                    return NULL;
                }

                output[0] = '\0';
            }

            return output;
        }

        if (value == '\\')
        {
            unsigned char escaped;

            if (parser->position >=
                parser->length)
            {
                free(output);

                return NULL;
            }

            escaped =
                (unsigned char)
                parser->data[
                    parser->position++
                ];

            switch (escaped)
            {
            case '"':
                value = '"';
                break;

            case '\\':
                value = '\\';
                break;

            case '/':
                value = '/';
                break;

            case 'b':
                value = '\b';
                break;

            case 'f':
                value = '\f';
                break;

            case 'n':
                value = '\n';
                break;

            case 'r':
                value = '\r';
                break;

            case 't':
                value = '\t';
                break;

            case 'u':
            {
                unsigned int codepoint;
                unsigned int low_surrogate;

                if (!rb_json_parse_u16(
                    parser,
                    &codepoint
                ))
                {
                    free(output);

                    return NULL;
                }

                if (codepoint >= 0xD800 &&
                    codepoint <= 0xDBFF)
                {
                    if (parser->position + 6 >
                        parser->length ||
                        parser->data[
                            parser->position
                        ] != '\\' ||
                        parser->data[
                            parser->position + 1
                        ] != 'u')
                    {
                        free(output);

                        return NULL;
                    }

                    parser->position += 2;

                    if (!rb_json_parse_u16(
                        parser,
                        &low_surrogate
                    ))
                    {
                        free(output);

                        return NULL;
                    }

                    if (low_surrogate < 0xDC00 ||
                        low_surrogate > 0xDFFF)
                    {
                        free(output);

                        return NULL;
                    }

                    codepoint =
                        0x10000 +
                        (((codepoint - 0xD800) << 10) |
                         (low_surrogate - 0xDC00));
                }
                else if (codepoint >= 0xDC00 &&
                    codepoint <= 0xDFFF)
                {
                    free(output);

                    return NULL;
                }

                if (!rb_json_append_utf8_codepoint(
                    &output,
                    &output_length,
                    &output_capacity,
                    codepoint
                ))
                {
                    free(output);

                    return NULL;
                }

                continue;
            }

            default:
                free(output);

                return NULL;
            }

            if (!rb_json_append_raw_byte(
                &output,
                &output_length,
                &output_capacity,
                value
            ))
            {
                free(output);

                return NULL;
            }

            continue;
        }

        /*
         * Raw JSON string bytes are already UTF-8.
         *
         * Preserve them exactly.
         *
         * Do not treat a raw UTF-8 byte as a Unicode
         * code point and encode it again.
         */
        if (value < 0x20)
        {
            free(output);

            return NULL;
        }

        if (!rb_json_append_raw_byte(
            &output,
            &output_length,
            &output_capacity,
            value
        ))
        {
            free(output);

            return NULL;
        }
    }

    free(output);

    return NULL;
}


static int rb_json_parse_size(
    rb_json_parser_t* parser,
    size_t* value
)
{
    size_t result = 0;
    int found = 0;

    if (parser == NULL ||
        value == NULL)
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    while (parser->position <
        parser->length &&
        parser->data[
            parser->position
        ] >= '0' &&
        parser->data[
            parser->position
        ] <= '9')
    {
        unsigned int digit;

        digit =
            (unsigned int)
            (parser->data[
                parser->position
            ] - '0');

        if (result >
            (((size_t)-1) -
             digit) /
            10)
        {
            return 0;
        }

        result =
            result * 10 +
            digit;

        parser->position++;

        found = 1;
    }

    if (!found)
    {
        return 0;
    }

    *value = result;

    return 1;
}


static int rb_json_skip_value(
    rb_json_parser_t* parser
);


static int rb_json_skip_array(
    rb_json_parser_t* parser
)
{
    if (!rb_json_expect(
        parser,
        '['
    ))
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position <
        parser->length &&
        parser->data[
            parser->position
        ] == ']')
    {
        parser->position++;

        return 1;
    }

    for (;;)
    {
        if (!rb_json_skip_value(parser))
        {
            return 0;
        }

        rb_json_skip_whitespace(parser);

        if (parser->position >=
            parser->length)
        {
            return 0;
        }

        if (parser->data[
            parser->position
        ] == ']')
        {
            parser->position++;

            return 1;
        }

        if (parser->data[
            parser->position
        ] != ',')
        {
            return 0;
        }

        parser->position++;
    }
}


static int rb_json_skip_object(
    rb_json_parser_t* parser
)
{
    if (!rb_json_expect(
        parser,
        '{'
    ))
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position <
        parser->length &&
        parser->data[
            parser->position
        ] == '}')
    {
        parser->position++;

        return 1;
    }

    for (;;)
    {
        char* key;

        key =
            rb_json_parse_string(
                parser
            );

        if (key == NULL)
        {
            return 0;
        }

        free(key);

        if (!rb_json_expect(
            parser,
            ':'
        ))
        {
            return 0;
        }

        if (!rb_json_skip_value(parser))
        {
            return 0;
        }

        rb_json_skip_whitespace(parser);

        if (parser->position >=
            parser->length)
        {
            return 0;
        }

        if (parser->data[
            parser->position
        ] == '}')
        {
            parser->position++;

            return 1;
        }

        if (parser->data[
            parser->position
        ] != ',')
        {
            return 0;
        }

        parser->position++;
    }
}


static int rb_json_skip_value(
    rb_json_parser_t* parser
)
{
    char value;

    if (parser == NULL)
    {
        return 0;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position >=
        parser->length)
    {
        return 0;
    }

    value =
        parser->data[
            parser->position
        ];

    if (value == '"')
    {
        char* string_value;

        string_value =
            rb_json_parse_string(
                parser
            );

        if (string_value == NULL)
        {
            return 0;
        }

        free(string_value);

        return 1;
    }

    if (value == '{')
    {
        return rb_json_skip_object(parser);
    }

    if (value == '[')
    {
        return rb_json_skip_array(parser);
    }

    if (value >= '0' &&
        value <= '9')
    {
        size_t numeric;

        return rb_json_parse_size(
            parser,
            &numeric
        );
    }

    if (rb_json_match_literal(
        parser,
        "null"
    ))
    {
        return 1;
    }

    if (rb_json_match_literal(
        parser,
        "true"
    ))
    {
        return 1;
    }

    if (rb_json_match_literal(
        parser,
        "false"
    ))
    {
        return 1;
    }

    return 0;
}


/*
 * ------------------------------------------------------------
 * RB-MARKDOWN contract parser
 * ------------------------------------------------------------
 */

static rb_chunker_result_t rb_chunker_parse_source_object(
    rb_json_parser_t* parser,
    rb_chunker_source_document_t* document
)
{
    int filename_seen = 0;
    int size_seen = 0;

    if (!rb_json_expect(
        parser,
        '{'
    ))
    {
        return RB_CHUNKER_ERR_INVALID_JSON;
    }

    for (;;)
    {
        char* key;

        rb_json_skip_whitespace(parser);

        if (parser->position <
            parser->length &&
            parser->data[
                parser->position
            ] == '}')
        {
            parser->position++;

            break;
        }

        key =
            rb_json_parse_string(
                parser
            );

        if (key == NULL)
        {
            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (!rb_json_expect(
            parser,
            ':'
        ))
        {
            free(key);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (strcmp(
            key,
            "filename"
        ) == 0)
        {
            char* value;

            if (filename_seen)
            {
                free(key);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            value =
                rb_json_parse_string(
                    parser
                );

            if (value == NULL ||
                value[0] == '\0' ||
                strlen(value) >=
                sizeof(
                    document->
                    source_filename
                ))
            {
                free(value);
                free(key);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            strcpy_s(
                document->source_filename,
                sizeof(
                    document->
                    source_filename
                ),
                value
            );

            free(value);

            filename_seen = 1;
        }
        else if (strcmp(
            key,
            "size"
        ) == 0)
        {
            if (size_seen ||
                !rb_json_parse_size(
                    parser,
                    &document->
                    source_size
                ))
            {
                free(key);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            size_seen = 1;
        }
        else
        {
            if (!rb_json_skip_value(parser))
            {
                free(key);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }
        }

        free(key);

        rb_json_skip_whitespace(parser);

        if (parser->position >=
            parser->length)
        {
            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (parser->data[
            parser->position
        ] == '}')
        {
            parser->position++;

            break;
        }

        if (parser->data[
            parser->position
        ] != ',')
        {
            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        parser->position++;
    }

    if (!filename_seen ||
        !size_seen)
    {
        return RB_CHUNKER_ERR_INVALID_CONTRACT;
    }

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_parse_block_object(
    rb_json_parser_t* parser,
    rb_chunker_source_block_t* block
)
{
    char* type_string = NULL;

    int index_seen = 0;
    int type_seen = 0;
    int heading_seen = 0;
    int parent_seen = 0;
    int source_offset_seen = 0;
    int source_length_seen = 0;
    int content_offset_seen = 0;
    int content_length_seen = 0;
    int text_seen = 0;

    if (parser == NULL ||
        block == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    memset(
        block,
        0,
        sizeof(*block)
    );

    if (!rb_json_expect(
        parser,
        '{'
    ))
    {
        return RB_CHUNKER_ERR_INVALID_JSON;
    }

    for (;;)
    {
        char* key;

        rb_json_skip_whitespace(parser);

        if (parser->position <
            parser->length &&
            parser->data[
                parser->position
            ] == '}')
        {
            parser->position++;

            break;
        }

        key =
            rb_json_parse_string(
                parser
            );

        if (key == NULL)
        {
            free(type_string);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (!rb_json_expect(
            parser,
            ':'
        ))
        {
            free(key);
            free(type_string);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (strcmp(
            key,
            "index"
        ) == 0)
        {
            if (index_seen ||
                !rb_json_parse_size(
                    parser,
                    &block->index
                ))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            index_seen = 1;
        }
        else if (strcmp(
            key,
            "type"
        ) == 0)
        {
            if (type_seen)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            type_string =
                rb_json_parse_string(
                    parser
                );

            if (type_string == NULL)
            {
                free(key);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }

            block->type =
                rb_chunker_block_type_from_string(
                    type_string
                );

            if (block->type ==
                RB_CHUNKER_BLOCK_UNKNOWN)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            type_seen = 1;
        }
        else if (strcmp(
            key,
            "heading_level"
        ) == 0)
        {
            size_t value;

            if (heading_seen ||
                !rb_json_parse_size(
                    parser,
                    &value
                ) ||
                value > 6)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            block->heading_level =
                (unsigned int)value;

            heading_seen = 1;
        }
        else if (strcmp(
            key,
            "parent_heading_index"
        ) == 0)
        {
            if (parent_seen)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            rb_json_skip_whitespace(parser);

            if (rb_json_match_literal(
                parser,
                "null"
            ))
            {
                block->has_parent_heading = 0;
            }
            else
            {
                if (!rb_json_parse_size(
                    parser,
                    &block->
                    parent_heading_index
                ))
                {
                    free(key);
                    free(type_string);

                    return RB_CHUNKER_ERR_INVALID_CONTRACT;
                }

                block->has_parent_heading = 1;
            }

            parent_seen = 1;
        }
        else if (strcmp(
            key,
            "source_offset"
        ) == 0)
        {
            if (source_offset_seen ||
                !rb_json_parse_size(
                    parser,
                    &block->
                    source_offset
                ))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            source_offset_seen = 1;
        }
        else if (strcmp(
            key,
            "source_length"
        ) == 0)
        {
            if (source_length_seen ||
                !rb_json_parse_size(
                    parser,
                    &block->
                    source_length
                ))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            source_length_seen = 1;
        }
        else if (strcmp(
            key,
            "content_offset"
        ) == 0)
        {
            if (content_offset_seen ||
                !rb_json_parse_size(
                    parser,
                    &block->
                    content_offset
                ))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            content_offset_seen = 1;
        }
        else if (strcmp(
            key,
            "content_length"
        ) == 0)
        {
            if (content_length_seen ||
                !rb_json_parse_size(
                    parser,
                    &block->
                    content_length
                ))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            content_length_seen = 1;
        }
        else if (strcmp(
            key,
            "info"
        ) == 0)
        {
            if (block->info != NULL)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            block->info =
                rb_json_parse_string(
                    parser
                );

            if (block->info == NULL)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }
        }
        else if (strcmp(
            key,
            "text"
        ) == 0)
        {
            if (text_seen)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            block->text =
                rb_json_parse_string(
                    parser
                );

            if (block->text == NULL)
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }

            text_seen = 1;
        }
        else
        {
            if (!rb_json_skip_value(parser))
            {
                free(key);
                free(type_string);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }
        }

        free(key);

        rb_json_skip_whitespace(parser);

        if (parser->position >=
            parser->length)
        {
            free(type_string);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (parser->data[
            parser->position
        ] == '}')
        {
            parser->position++;

            break;
        }

        if (parser->data[
            parser->position
        ] != ',')
        {
            free(type_string);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        parser->position++;
    }

    free(type_string);

    if (!index_seen ||
        !type_seen ||
        !heading_seen ||
        !parent_seen ||
        !source_offset_seen ||
        !source_length_seen ||
        !content_offset_seen ||
        !content_length_seen ||
        !text_seen)
    {
        return RB_CHUNKER_ERR_INVALID_CONTRACT;
    }

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_parse_blocks_array(
    rb_json_parser_t* parser,
    rb_chunker_source_document_t* document
)
{
    rb_chunker_result_t result;

    if (!rb_json_expect(
        parser,
        '['
    ))
    {
        return RB_CHUNKER_ERR_INVALID_JSON;
    }

    rb_json_skip_whitespace(parser);

    if (parser->position <
        parser->length &&
        parser->data[
            parser->position
        ] == ']')
    {
        parser->position++;

        return RB_CHUNKER_ERR_INVALID_CONTRACT;
    }

    for (;;)
    {
        rb_chunker_source_block_t block;

        memset(
            &block,
            0,
            sizeof(block)
        );

        result =
            rb_chunker_parse_block_object(
                parser,
                &block
            );

        if (result != RB_CHUNKER_OK)
        {
            free(block.info);
            free(block.text);

            return result;
        }

        result =
            rb_chunker_source_document_add_block(
                document,
                &block
            );

        if (result != RB_CHUNKER_OK)
        {
            free(block.info);
            free(block.text);

            return result;
        }

        rb_json_skip_whitespace(parser);

        if (parser->position >=
            parser->length)
        {
            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (parser->data[
            parser->position
        ] == ']')
        {
            parser->position++;

            break;
        }

        if (parser->data[
            parser->position
        ] != ',')
        {
            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        parser->position++;
    }

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_validate_source_document(
    const rb_chunker_source_document_t* document
)
{
    size_t index;

    if (document == NULL ||
        document->source_filename[0] == '\0' ||
        document->block_count == 0)
    {
        return RB_CHUNKER_ERR_INVALID_STRUCTURE;
    }

    for (index = 0;
        index < document->block_count;
        index++)
    {
        const rb_chunker_source_block_t* block;

        block =
            &document->blocks[index];

        if (block->index !=
            index + 1)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->source_offset >
            document->source_size)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->source_length >
            document->source_size -
            block->source_offset)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->content_offset >
            document->source_size)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->content_length >
            document->source_size -
            block->content_offset)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->type ==
            RB_CHUNKER_BLOCK_HEADING)
        {
            if (block->heading_level == 0 ||
                block->heading_level > 6)
            {
                return RB_CHUNKER_ERR_INVALID_STRUCTURE;
            }
        }
        else if (block->heading_level != 0)
        {
            return RB_CHUNKER_ERR_INVALID_STRUCTURE;
        }

        if (block->has_parent_heading)
        {
            size_t parent;

            parent =
                block->parent_heading_index;

            if (parent == 0 ||
                parent >= block->index)
            {
                return RB_CHUNKER_ERR_INVALID_STRUCTURE;
            }

            if (document->blocks[
                parent - 1
            ].type !=
                RB_CHUNKER_BLOCK_HEADING)
            {
                return RB_CHUNKER_ERR_INVALID_STRUCTURE;
            }
        }
    }

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_parse_document(
    const char* data,
    size_t length,
    rb_chunker_source_document_t* document
)
{
    rb_json_parser_t parser;

    char* format = NULL;

    size_t format_version = 0;

    int format_seen = 0;
    int format_version_seen = 0;
    int source_seen = 0;
    int blocks_seen = 0;

    rb_chunker_result_t result;

    if (data == NULL ||
        document == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    parser.data = data;
    parser.length = length;
    parser.position = 0;

    if (!rb_json_expect(
        &parser,
        '{'
    ))
    {
        return RB_CHUNKER_ERR_INVALID_JSON;
    }

    for (;;)
    {
        char* key;

        rb_json_skip_whitespace(&parser);

        if (parser.position <
            parser.length &&
            parser.data[
                parser.position
            ] == '}')
        {
            parser.position++;

            break;
        }

        key =
            rb_json_parse_string(
                &parser
            );

        if (key == NULL)
        {
            free(format);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (!rb_json_expect(
            &parser,
            ':'
        ))
        {
            free(key);
            free(format);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (strcmp(
            key,
            "format"
        ) == 0)
        {
            if (format_seen)
            {
                free(key);
                free(format);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            format =
                rb_json_parse_string(
                    &parser
                );

            if (format == NULL)
            {
                free(key);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }

            format_seen = 1;
        }
        else if (strcmp(
            key,
            "format_version"
        ) == 0)
        {
            if (format_version_seen ||
                !rb_json_parse_size(
                    &parser,
                    &format_version
                ))
            {
                free(key);
                free(format);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            format_version_seen = 1;
        }
        else if (strcmp(
            key,
            "source"
        ) == 0)
        {
            if (source_seen)
            {
                free(key);
                free(format);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            result =
                rb_chunker_parse_source_object(
                    &parser,
                    document
                );

            if (result != RB_CHUNKER_OK)
            {
                free(key);
                free(format);

                return result;
            }

            source_seen = 1;
        }
        else if (strcmp(
            key,
            "blocks"
        ) == 0)
        {
            if (blocks_seen)
            {
                free(key);
                free(format);

                return RB_CHUNKER_ERR_INVALID_CONTRACT;
            }

            result =
                rb_chunker_parse_blocks_array(
                    &parser,
                    document
                );

            if (result != RB_CHUNKER_OK)
            {
                free(key);
                free(format);

                return result;
            }

            blocks_seen = 1;
        }
        else
        {
            if (!rb_json_skip_value(&parser))
            {
                free(key);
                free(format);

                return RB_CHUNKER_ERR_INVALID_JSON;
            }
        }

        free(key);

        rb_json_skip_whitespace(&parser);

        if (parser.position >=
            parser.length)
        {
            free(format);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        if (parser.data[
            parser.position
        ] == '}')
        {
            parser.position++;

            break;
        }

        if (parser.data[
            parser.position
        ] != ',')
        {
            free(format);

            return RB_CHUNKER_ERR_INVALID_JSON;
        }

        parser.position++;
    }

    rb_json_skip_whitespace(&parser);

    if (parser.position !=
        parser.length)
    {
        free(format);

        return RB_CHUNKER_ERR_INVALID_JSON;
    }

    if (!format_seen ||
        !format_version_seen ||
        !source_seen ||
        !blocks_seen)
    {
        free(format);

        return RB_CHUNKER_ERR_INVALID_CONTRACT;
    }

    if (strcmp(
        format,
        RB_CHUNKER_EXPECTED_FORMAT
    ) != 0 ||
        format_version !=
        RB_CHUNKER_EXPECTED_FORMAT_VER)
    {
        free(format);

        return RB_CHUNKER_ERR_INVALID_CONTRACT;
    }

    free(format);

    return rb_chunker_validate_source_document(
        document
    );
}


/*
 * ------------------------------------------------------------
 * Chunk construction
 * ------------------------------------------------------------
 */

static int rb_chunker_same_parent(
    const rb_chunker_chunk_t* chunk,
    const rb_chunker_source_block_t* block
)
{
    if (chunk == NULL ||
        block == NULL)
    {
        return 0;
    }

    if (chunk->has_parent_heading !=
        block->has_parent_heading)
    {
        return 0;
    }

    if (!chunk->has_parent_heading)
    {
        return 1;
    }

    return chunk->parent_heading_index ==
        block->parent_heading_index;
}


static const char* rb_chunker_separator_for(
    rb_chunker_block_type_t previous,
    rb_chunker_block_type_t current
)
{
    int previous_list;
    int current_list;

    previous_list =
        previous ==
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM ||
        previous ==
        RB_CHUNKER_BLOCK_ORDERED_LIST_ITEM;

    current_list =
        current ==
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM ||
        current ==
        RB_CHUNKER_BLOCK_ORDERED_LIST_ITEM;

    if (previous_list &&
        current_list)
    {
        return "\n";
    }

    return "\n\n";
}


static rb_chunker_result_t rb_chunker_start_chunk(
    rb_chunker_chunk_t* chunk,
    const rb_chunker_source_block_t* block
)
{
    rb_chunker_result_t result;

    if (chunk == NULL ||
        block == NULL ||
        block->text == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    rb_chunker_chunk_init(chunk);

    chunk->has_parent_heading =
        block->has_parent_heading;

    chunk->parent_heading_index =
        block->parent_heading_index;

    chunk->source_offset_start =
        block->source_offset;

    chunk->source_offset_end =
        block->source_offset +
        block->source_length;

    result =
        rb_chunker_chunk_add_source_block(
            chunk,
            block
        );

    if (result != RB_CHUNKER_OK)
    {
        rb_chunker_chunk_free(chunk);

        return result;
    }

    result =
        rb_chunker_chunk_append_text(
            chunk,
            block->text,
            NULL
        );

    if (result != RB_CHUNKER_OK)
    {
        rb_chunker_chunk_free(chunk);

        return result;
    }

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_append_block(
    rb_chunker_chunk_t* chunk,
    const rb_chunker_source_block_t* block
)
{
    const char* separator;

    rb_chunker_result_t result;

    if (chunk == NULL ||
        block == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (chunk->block_type_count == 0)
    {
        return RB_CHUNKER_ERR_INVALID_STRUCTURE;
    }

    separator =
        rb_chunker_separator_for(
            chunk->block_types[
                chunk->
                block_type_count - 1
            ],
            block->type
        );

    result =
        rb_chunker_chunk_append_text(
            chunk,
            block->text,
            separator
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    result =
        rb_chunker_chunk_add_source_block(
            chunk,
            block
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    chunk->source_offset_end =
        block->source_offset +
        block->source_length;

    return RB_CHUNKER_OK;
}


static rb_chunker_result_t rb_chunker_commit_chunk(
    rb_chunker_chunk_document_t* document,
    rb_chunker_chunk_t* chunk
)
{
    rb_chunker_result_t result;

    if (document == NULL ||
        chunk == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    if (chunk->source_block_count == 0)
    {
        return RB_CHUNKER_OK;
    }

    result =
        rb_chunker_chunk_document_reserve(
            document,
            document->chunk_count + 1
        );

    if (result != RB_CHUNKER_OK)
    {
        return result;
    }

    chunk->index =
        document->chunk_count + 1;

    document->chunks[
        document->chunk_count
    ] = *chunk;

    document->chunk_count++;

    rb_chunker_chunk_init(chunk);

    return RB_CHUNKER_OK;
}


static int rb_chunker_atomic_block(
    rb_chunker_block_type_t type
)
{
    return type ==
        RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK ||
        type ==
        RB_CHUNKER_BLOCK_BLOCKQUOTE;
}


static rb_chunker_result_t rb_chunker_build_chunks(
    const rb_chunker_source_document_t* source,
    rb_chunker_chunk_document_t* output
)
{
    size_t index;

    rb_chunker_chunk_t current;

    rb_chunker_result_t result;

    if (source == NULL ||
        output == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    rb_chunker_chunk_init(&current);

    for (index = 0;
        index < source->block_count;
        index++)
    {
        const rb_chunker_source_block_t* block;

        block =
            &source->blocks[index];

        /*
         * Headings establish context.
         */
        if (block->type ==
            RB_CHUNKER_BLOCK_HEADING)
        {
            result =
                rb_chunker_commit_chunk(
                    output,
                    &current
                );

            if (result != RB_CHUNKER_OK)
            {
                rb_chunker_chunk_free(&current);

                return result;
            }

            continue;
        }

        /*
         * Fenced code and blockquotes remain atomic.
         */
        if (rb_chunker_atomic_block(
            block->type
        ))
        {
            result =
                rb_chunker_commit_chunk(
                    output,
                    &current
                );

            if (result != RB_CHUNKER_OK)
            {
                rb_chunker_chunk_free(&current);

                return result;
            }

            result =
                rb_chunker_start_chunk(
                    &current,
                    block
                );

            if (result != RB_CHUNKER_OK)
            {
                return result;
            }

            result =
                rb_chunker_commit_chunk(
                    output,
                    &current
                );

            if (result != RB_CHUNKER_OK)
            {
                rb_chunker_chunk_free(&current);

                return result;
            }

            continue;
        }

        if (current.source_block_count == 0)
        {
            result =
                rb_chunker_start_chunk(
                    &current,
                    block
                );

            if (result != RB_CHUNKER_OK)
            {
                return result;
            }

            continue;
        }

        /*
         * Parent-heading changes are hard boundaries.
         */
        if (!rb_chunker_same_parent(
            &current,
            block
        ))
        {
            result =
                rb_chunker_commit_chunk(
                    output,
                    &current
                );

            if (result != RB_CHUNKER_OK)
            {
                rb_chunker_chunk_free(&current);

                return result;
            }

            result =
                rb_chunker_start_chunk(
                    &current,
                    block
                );

            if (result != RB_CHUNKER_OK)
            {
                return result;
            }

            continue;
        }

        {
            const char* separator;
            size_t proposed_size;

            separator =
                rb_chunker_separator_for(
                    current.block_types[
                        current.block_type_count - 1
                    ],
                    block->type
                );

            proposed_size =
                current.text_length +
                strlen(separator) +
                strlen(block->text);

            /*
             * Split only between source blocks.
             */
            if (proposed_size >
                RB_CHUNKER_MAX_COMBINED_BYTES)
            {
                result =
                    rb_chunker_commit_chunk(
                        output,
                        &current
                    );

                if (result != RB_CHUNKER_OK)
                {
                    rb_chunker_chunk_free(&current);

                    return result;
                }

                result =
                    rb_chunker_start_chunk(
                        &current,
                        block
                    );

                if (result != RB_CHUNKER_OK)
                {
                    return result;
                }
            }
            else
            {
                result =
                    rb_chunker_append_block(
                        &current,
                        block
                    );

                if (result != RB_CHUNKER_OK)
                {
                    rb_chunker_chunk_free(&current);

                    return result;
                }
            }
        }
    }

    result =
        rb_chunker_commit_chunk(
            output,
            &current
        );

    if (result != RB_CHUNKER_OK)
    {
        rb_chunker_chunk_free(&current);

        return result;
    }

    return RB_CHUNKER_OK;
}


/*
 * ------------------------------------------------------------
 * File input
 * ------------------------------------------------------------
 */

static rb_chunker_result_t rb_chunker_read_file(
    const char* path,
    char** data,
    size_t* length
)
{
    FILE* file = NULL;

    long file_size;

    char* buffer;

    size_t read_count;

    if (path == NULL ||
        data == NULL ||
        length == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    *data = NULL;
    *length = 0;

    if (fopen_s(
        &file,
        path,
        "rb"
    ) != 0 ||
        file == NULL)
    {
        return RB_CHUNKER_ERR_OPEN_FAILED;
    }

    if (fseek(
        file,
        0,
        SEEK_END
    ) != 0)
    {
        fclose(file);

        return RB_CHUNKER_ERR_READ_FAILED;
    }

    file_size = ftell(file);

    if (file_size < 0)
    {
        fclose(file);

        return RB_CHUNKER_ERR_READ_FAILED;
    }

    if (fseek(
        file,
        0,
        SEEK_SET
    ) != 0)
    {
        fclose(file);

        return RB_CHUNKER_ERR_READ_FAILED;
    }

    buffer =
        (char*)malloc(
            (size_t)file_size + 1
        );

    if (buffer == NULL)
    {
        fclose(file);

        return RB_CHUNKER_ERR_MEMORY;
    }

    read_count =
        fread(
            buffer,
            1,
            (size_t)file_size,
            file
        );

    fclose(file);

    if (read_count !=
        (size_t)file_size)
    {
        free(buffer);

        return RB_CHUNKER_ERR_READ_FAILED;
    }

    buffer[
        read_count
    ] = '\0';

    *data = buffer;
    *length = read_count;

    return RB_CHUNKER_OK;
}


/*
 * ------------------------------------------------------------
 * JSON output
 * ------------------------------------------------------------
 */

static int rb_chunker_json_write_byte(
    FILE* file,
    unsigned char value
)
{
    if (file == NULL)
    {
        return 0;
    }

    switch (value)
    {
    case '"':
        return fputs("\\\"", file) >= 0;

    case '\\':
        return fputs("\\\\", file) >= 0;

    case '\b':
        return fputs("\\b", file) >= 0;

    case '\f':
        return fputs("\\f", file) >= 0;

    case '\n':
        return fputs("\\n", file) >= 0;

    case '\r':
        return fputs("\\r", file) >= 0;

    case '\t':
        return fputs("\\t", file) >= 0;

    default:
        break;
    }

    if (value < 0x20)
    {
        return fprintf(
            file,
            "\\u%04x",
            (unsigned int)value
        ) >= 0;
    }

    /*
     * UTF-8 bytes >= 0x20 are preserved exactly.
     */
    return fputc(
        (int)value,
        file
    ) != EOF;
}


static int rb_chunker_json_write_string(
    FILE* file,
    const char* text
)
{
    const unsigned char* cursor;

    if (file == NULL ||
        text == NULL)
    {
        return 0;
    }

    if (fputc('"', file) == EOF)
    {
        return 0;
    }

    cursor =
        (const unsigned char*)text;

    while (*cursor != '\0')
    {
        if (!rb_chunker_json_write_byte(
            file,
            *cursor
        ))
        {
            return 0;
        }

        cursor++;
    }

    return fputc(
        '"',
        file
    ) != EOF;
}


static const rb_chunker_source_block_t*
rb_chunker_find_block(
    const rb_chunker_source_document_t* source,
    size_t one_based_index
)
{
    if (source == NULL ||
        one_based_index == 0 ||
        one_based_index >
        source->block_count)
    {
        return NULL;
    }

    return &source->blocks[
        one_based_index - 1
    ];
}


static int rb_chunker_write_heading_path(
    FILE* file,
    const rb_chunker_source_document_t* source,
    const rb_chunker_chunk_t* chunk
)
{
    size_t stack[16];
    size_t count = 0;

    size_t current;

    if (file == NULL ||
        source == NULL ||
        chunk == NULL)
    {
        return 0;
    }

    if (fputs("[", file) < 0)
    {
        return 0;
    }

    if (!chunk->has_parent_heading)
    {
        return fputs(
            "]",
            file
        ) >= 0;
    }

    current =
        chunk->parent_heading_index;

    while (current != 0 &&
        count <
        sizeof(stack) /
        sizeof(stack[0]))
    {
        const rb_chunker_source_block_t* heading;

        heading =
            rb_chunker_find_block(
                source,
                current
            );

        if (heading == NULL ||
            heading->type !=
            RB_CHUNKER_BLOCK_HEADING)
        {
            return 0;
        }

        stack[count++] = current;

        if (!heading->
            has_parent_heading)
        {
            break;
        }

        current =
            heading->
            parent_heading_index;
    }

    while (count > 0)
    {
        const rb_chunker_source_block_t* heading;

        count--;

        heading =
            rb_chunker_find_block(
                source,
                stack[count]
            );

        if (heading == NULL ||
            heading->text == NULL)
        {
            return 0;
        }

        if (!rb_chunker_json_write_string(
            file,
            heading->text
        ))
        {
            return 0;
        }

        if (count != 0)
        {
            if (fputs(
                ", ",
                file
            ) < 0)
            {
                return 0;
            }
        }
    }

    return fputs(
        "]",
        file
    ) >= 0;
}


static int rb_chunker_write_parent_index(
    FILE* file,
    const rb_chunker_chunk_t* chunk
)
{
    if (file == NULL ||
        chunk == NULL)
    {
        return 0;
    }

    if (!chunk->
        has_parent_heading)
    {
        return fputs(
            "null",
            file
        ) >= 0;
    }

    return fprintf(
        file,
        "%llu",
        (unsigned long long)
        chunk->
        parent_heading_index
    ) >= 0;
}


static int rb_chunker_write_artifact(
    const char* path,
    const rb_chunker_source_document_t* source,
    const rb_chunker_chunk_document_t* chunks
)
{
    FILE* file = NULL;

    size_t index;

    if (path == NULL ||
        source == NULL ||
        chunks == NULL)
    {
        return 0;
    }

    if (fopen_s(
        &file,
        path,
        "wb"
    ) != 0 ||
        file == NULL)
    {
        return 0;
    }

    if (fputs(
        "{\n"
        "  \"contract\": \"RB-RETRIEVAL-CHUNKS\",\n"
        "  \"contract_version\": 1,\n"
        "  \"producer\": {\n"
        "    \"module_id\": \"RB-CHUNKER\",\n"
        "    \"module_version\": \"0.1.1\"\n"
        "  },\n"
        "  \"source\": {\n"
        "    \"filename\": ",
        file
    ) < 0)
    {
        fclose(file);

        return 0;
    }

    if (!rb_chunker_json_write_string(
        file,
        source->source_filename
    ))
    {
        fclose(file);

        return 0;
    }

    if (fprintf(
        file,
        ",\n"
        "    \"size\": %llu,\n"
        "    \"structured_format\": "
        "\"RB-MARKDOWN\",\n"
        "    \"structured_format_version\": 2\n"
        "  },\n"
        "  \"chunking\": {\n"
        "    \"strategy\": \"STRUCTURAL\",\n"
        "    \"max_combined_bytes\": %u,\n"
        "    \"tokenizer\": null\n"
        "  },\n"
        "  \"chunks\": [\n",
        (unsigned long long)
        source->source_size,
        RB_CHUNKER_MAX_COMBINED_BYTES
    ) < 0)
    {
        fclose(file);

        return 0;
    }

    for (index = 0;
        index <
        chunks->chunk_count;
        index++)
    {
        const rb_chunker_chunk_t* chunk;

        size_t block_index;

        chunk =
            &chunks->chunks[index];

        if (fprintf(
            file,
            "    {\n"
            "      \"index\": %llu,\n"
            "      \"source_blocks\": [",
            (unsigned long long)
            chunk->index
        ) < 0)
        {
            fclose(file);

            return 0;
        }

        for (block_index = 0;
            block_index <
            chunk->source_block_count;
            block_index++)
        {
            if (fprintf(
                file,
                "%llu",
                (unsigned long long)
                chunk->
                source_blocks[
                    block_index
                ]
            ) < 0)
            {
                fclose(file);

                return 0;
            }

            if (block_index + 1 <
                chunk->
                source_block_count)
            {
                if (fputs(
                    ", ",
                    file
                ) < 0)
                {
                    fclose(file);

                    return 0;
                }
            }
        }

        if (fputs(
            "],\n"
            "      \"block_types\": [",
            file
        ) < 0)
        {
            fclose(file);

            return 0;
        }

        for (block_index = 0;
            block_index <
            chunk->block_type_count;
            block_index++)
        {
            if (!rb_chunker_json_write_string(
                file,
                rb_chunker_block_type_string(
                    chunk->
                    block_types[
                        block_index
                    ]
                )
            ))
            {
                fclose(file);

                return 0;
            }

            if (block_index + 1 <
                chunk->
                block_type_count)
            {
                if (fputs(
                    ", ",
                    file
                ) < 0)
                {
                    fclose(file);

                    return 0;
                }
            }
        }

        if (fputs(
            "],\n"
            "      \"parent_heading_index\": ",
            file
        ) < 0)
        {
            fclose(file);

            return 0;
        }

        if (!rb_chunker_write_parent_index(
            file,
            chunk
        ))
        {
            fclose(file);

            return 0;
        }

        if (fprintf(
            file,
            ",\n"
            "      \"source_offset_start\": %llu,\n"
            "      \"source_offset_end\": %llu,\n"
            "      \"heading_path\": ",
            (unsigned long long)
            chunk->
            source_offset_start,
            (unsigned long long)
            chunk->
            source_offset_end
        ) < 0)
        {
            fclose(file);

            return 0;
        }

        if (!rb_chunker_write_heading_path(
            file,
            source,
            chunk
        ))
        {
            fclose(file);

            return 0;
        }

        if (fprintf(
            file,
            ",\n"
            "      \"byte_count\": %llu,\n"
            "      \"character_count\": %llu,\n"
            "      \"text\": ",
            (unsigned long long)
            chunk->text_length,
            (unsigned long long)
            rb_chunker_utf8_character_count(
                chunk->text
            )
        ) < 0)
        {
            fclose(file);

            return 0;
        }

        if (!rb_chunker_json_write_string(
            file,
            chunk->text
        ))
        {
            fclose(file);

            return 0;
        }

        if (index + 1 <
            chunks->chunk_count)
        {
            if (fputs(
                "\n    },\n",
                file
            ) < 0)
            {
                fclose(file);

                return 0;
            }
        }
        else
        {
            if (fputs(
                "\n    }\n",
                file
            ) < 0)
            {
                fclose(file);

                return 0;
            }
        }
    }

    if (fputs(
        "  ]\n"
        "}\n",
        file
    ) < 0)
    {
        fclose(file);

        return 0;
    }

    if (fflush(file) != 0)
    {
        fclose(file);

        return 0;
    }

    if (fclose(file) != 0)
    {
        return 0;
    }

    return 1;
}


/*
 * ------------------------------------------------------------
 * Path handling
 * ------------------------------------------------------------
 */

static int rb_chunker_has_suffix(
    const char* value,
    const char* suffix
)
{
    size_t value_length;
    size_t suffix_length;

    if (value == NULL ||
        suffix == NULL)
    {
        return 0;
    }

    value_length = strlen(value);
    suffix_length = strlen(suffix);

    if (suffix_length >
        value_length)
    {
        return 0;
    }

    return _stricmp(
        value +
        value_length -
        suffix_length,
        suffix
    ) == 0;
}


static int rb_chunker_build_output_path(
    const char* output_directory,
    const char* input_filename,
    char* output_path,
    size_t output_path_size
)
{
    size_t filename_length;
    size_t suffix_length;
    size_t base_length;

    int written;

    if (output_directory == NULL ||
        input_filename == NULL ||
        output_path == NULL ||
        output_path_size == 0)
    {
        return 0;
    }

    filename_length =
        strlen(input_filename);

    suffix_length =
        strlen(
            RB_CHUNKER_INPUT_SUFFIX
        );

    if (filename_length <
        suffix_length ||
        !rb_chunker_has_suffix(
            input_filename,
            RB_CHUNKER_INPUT_SUFFIX
        ))
    {
        return 0;
    }

    base_length =
        filename_length -
        suffix_length;

    written =
        snprintf(
            output_path,
            output_path_size,
            "%s\\%.*s%s",
            output_directory,
            (int)base_length,
            input_filename,
            RB_CHUNKER_OUTPUT_SUFFIX
        );

    return written >= 0 &&
        (size_t)written <
        output_path_size;
}


/*
 * ------------------------------------------------------------
 * Qualification helpers
 * ------------------------------------------------------------
 */

static rb_chunker_result_t rb_chunker_test_add_block(
    rb_chunker_source_document_t* document,
    size_t index,
    rb_chunker_block_type_t type,
    int has_parent,
    size_t parent,
    size_t offset,
    const char* text
)
{
    rb_chunker_source_block_t block;

    if (document == NULL ||
        text == NULL)
    {
        return RB_CHUNKER_ERR_INVALID_ARGUMENT;
    }

    memset(
        &block,
        0,
        sizeof(block)
    );

    block.index = index;
    block.type = type;

    block.heading_level =
        type ==
        RB_CHUNKER_BLOCK_HEADING
        ? 2
        : 0;

    block.has_parent_heading =
        has_parent;

    block.parent_heading_index =
        parent;

    block.source_offset =
        offset;

    block.source_length =
        strlen(text);

    block.content_offset =
        offset;

    block.content_length =
        strlen(text);

    block.text =
        rb_chunker_strdup(
            text
        );

    if (block.text == NULL)
    {
        return RB_CHUNKER_ERR_MEMORY;
    }

    return rb_chunker_source_document_add_block(
        document,
        &block
    );
}


/*
 * ------------------------------------------------------------
 * Module qualification
 * ------------------------------------------------------------
 */

static rb_module_result_t rb_chunker_qualify(
    rb_module_qualification_result_t* result
)
{
    unsigned int passed = 0;

    rb_chunker_source_document_t source;
    rb_chunker_chunk_document_t chunks;

    rb_chunker_result_t chunk_result;

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
     * One paragraph produces one chunk.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        0,
        "Paragraph"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        strcmp(
            chunks.chunks[0].text,
            "Paragraph"
        ) == 0)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 02
     * Adjacent paragraphs with the same parent
     * are combined.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        0,
        "One"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        4,
        "Two"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        strcmp(
            chunks.chunks[0].text,
            "One\n\nTwo"
        ) == 0)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 03
     * Parent-heading change creates a hard boundary.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 200;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_HEADING,
        0,
        0,
        0,
        "First"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        1,
        1,
        10,
        "Alpha"
    );

    (void)rb_chunker_test_add_block(
        &source,
        3,
        RB_CHUNKER_BLOCK_HEADING,
        0,
        0,
        20,
        "Second"
    );

    (void)rb_chunker_test_add_block(
        &source,
        4,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        1,
        3,
        30,
        "Beta"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 2 &&
        chunks.chunks[0].
        parent_heading_index == 1 &&
        chunks.chunks[1].
        parent_heading_index == 3)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 04
     * Adjacent list items combine.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM,
        0,
        0,
        0,
        "First"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM,
        0,
        0,
        10,
        "Second"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        strcmp(
            chunks.chunks[0].text,
            "First\nSecond"
        ) == 0)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 05
     * Fenced code remains atomic.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        0,
        "Before"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK,
        0,
        0,
        10,
        "code"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 2 &&
        chunks.chunks[1].
        source_block_count == 1 &&
        chunks.chunks[1].
        block_types[0] ==
        RB_CHUNKER_BLOCK_FENCED_CODE_BLOCK)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 06
     * Blockquote remains atomic.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_BLOCKQUOTE,
        0,
        0,
        0,
        "Quote"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        chunks.chunks[0].
        source_block_count == 1 &&
        chunks.chunks[0].
        block_types[0] ==
        RB_CHUNKER_BLOCK_BLOCKQUOTE)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 07
     * Source block order is preserved.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        0,
        "A"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        10,
        "B"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        chunks.chunks[0].
        source_blocks[0] == 1 &&
        chunks.chunks[0].
        source_blocks[1] == 2)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 08
     * Source offset range remains traceable.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        5,
        "ABCDE"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        20,
        "XYZ"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        chunks.chunks[0].
        source_offset_start == 5 &&
        chunks.chunks[0].
        source_offset_end == 23)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 09
     * Heading blocks establish context but are
     * not emitted as standalone chunks.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_HEADING,
        0,
        0,
        0,
        "Section"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        1,
        1,
        10,
        "Content"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        chunks.chunks[0].
        source_block_count == 1 &&
        chunks.chunks[0].
        source_blocks[0] == 2)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 10
     * Identical input produces identical chunk text.
     */
    {
        rb_chunker_chunk_document_t first;
        rb_chunker_chunk_document_t second;

        rb_chunker_source_document_init(&source);
        rb_chunker_chunk_document_init(&first);
        rb_chunker_chunk_document_init(&second);

        strcpy_s(
            source.source_filename,
            sizeof(source.source_filename),
            "test.md"
        );

        source.source_size = 100;

        (void)rb_chunker_test_add_block(
            &source,
            1,
            RB_CHUNKER_BLOCK_PARAGRAPH,
            0,
            0,
            0,
            "Deterministic"
        );

        if (rb_chunker_build_chunks(
            &source,
            &first
        ) == RB_CHUNKER_OK &&
            rb_chunker_build_chunks(
                &source,
                &second
            ) == RB_CHUNKER_OK &&
            first.chunk_count ==
            second.chunk_count &&
            first.chunk_count == 1 &&
            strcmp(
                first.chunks[0].text,
                second.chunks[0].text
            ) == 0)
        {
            passed++;
        }

        rb_chunker_chunk_document_free(&first);
        rb_chunker_chunk_document_free(&second);
        rb_chunker_source_document_free(&source);
    }


    /*
     * Test 11
     * Different block types remain traceable.
     */
    rb_chunker_source_document_init(&source);
    rb_chunker_chunk_document_init(&chunks);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "test.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        0,
        0,
        0,
        "Intro"
    );

    (void)rb_chunker_test_add_block(
        &source,
        2,
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM,
        0,
        0,
        10,
        "Item"
    );

    chunk_result =
        rb_chunker_build_chunks(
            &source,
            &chunks
        );

    if (chunk_result ==
        RB_CHUNKER_OK &&
        chunks.chunk_count == 1 &&
        chunks.chunks[0].
        block_type_count == 2 &&
        chunks.chunks[0].
        block_types[0] ==
        RB_CHUNKER_BLOCK_PARAGRAPH &&
        chunks.chunks[0].
        block_types[1] ==
        RB_CHUNKER_BLOCK_UNORDERED_LIST_ITEM)
    {
        passed++;
    }

    rb_chunker_chunk_document_free(&chunks);
    rb_chunker_source_document_free(&source);


    /*
     * Test 12
     * Oversized combined content splits only
     * at a source-block boundary.
     */
    {
        char large_a[1001];
        char large_b[1001];

        memset(
            large_a,
            'A',
            sizeof(large_a) - 1
        );

        large_a[
            sizeof(large_a) - 1
        ] = '\0';

        memset(
            large_b,
            'B',
            sizeof(large_b) - 1
        );

        large_b[
            sizeof(large_b) - 1
        ] = '\0';

        rb_chunker_source_document_init(&source);
        rb_chunker_chunk_document_init(&chunks);

        strcpy_s(
            source.source_filename,
            sizeof(source.source_filename),
            "test.md"
        );

        source.source_size = 3000;

        (void)rb_chunker_test_add_block(
            &source,
            1,
            RB_CHUNKER_BLOCK_PARAGRAPH,
            0,
            0,
            0,
            large_a
        );

        (void)rb_chunker_test_add_block(
            &source,
            2,
            RB_CHUNKER_BLOCK_PARAGRAPH,
            0,
            0,
            1200,
            large_b
        );

        chunk_result =
            rb_chunker_build_chunks(
                &source,
                &chunks
            );

        if (chunk_result ==
            RB_CHUNKER_OK &&
            chunks.chunk_count == 2 &&
            chunks.chunks[0].
            source_block_count == 1 &&
            chunks.chunks[1].
            source_block_count == 1)
        {
            passed++;
        }

        rb_chunker_chunk_document_free(&chunks);
        rb_chunker_source_document_free(&source);
    }


    /*
     * Test 13
     * Raw UTF-8 bytes are preserved during JSON
     * string parsing.
     *
     * Input contains:
     *
     *     Core → Module │ ├── └──
     */
    {
        static const char json_text[] =
            "\"Core \xE2\x86\x92 Module "
            "\xE2\x94\x82 "
            "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 "
            "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\"";

        static const char expected[] =
            "Core \xE2\x86\x92 Module "
            "\xE2\x94\x82 "
            "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 "
            "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80";

        rb_json_parser_t parser;
        char* parsed;

        parser.data = json_text;
        parser.length = strlen(json_text);
        parser.position = 0;

        parsed =
            rb_json_parse_string(
                &parser
            );

        if (parsed != NULL &&
            strcmp(
                parsed,
                expected
            ) == 0)
        {
            passed++;
        }

        free(parsed);
    }


    result->tests_executed = 13;
    result->tests_passed = passed;

    result->tests_failed =
        result->tests_executed -
        result->tests_passed;


    /*
     * Mandatory negative validation.
     *
     * A block claiming a parent that does not
     * identify a prior heading must fail structural
     * validation.
     */
    result->negative_test_executed = 1;

    rb_chunker_source_document_init(&source);

    strcpy_s(
        source.source_filename,
        sizeof(source.source_filename),
        "invalid.md"
    );

    source.source_size = 100;

    (void)rb_chunker_test_add_block(
        &source,
        1,
        RB_CHUNKER_BLOCK_PARAGRAPH,
        1,
        99,
        0,
        "Invalid parent"
    );

    if (rb_chunker_validate_source_document(
        &source
    ) ==
        RB_CHUNKER_ERR_INVALID_STRUCTURE)
    {
        result->negative_test_passed = 1;
    }

    rb_chunker_source_document_free(&source);


    if (result->tests_failed != 0 ||
        !result->
        negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}


/*
 * ------------------------------------------------------------
 * Module execution
 * ------------------------------------------------------------
 */

static rb_module_result_t rb_chunker_execute(
    const rb_module_execution_context_t* context
)
{
    WIN32_FIND_DATAA find_data;

    HANDLE search;

    char pattern[
        RB_CHUNKER_PATH_MAX
    ];

    unsigned int documents = 0;

    if (context == NULL ||
        context->output_path == NULL ||
        context->output_path[0] == '\0')
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (snprintf(
        pattern,
        sizeof(pattern),
        "%s\\*%s",
        context->output_path,
        RB_CHUNKER_INPUT_SUFFIX
    ) < 0)
    {
        return RB_MODULE_ERR_EXECUTION;
    }

    search =
        FindFirstFileA(
            pattern,
            &find_data
        );

    if (search ==
        INVALID_HANDLE_VALUE)
    {
        if (GetLastError() ==
            ERROR_FILE_NOT_FOUND)
        {
            printf(
                "[CHUNKER] Structured artifacts: 0\n"
            );

            return RB_MODULE_OK;
        }

        return RB_MODULE_ERR_EXECUTION;
    }

    do
    {
        char input_path[
            RB_CHUNKER_PATH_MAX
        ];

        char output_path[
            RB_CHUNKER_PATH_MAX
        ];

        char* data = NULL;

        size_t data_length = 0;

        rb_chunker_source_document_t source;
        rb_chunker_chunk_document_t chunks;

        rb_chunker_result_t result;

        if (find_data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (!rb_chunker_has_suffix(
            find_data.cFileName,
            RB_CHUNKER_INPUT_SUFFIX
        ))
        {
            continue;
        }

        if (snprintf(
            input_path,
            sizeof(input_path),
            "%s\\%s",
            context->output_path,
            find_data.cFileName
        ) < 0)
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (!rb_chunker_build_output_path(
            context->output_path,
            find_data.cFileName,
            output_path,
            sizeof(output_path)
        ))
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        rb_chunker_source_document_init(&source);
        rb_chunker_chunk_document_init(&chunks);

        printf(
            "[CHUNKER] Source: %s\n",
            input_path
        );

        result =
            rb_chunker_read_file(
                input_path,
                &data,
                &data_length
            );

        if (result !=
            RB_CHUNKER_OK)
        {
            fprintf(
                stderr,
                "[CHUNKER] Read FAIL\n"
            );

            rb_chunker_source_document_free(&source);
            rb_chunker_chunk_document_free(&chunks);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        result =
            rb_chunker_parse_document(
                data,
                data_length,
                &source
            );

        free(data);

        if (result !=
            RB_CHUNKER_OK)
        {
            fprintf(
                stderr,
                "[CHUNKER] Structured input FAIL: %d\n",
                (int)result
            );

            rb_chunker_source_document_free(&source);
            rb_chunker_chunk_document_free(&chunks);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[CHUNKER] Structured input PASS: %u block(s)\n",
            (unsigned int)
            source.block_count
        );

        result =
            rb_chunker_build_chunks(
                &source,
                &chunks
            );

        if (result !=
            RB_CHUNKER_OK)
        {
            fprintf(
                stderr,
                "[CHUNKER] Chunk construction FAIL: %d\n",
                (int)result
            );

            rb_chunker_source_document_free(&source);
            rb_chunker_chunk_document_free(&chunks);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[CHUNKER] Chunk construction PASS: %u chunk(s)\n",
            (unsigned int)
            chunks.chunk_count
        );

        if (!rb_chunker_write_artifact(
            output_path,
            &source,
            &chunks
        ))
        {
            fprintf(
                stderr,
                "[CHUNKER] Artifact write FAIL: %s\n",
                output_path
            );

            rb_chunker_source_document_free(&source);
            rb_chunker_chunk_document_free(&chunks);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[CHUNKER] Artifact: %s\n",
            output_path
        );

        printf(
            "[CHUNKER] Artifact write: PASS\n"
        );

        documents++;

        rb_chunker_source_document_free(&source);
        rb_chunker_chunk_document_free(&chunks);

    } while (
        FindNextFileA(
            search,
            &find_data
        )
    );

    FindClose(search);

    printf(
        "[CHUNKER] Structured artifacts processed: %u\n",
        documents
    );

    return RB_MODULE_OK;
}


static void rb_chunker_shutdown(
    void
)
{
    /*
     * No persistent module-owned resources
     * at this time.
     */
}


/*
 * ------------------------------------------------------------
 * Module descriptor
 * ------------------------------------------------------------
 *
 * API 1.2 descriptor.
 *
 * Chunker executes at stage 200.
 */

static const rb_module_descriptor_t
rb_chunker_descriptor =
{
    RB_CHUNKER_MODULE_ID,
    RB_CHUNKER_MODULE_NAME,

    RB_CHUNKER_VERSION_MAJOR,
    RB_CHUNKER_VERSION_MINOR,
    RB_CHUNKER_VERSION_PATCH,

    RB_MODULE_API_MAJOR,
    RB_MODULE_API_MINOR,

    200,

    rb_chunker_qualify,
    rb_chunker_execute,
    rb_chunker_shutdown
};


RB_MODULE_EXPORT const rb_module_descriptor_t*
rb_module_get_descriptor(
    void
)
{
    return &rb_chunker_descriptor;
}