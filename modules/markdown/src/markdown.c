#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "markdown.h"

#define RB_MARKDOWN_ARTIFACT_SUFFIX ".rag.json"
#define RB_MARKDOWN_HEADING_LEVELS   6


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


static size_t rb_markdown_line_end(
    const char* source,
    size_t source_length,
    size_t offset
)
{
    size_t end;

    end = offset;

    while (end < source_length &&
        !rb_markdown_is_line_end(
            source[end]
        ))
    {
        end++;
    }

    return end;
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
    size_t parent_heading_index,
    size_t source_offset,
    size_t source_length,
    size_t content_offset,
    size_t content_length,
    size_t fence_info_offset,
    size_t fence_info_length
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

    block->parent_heading_index =
        parent_heading_index;

    block->source_offset =
        source_offset;

    block->source_length =
        source_length;

    block->content_offset =
        content_offset;

    block->content_length =
        content_length;

    block->fence_info_offset =
        fence_info_offset;

    block->fence_info_length =
        fence_info_length;

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
        level < RB_MARKDOWN_HEADING_LEVELS)
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


static int rb_markdown_blockquote(
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

    if (start >= end ||
        source[start] != '>')
    {
        return 0;
    }

    index =
        start + 1;

    if (index < end &&
        source[index] == ' ')
    {
        index++;
    }

    *content_start =
        index;

    return 1;
}


static int rb_markdown_fence_open(
    const char* source,
    size_t start,
    size_t end,
    char* fence_character,
    size_t* fence_length,
    size_t* info_offset,
    size_t* info_length
)
{
    size_t index;
    size_t count;

    if (source == NULL ||
        fence_character == NULL ||
        fence_length == NULL ||
        info_offset == NULL ||
        info_length == NULL)
    {
        return 0;
    }

    if (start >= end)
    {
        return 0;
    }

    if (source[start] != '`' &&
        source[start] != '~')
    {
        return 0;
    }

    *fence_character =
        source[start];

    index =
        start;

    while (index < end &&
        source[index] ==
        *fence_character)
    {
        index++;
    }

    count =
        index - start;

    if (count < 3)
    {
        return 0;
    }

    if (*fence_character == '`')
    {
        size_t check;

        for (check = index;
            check < end;
            check++)
        {
            if (source[check] == '`')
            {
                return 0;
            }
        }
    }

    while (index < end &&
        (source[index] == ' ' ||
            source[index] == '\t'))
    {
        index++;
    }

    *fence_length =
        count;

    *info_offset =
        index;

    *info_length =
        end - index;

    return 1;
}


static int rb_markdown_fence_close(
    const char* source,
    size_t start,
    size_t end,
    char fence_character,
    size_t minimum_fence_length
)
{
    size_t index;
    size_t count;

    if (source == NULL ||
        start >= end ||
        source[start] != fence_character)
    {
        return 0;
    }

    index =
        start;

    while (index < end &&
        source[index] ==
        fence_character)
    {
        index++;
    }

    count =
        index - start;

    if (count < minimum_fence_length)
    {
        return 0;
    }

    while (index < end)
    {
        if (source[index] != ' ' &&
            source[index] != '\t')
        {
            return 0;
        }

        index++;
    }

    return 1;
}


static size_t rb_markdown_current_heading(
    const size_t heading_stack[
        RB_MARKDOWN_HEADING_LEVELS
    ]
)
{
    int level;

    for (level =
        RB_MARKDOWN_HEADING_LEVELS - 1;
        level >= 0;
        level--)
    {
        if (heading_stack[level] !=
            RB_MARKDOWN_NO_PARENT)
        {
            return heading_stack[level];
        }
    }

    return RB_MARKDOWN_NO_PARENT;
}


static size_t rb_markdown_parent_for_heading(
    const size_t heading_stack[
        RB_MARKDOWN_HEADING_LEVELS
    ],
    unsigned int heading_level
)
{
    int level;

    if (heading_level <= 1)
    {
        return RB_MARKDOWN_NO_PARENT;
    }

    for (level =
        (int)heading_level - 2;
        level >= 0;
        level--)
    {
        if (heading_stack[level] !=
            RB_MARKDOWN_NO_PARENT)
        {
            return heading_stack[level];
        }
    }

    return RB_MARKDOWN_NO_PARENT;
}


static void rb_markdown_update_heading_stack(
    size_t heading_stack[
        RB_MARKDOWN_HEADING_LEVELS
    ],
    unsigned int heading_level,
    size_t heading_index
)
{
    size_t level;

    if (heading_level == 0 ||
        heading_level >
        RB_MARKDOWN_HEADING_LEVELS)
    {
        return;
    }

    heading_stack[
        heading_level - 1
    ] =
        heading_index;

        for (level = heading_level;
            level <
            RB_MARKDOWN_HEADING_LEVELS;
            level++)
        {
            heading_stack[level] =
                RB_MARKDOWN_NO_PARENT;
        }
}


static rb_markdown_result_t rb_markdown_parse_fenced_code(
    const char* source,
    size_t source_length,
    size_t opening_start,
    size_t opening_end,
    char fence_character,
    size_t fence_length,
    size_t info_offset,
    size_t info_length,
    size_t parent_heading_index,
    rb_markdown_document_t* document,
    size_t* next_offset
)
{
    size_t content_start;
    size_t scan;
    size_t closing_start;
    size_t closing_end;
    size_t source_end;
    size_t content_end;

    rb_markdown_result_t result;

    if (source == NULL ||
        document == NULL ||
        next_offset == NULL)
    {
        return RB_MARKDOWN_ERR_INVALID_ARGUMENT;
    }

    content_start =
        rb_markdown_skip_line_end(
            source,
            source_length,
            opening_end
        );

    scan =
        content_start;

    closing_start =
        RB_MARKDOWN_NO_PARENT;

    closing_end =
        RB_MARKDOWN_NO_PARENT;

    while (scan < source_length)
    {
        size_t line_end;

        line_end =
            rb_markdown_line_end(
                source,
                source_length,
                scan
            );

        if (rb_markdown_fence_close(
            source,
            scan,
            line_end,
            fence_character,
            fence_length
        ))
        {
            closing_start =
                scan;

            closing_end =
                line_end;

            break;
        }

        scan =
            rb_markdown_skip_line_end(
                source,
                source_length,
                line_end
            );
    }

    if (closing_start ==
        RB_MARKDOWN_NO_PARENT)
    {
        return RB_MARKDOWN_ERR_INVALID_FORMAT;
    }

    content_end =
        closing_start;

    if (content_end > content_start)
    {
        if (source[content_end - 1] == '\n')
        {
            content_end--;

            if (content_end > content_start &&
                source[content_end - 1] == '\r')
            {
                content_end--;
            }
        }
        else if (source[content_end - 1] == '\r')
        {
            content_end--;
        }
    }

    source_end =
        closing_end;

    result =
        rb_markdown_add_block(
            document,
            RB_MARKDOWN_BLOCK_FENCED_CODE_BLOCK,
            0,
            parent_heading_index,
            opening_start,
            source_end - opening_start,
            content_start,
            content_end - content_start,
            info_offset,
            info_length
        );

    if (result != RB_MARKDOWN_OK)
    {
        return result;
    }

    *next_offset =
        rb_markdown_skip_line_end(
            source,
            source_length,
            closing_end
        );

    return RB_MARKDOWN_OK;
}


rb_markdown_result_t rb_markdown_parse(
    const char* source,
    size_t source_length,
    rb_markdown_document_t* document
)
{
    size_t offset;

    size_t heading_stack[
        RB_MARKDOWN_HEADING_LEVELS
    ];

    size_t level;

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

    for (level = 0;
        level <
        RB_MARKDOWN_HEADING_LEVELS;
        level++)
    {
        heading_stack[level] =
            RB_MARKDOWN_NO_PARENT;
    }

    offset = 0;

    while (offset < source_length)
    {
        size_t line_start;
        size_t line_end;
        size_t content_start;
        size_t parent_heading_index;

        unsigned int heading_level;

        rb_markdown_result_t result;

        char fence_character;
        size_t fence_length;
        size_t fence_info_offset;
        size_t fence_info_length;

        line_start =
            offset;

        line_end =
            rb_markdown_line_end(
                source,
                source_length,
                offset
            );

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

        if (rb_markdown_fence_open(
            source,
            line_start,
            line_end,
            &fence_character,
            &fence_length,
            &fence_info_offset,
            &fence_info_length
        ))
        {
            parent_heading_index =
                rb_markdown_current_heading(
                    heading_stack
                );

            result =
                rb_markdown_parse_fenced_code(
                    source,
                    source_length,
                    line_start,
                    line_end,
                    fence_character,
                    fence_length,
                    fence_info_offset,
                    fence_info_length,
                    parent_heading_index,
                    document,
                    &offset
                );

            if (result != RB_MARKDOWN_OK)
            {
                return result;
            }

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
            size_t heading_index;

            parent_heading_index =
                rb_markdown_parent_for_heading(
                    heading_stack,
                    heading_level
                );

            heading_index =
                document->block_count;

            result =
                rb_markdown_add_block(
                    document,
                    RB_MARKDOWN_BLOCK_HEADING,
                    heading_level,
                    parent_heading_index,
                    line_start,
                    line_end - line_start,
                    content_start,
                    line_end - content_start,
                    0,
                    0
                );

            if (result != RB_MARKDOWN_OK)
            {
                return result;
            }

            rb_markdown_update_heading_stack(
                heading_stack,
                heading_level,
                heading_index
            );
        }
        else
        {
            parent_heading_index =
                rb_markdown_current_heading(
                    heading_stack
                );

            if (rb_markdown_blockquote(
                source,
                line_start,
                line_end,
                &content_start
            ))
            {
                result =
                    rb_markdown_add_block(
                        document,
                        RB_MARKDOWN_BLOCK_BLOCKQUOTE,
                        0,
                        parent_heading_index,
                        line_start,
                        line_end - line_start,
                        content_start,
                        line_end - content_start,
                        0,
                        0
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
                        parent_heading_index,
                        line_start,
                        line_end - line_start,
                        content_start,
                        line_end - content_start,
                        0,
                        0
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
                        parent_heading_index,
                        line_start,
                        line_end - line_start,
                        content_start,
                        line_end - content_start,
                        0,
                        0
                    );
            }
            else
            {
                result =
                    rb_markdown_add_block(
                        document,
                        RB_MARKDOWN_BLOCK_PARAGRAPH,
                        0,
                        parent_heading_index,
                        line_start,
                        line_end - line_start,
                        line_start,
                        line_end - line_start,
                        0,
                        0
                    );
            }

            if (result != RB_MARKDOWN_OK)
            {
                return result;
            }
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


static int rb_markdown_json_write_byte(
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

    return fputc(
        (int)value,
        file
    ) != EOF;
}


static int rb_markdown_json_write_string(
    FILE* file,
    const char* text,
    size_t length
)
{
    size_t index;

    if (file == NULL ||
        text == NULL)
    {
        return 0;
    }

    if (fputc('"', file) == EOF)
    {
        return 0;
    }

    for (index = 0;
        index < length;
        index++)
    {
        if (!rb_markdown_json_write_byte(
            file,
            (unsigned char)text[index]
        ))
        {
            return 0;
        }
    }

    if (fputc('"', file) == EOF)
    {
        return 0;
    }

    return 1;
}


static const char* rb_markdown_filename_from_path(
    const char* path
)
{
    const char* filename;
    const char* cursor;

    if (path == NULL)
    {
        return NULL;
    }

    filename = path;

    for (cursor = path;
        *cursor != '\0';
        cursor++)
    {
        if (*cursor == '\\' ||
            *cursor == '/')
        {
            filename =
                cursor + 1;
        }
    }

    return filename;
}


static int rb_markdown_build_artifact_path(
    const char* output_path,
    const char* source_filename,
    char* artifact_path,
    size_t artifact_path_size
)
{
    size_t filename_length;
    size_t base_length;
    int written;

    if (output_path == NULL ||
        source_filename == NULL ||
        artifact_path == NULL ||
        artifact_path_size == 0)
    {
        return 0;
    }

    filename_length =
        strlen(source_filename);

    base_length =
        filename_length;

    if (filename_length >= 3)
    {
        if (source_filename[filename_length - 3] == '.' &&
            (source_filename[filename_length - 2] == 'm' ||
                source_filename[filename_length - 2] == 'M') &&
            (source_filename[filename_length - 1] == 'd' ||
                source_filename[filename_length - 1] == 'D'))
        {
            base_length -= 3;
        }
    }

    written =
        snprintf(
            artifact_path,
            artifact_path_size,
            "%s\\%.*s%s",
            output_path,
            (int)base_length,
            source_filename,
            RB_MARKDOWN_ARTIFACT_SUFFIX
        );

    if (written < 0 ||
        (size_t)written >= artifact_path_size)
    {
        return 0;
    }

    return 1;
}


static int rb_markdown_write_parent_heading(
    FILE* file,
    size_t parent_heading_index
)
{
    if (file == NULL)
    {
        return 0;
    }

    if (parent_heading_index ==
        RB_MARKDOWN_NO_PARENT)
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
        (parent_heading_index + 1)
    ) >= 0;
}


static int rb_markdown_write_artifact(
    const char* artifact_path,
    const char* source_filename,
    const char* source,
    size_t source_length,
    const rb_markdown_document_t* document
)
{
    FILE* file = NULL;
    size_t index;

    if (artifact_path == NULL ||
        source_filename == NULL ||
        source == NULL ||
        document == NULL)
    {
        return 0;
    }

    if (fopen_s(
        &file,
        artifact_path,
        "wb"
    ) != 0 ||
        file == NULL)
    {
        return 0;
    }

    if (fputs("{\n", file) < 0 ||
        fputs("  \"format\": \"RB-MARKDOWN\",\n", file) < 0 ||
        fputs("  \"format_version\": 2,\n", file) < 0 ||
        fputs("  \"source\": {\n", file) < 0 ||
        fputs("    \"filename\": ", file) < 0)
    {
        fclose(file);
        return 0;
    }

    if (!rb_markdown_json_write_string(
        file,
        source_filename,
        strlen(source_filename)
    ))
    {
        fclose(file);
        return 0;
    }

    if (fprintf(
        file,
        ",\n"
        "    \"size\": %llu\n"
        "  },\n"
        "  \"blocks\": [\n",
        (unsigned long long)source_length
    ) < 0)
    {
        fclose(file);
        return 0;
    }

    for (index = 0;
        index < document->block_count;
        index++)
    {
        const rb_markdown_block_t* block;
        const char* type;

        block =
            &document->blocks[index];

        type =
            rb_markdown_block_type_string(
                block->type
            );

        if (fprintf(
            file,
            "    {\n"
            "      \"index\": %llu,\n"
            "      \"type\": \"%s\",\n"
            "      \"heading_level\": %u,\n"
            "      \"parent_heading_index\": ",
            (unsigned long long)(index + 1),
            type,
            block->heading_level
        ) < 0)
        {
            fclose(file);
            return 0;
        }

        if (!rb_markdown_write_parent_heading(
            file,
            block->parent_heading_index
        ))
        {
            fclose(file);
            return 0;
        }

        if (fprintf(
            file,
            ",\n"
            "      \"source_offset\": %llu,\n"
            "      \"source_length\": %llu,\n"
            "      \"content_offset\": %llu,\n"
            "      \"content_length\": %llu",
            (unsigned long long)block->source_offset,
            (unsigned long long)block->source_length,
            (unsigned long long)block->content_offset,
            (unsigned long long)block->content_length
        ) < 0)
        {
            fclose(file);
            return 0;
        }

        if (block->type ==
            RB_MARKDOWN_BLOCK_FENCED_CODE_BLOCK)
        {
            if (fputs(
                ",\n"
                "      \"info\": ",
                file
            ) < 0)
            {
                fclose(file);
                return 0;
            }

            if (block->fence_info_offset >
                source_length ||
                block->fence_info_length >
                source_length -
                block->fence_info_offset)
            {
                fclose(file);
                return 0;
            }

            if (!rb_markdown_json_write_string(
                file,
                source +
                block->fence_info_offset,
                block->fence_info_length
            ))
            {
                fclose(file);
                return 0;
            }
        }

        if (fputs(
            ",\n"
            "      \"text\": ",
            file
        ) < 0)
        {
            fclose(file);
            return 0;
        }

        if (block->content_offset >
            source_length ||
            block->content_length >
            source_length -
            block->content_offset)
        {
            fclose(file);
            return 0;
        }

        if (!rb_markdown_json_write_string(
            file,
            source +
            block->content_offset,
            block->content_length
        ))
        {
            fclose(file);
            return 0;
        }

        if (index + 1 <
            document->block_count)
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

    source = "# Heading";

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

    source = "### Heading";

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

    source = "Paragraph text";

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

    source = "- Item";

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

    source = "1. Item";

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

    source = "x";

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

    source =
        "```text\n"
        ".github/\n"
        "docs/\n"
        "```";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_FENCED_CODE_BLOCK &&
        document.blocks[0].content_length ==
        strlen(".github/\ndocs/"))
    {
        passed++;
    }

    source =
        "```markdown\n"
        "# Literal Heading\n"
        "```";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_FENCED_CODE_BLOCK &&
        document.blocks[0].fence_info_length ==
        strlen("markdown") &&
        strncmp(
            source +
            document.blocks[0].fence_info_offset,
            "markdown",
            strlen("markdown")
        ) == 0)
    {
        passed++;
    }

    source =
        "> Quoted text";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 1 &&
        document.blocks[0].type ==
        RB_MARKDOWN_BLOCK_BLOCKQUOTE &&
        document.blocks[0].content_offset == 2 &&
        document.blocks[0].content_length ==
        strlen("Quoted text"))
    {
        passed++;
    }

    source =
        "## Section\n"
        "Paragraph";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 2 &&
        document.blocks[1].parent_heading_index == 0)
    {
        passed++;
    }

    source =
        "## Parent\n"
        "### Child\n"
        "- Item";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 3 &&
        document.blocks[0].parent_heading_index ==
        RB_MARKDOWN_NO_PARENT &&
        document.blocks[1].parent_heading_index == 0 &&
        document.blocks[2].parent_heading_index == 1)
    {
        passed++;
    }

    source =
        "## First\n"
        "### Child\n"
        "## Second\n"
        "Paragraph";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result == RB_MARKDOWN_OK &&
        document.block_count == 4 &&
        document.blocks[2].parent_heading_index ==
        RB_MARKDOWN_NO_PARENT &&
        document.blocks[3].parent_heading_index == 2)
    {
        passed++;
    }

    result->tests_executed =
        16;

    result->tests_passed =
        passed;

    result->tests_failed =
        result->tests_executed -
        result->tests_passed;

    result->negative_test_executed =
        1;

    source =
        "```text\n"
        "unclosed";

    parse_result =
        rb_markdown_parse(
            source,
            strlen(source),
            &document
        );

    if (parse_result ==
        RB_MARKDOWN_ERR_INVALID_FORMAT)
    {
        result->negative_test_passed =
            1;
    }

    if (result->tests_failed != 0 ||
        !result->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}


static rb_module_result_t rb_markdown_execute(
    const rb_module_execution_context_t* context
)
{
    WIN32_FIND_DATAA find_data;
    HANDLE search;

    char pattern[RB_MODULE_PATH_MAX];

    unsigned int documents = 0;

    if (context == NULL ||
        context->source_path == NULL ||
        context->source_path[0] == '\0' ||
        context->output_path == NULL ||
        context->output_path[0] == '\0')
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (snprintf(
        pattern,
        sizeof(pattern),
        "%s\\*.md",
        context->source_path
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
                "[MARKDOWN] Documents processed: 0\n"
            );

            return RB_MODULE_OK;
        }

        return RB_MODULE_ERR_EXECUTION;
    }

    do
    {
        char source_path[RB_MODULE_PATH_MAX];
        char artifact_path[RB_MODULE_PATH_MAX];

        const char* source_filename;

        FILE* file = NULL;

        long size;

        char* source;

        size_t read_count;

        rb_markdown_document_t document;

        rb_markdown_result_t parse_result;

        if (find_data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (snprintf(
            source_path,
            sizeof(source_path),
            "%s\\%s",
            context->source_path,
            find_data.cFileName
        ) < 0)
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        source_filename =
            rb_markdown_filename_from_path(
                source_path
            );

        if (source_filename == NULL ||
            source_filename[0] == '\0')
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (!rb_markdown_build_artifact_path(
            context->output_path,
            source_filename,
            artifact_path,
            sizeof(artifact_path)
        ))
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (fopen_s(
            &file,
            source_path,
            "rb"
        ) != 0 ||
            file == NULL)
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (fseek(
            file,
            0,
            SEEK_END
        ) != 0)
        {
            fclose(file);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        size =
            ftell(file);

        if (size < 0)
        {
            fclose(file);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (fseek(
            file,
            0,
            SEEK_SET
        ) != 0)
        {
            fclose(file);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        source =
            (char*)malloc(
                (size_t)size + 1
            );

        if (source == NULL)
        {
            fclose(file);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        read_count =
            fread(
                source,
                1,
                (size_t)size,
                file
            );

        fclose(file);

        if (read_count !=
            (size_t)size)
        {
            free(source);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        source[read_count] =
            '\0';

        printf(
            "[MARKDOWN] Source: %s\n",
            source_path
        );

        printf(
            "[MARKDOWN] Read: %u bytes\n",
            (unsigned int)read_count
        );

        parse_result =
            rb_markdown_parse(
                source,
                read_count,
                &document
            );

        if (parse_result !=
            RB_MARKDOWN_OK)
        {
            fprintf(
                stderr,
                "[MARKDOWN] Parse FAIL: %s\n",
                rb_markdown_result_string(
                    parse_result
                )
            );

            free(source);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[MARKDOWN] Parse PASS: %u block(s)\n",
            (unsigned int)
            document.block_count
        );

        if (!rb_markdown_write_artifact(
            artifact_path,
            source_filename,
            source,
            read_count,
            &document
        ))
        {
            fprintf(
                stderr,
                "[MARKDOWN] Artifact write FAIL: %s\n",
                artifact_path
            );

            free(source);
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[MARKDOWN] Artifact: %s\n",
            artifact_path
        );

        printf(
            "[MARKDOWN] Artifact write: PASS\n"
        );

        documents++;

        free(source);

    } while (
        FindNextFileA(
            search,
            &find_data
        )
        );

    FindClose(search);

    printf(
        "[MARKDOWN] Documents processed: %u\n",
        documents
    );

    return RB_MODULE_OK;
}


static void rb_markdown_shutdown(
    void
)
{
}


/*
 * API 1.2 descriptor.
 *
 * Markdown executes at stage 100.
 */
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

    100,

    rb_markdown_qualify,
    rb_markdown_execute,
    rb_markdown_shutdown
};


RB_MODULE_EXPORT const rb_module_descriptor_t*
rb_module_get_descriptor(
    void
)
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

    case RB_MARKDOWN_BLOCK_FENCED_CODE_BLOCK:
        return "FENCED_CODE_BLOCK";

    case RB_MARKDOWN_BLOCK_BLOCKQUOTE:
        return "BLOCKQUOTE";

    case RB_MARKDOWN_BLOCK_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}