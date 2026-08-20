/*
 * STN-LABZ rag_builder
 * RB-CORPUS-AGGREGATOR
 *
 * Version 0.3.0
 * Authority: MCR 2026-08-19-CA
 *
 * Mission:
 *   Aggregate validated RB-PROVENANCE .corpus.json artifacts into one
 *   deterministic kb.corpus.json artifact.
 *
 * Execution stage:
 *   400
 *
 * Authority boundary:
 *   This module does not establish controlled-document authority.
 *   It consumes per-document corpus artifacts already produced by
 *   RB-PROVENANCE.
 *
 * Mutation boundary:
 *   Source .corpus.json artifacts are read only.
 *   Only kb.corpus.json and kb.corpus.json.tmp are module-owned outputs.
 */

#include "corpus_aggregator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct
{
    char root_document_id[RB_CORPUS_AGGREGATOR_ID_MAX];
    char revision_id[RB_CORPUS_AGGREGATOR_ID_MAX];
    char previous_revision[RB_CORPUS_AGGREGATOR_ID_MAX];
    char canonical_sha256[RB_CORPUS_AGGREGATOR_SHA256_HEX];
    char status[RB_CORPUS_AGGREGATOR_ID_MAX];
    char source_filename[RB_CORPUS_AGGREGATOR_PATH_MAX];
    char corpus_filename[RB_CORPUS_AGGREGATOR_PATH_MAX];

    char* data;
    size_t data_length;
    size_t record_count;
} rb_ca_corpus_t;


typedef struct
{
    char root_document_id[RB_CORPUS_AGGREGATOR_ID_MAX];
    char revision_id[RB_CORPUS_AGGREGATOR_ID_MAX];
    char previous_revision[RB_CORPUS_AGGREGATOR_ID_MAX];
    char sha256[RB_CORPUS_AGGREGATOR_SHA256_HEX];
    char status[RB_CORPUS_AGGREGATOR_ID_MAX];
} rb_ca_index_record_t;


static int rb_ca_has_suffix(
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

    if (suffix_length > value_length)
    {
        return 0;
    }

    return strcmp(
        value + value_length - suffix_length,
        suffix
    ) == 0;
}


static int rb_ca_is_source_corpus_name(
    const char* filename
)
{
    if (filename == NULL ||
        filename[0] == '\0')
    {
        return 0;
    }

    if (strcmp(
            filename,
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME
        ) == 0 ||
        strcmp(
            filename,
            RB_CORPUS_AGGREGATOR_TEMP_NAME
        ) == 0)
    {
        return 0;
    }

    return rb_ca_has_suffix(
        filename,
        RB_CORPUS_AGGREGATOR_INPUT_SUFFIX
    );
}


static int rb_ca_build_path(
    const char* directory,
    const char* filename,
    char* output,
    size_t output_size
)
{
    size_t directory_length;
    int written;

    if (directory == NULL ||
        filename == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    directory_length = strlen(directory);

    if (directory_length == 0)
    {
        return 0;
    }

    if (directory[directory_length - 1] == '\\' ||
        directory[directory_length - 1] == '/')
    {
        written =
            snprintf(
                output,
                output_size,
                "%s%s",
                directory,
                filename
            );
    }
    else
    {
#ifdef _WIN32
        written =
            snprintf(
                output,
                output_size,
                "%s\\%s",
                directory,
                filename
            );
#else
        written =
            snprintf(
                output,
                output_size,
                "%s/%s",
                directory,
                filename
            );
#endif
    }

    return written > 0 &&
           (size_t)written < output_size;
}


static int rb_ca_read_file(
    const char* path,
    char** data,
    size_t* length
)
{
    FILE* file = NULL;
    long size;
    char* buffer;

    if (path == NULL ||
        data == NULL ||
        length == NULL)
    {
        return 0;
    }

    *data = NULL;
    *length = 0;

#ifdef _WIN32
    if (fopen_s(
            &file,
            path,
            "rb"
        ) != 0 ||
        file == NULL)
    {
        return 0;
    }
#else
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
#endif

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return 0;
    }

    size = ftell(file);

    if (size < 0 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }

    buffer =
        (char*)malloc(
            (size_t)size + 1
        );

    if (buffer == NULL)
    {
        fclose(file);
        return 0;
    }

    if (size > 0 &&
        fread(
            buffer,
            1,
            (size_t)size,
            file
        ) != (size_t)size)
    {
        free(buffer);
        fclose(file);
        return 0;
    }

    buffer[(size_t)size] = '\0';

    fclose(file);

    *data = buffer;
    *length = (size_t)size;

    return 1;
}


static const char* rb_ca_skip_ws(
    const char* cursor,
    const char* end
)
{
    while (cursor < end &&
        isspace(
            (unsigned char)*cursor
        ))
    {
        cursor++;
    }

    return cursor;
}


static int rb_ca_json_string_value(
    const char* data,
    size_t length,
    const char* key,
    char* output,
    size_t output_size
)
{
    char pattern[256];
    int written;
    const char* cursor;
    const char* end;
    size_t used = 0;

    if (data == NULL ||
        key == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    written =
        snprintf(
            pattern,
            sizeof(pattern),
            "\"%s\"",
            key
        );

    if (written <= 0 ||
        (size_t)written >= sizeof(pattern))
    {
        return 0;
    }

    cursor = strstr(data, pattern);

    if (cursor == NULL)
    {
        return 0;
    }

    end = data + length;
    cursor += strlen(pattern);
    cursor = rb_ca_skip_ws(cursor, end);

    if (cursor >= end ||
        *cursor != ':')
    {
        return 0;
    }

    cursor++;
    cursor = rb_ca_skip_ws(cursor, end);

    if (cursor >= end ||
        *cursor != '"')
    {
        return 0;
    }

    cursor++;

    while (cursor < end)
    {
        unsigned char ch =
            (unsigned char)*cursor++;

        if (ch == '"')
        {
            output[used] = '\0';
            return 1;
        }

        if (ch == '\\')
        {
            unsigned char escaped;

            if (cursor >= end)
            {
                return 0;
            }

            escaped =
                (unsigned char)*cursor++;

            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                ch = escaped;
                break;

            case 'b':
                ch = '\b';
                break;

            case 'f':
                ch = '\f';
                break;

            case 'n':
                ch = '\n';
                break;

            case 'r':
                ch = '\r';
                break;

            case 't':
                ch = '\t';
                break;

            default:
                /*
                 * Controlled identity fields are ASCII and must not depend
                 * on unicode escapes. Reject unexpected escape forms.
                 */
                return 0;
            }
        }

        if (used + 1 >= output_size)
        {
            return 0;
        }

        output[used++] = (char)ch;
    }

    return 0;
}


static int rb_ca_find_matching(
    const char* start,
    const char* end,
    char open_char,
    char close_char,
    const char** match
)
{
    const char* cursor;
    size_t depth = 0;
    int in_string = 0;
    int escaped = 0;

    if (start == NULL ||
        end == NULL ||
        match == NULL ||
        start >= end ||
        *start != open_char)
    {
        return 0;
    }

    for (cursor = start;
         cursor < end;
         cursor++)
    {
        char ch = *cursor;

        if (in_string)
        {
            if (escaped)
            {
                escaped = 0;
            }
            else if (ch == '\\')
            {
                escaped = 1;
            }
            else if (ch == '"')
            {
                in_string = 0;
            }

            continue;
        }

        if (ch == '"')
        {
            in_string = 1;
            continue;
        }

        if (ch == open_char)
        {
            depth++;
        }
        else if (ch == close_char)
        {
            if (depth == 0)
            {
                return 0;
            }

            depth--;

            if (depth == 0)
            {
                *match = cursor;
                return 1;
            }
        }
    }

    return 0;
}


static int rb_ca_records_count(
    const char* data,
    size_t length,
    size_t* count
)
{
    const char* marker;
    const char* cursor;
    const char* end;
    const char* array_end;
    size_t found = 0;

    if (data == NULL ||
        count == NULL)
    {
        return 0;
    }

    marker = strstr(
        data,
        "\"records\""
    );

    if (marker == NULL)
    {
        return 0;
    }

    end = data + length;
    cursor =
        strchr(
            marker,
            '['
        );

    if (cursor == NULL ||
        cursor >= end ||
        !rb_ca_find_matching(
            cursor,
            end,
            '[',
            ']',
            &array_end
        ))
    {
        return 0;
    }

    cursor++;

    while (cursor < array_end)
    {
        const char* object_end;

        cursor =
            rb_ca_skip_ws(
                cursor,
                array_end
            );

        if (cursor >= array_end)
        {
            break;
        }

        if (*cursor == ',')
        {
            cursor++;
            continue;
        }

        if (*cursor != '{' ||
            !rb_ca_find_matching(
                cursor,
                array_end,
                '{',
                '}',
                &object_end
            ))
        {
            return 0;
        }

        found++;
        cursor = object_end + 1;
    }

    *count = found;
    return 1;
}


static int rb_ca_parse_corpus(
    const char* corpus_filename,
    char* data,
    size_t data_length,
    rb_ca_corpus_t* corpus
)
{
    char format[RB_CORPUS_AGGREGATOR_ID_MAX];
    char authority_type[RB_CORPUS_AGGREGATOR_ID_MAX];

    if (corpus_filename == NULL ||
        data == NULL ||
        corpus == NULL)
    {
        return 0;
    }

    memset(
        corpus,
        0,
        sizeof(*corpus)
    );

    if (!rb_ca_json_string_value(
            data,
            data_length,
            "corpus_format",
            format,
            sizeof(format)
        ) ||
        strcmp(
            format,
            "STN-LABZ-RAG-CORPUS"
        ) != 0)
    {
        return 0;
    }

    if (!rb_ca_json_string_value(
            data,
            data_length,
            "filename",
            corpus->source_filename,
            sizeof(corpus->source_filename)
        ) ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "type",
            authority_type,
            sizeof(authority_type)
        ) ||
        strcmp(
            authority_type,
            "CHAIN"
        ) != 0 ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "root_document_id",
            corpus->root_document_id,
            sizeof(corpus->root_document_id)
        ) ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "revision_id",
            corpus->revision_id,
            sizeof(corpus->revision_id)
        ) ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "previous_revision",
            corpus->previous_revision,
            sizeof(corpus->previous_revision)
        ) ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "canonical_sha256",
            corpus->canonical_sha256,
            sizeof(corpus->canonical_sha256)
        ) ||
        !rb_ca_json_string_value(
            data,
            data_length,
            "status",
            corpus->status,
            sizeof(corpus->status)
        ))
    {
        return 0;
    }

    if (strcmp(
            corpus->status,
            "Approved"
        ) != 0 ||
        strlen(
            corpus->canonical_sha256
        ) != 64)
    {
        return 0;
    }

    if (!rb_ca_records_count(
            data,
            data_length,
            &corpus->record_count
        ))
    {
        return 0;
    }

#ifdef _WIN32
    strcpy_s(
        corpus->corpus_filename,
        sizeof(corpus->corpus_filename),
        corpus_filename
    );
#else
    snprintf(
        corpus->corpus_filename,
        sizeof(corpus->corpus_filename),
        "%s",
        corpus_filename
    );
#endif

    corpus->data = data;
    corpus->data_length = data_length;

    return 1;
}


static int rb_ca_identity_equal(
    const rb_ca_corpus_t* left,
    const rb_ca_corpus_t* right
)
{
    return left != NULL &&
           right != NULL &&
           strcmp(
               left->root_document_id,
               right->root_document_id
           ) == 0 &&
           strcmp(
               left->revision_id,
               right->revision_id
           ) == 0;
}


static int rb_ca_exact_duplicate(
    const rb_ca_corpus_t* left,
    const rb_ca_corpus_t* right
)
{
    return rb_ca_identity_equal(
               left,
               right
           ) &&
           strcmp(
               left->canonical_sha256,
               right->canonical_sha256
           ) == 0;
}


static int rb_ca_identity_conflict(
    const rb_ca_corpus_t* left,
    const rb_ca_corpus_t* right
)
{
    return rb_ca_identity_equal(
               left,
               right
           ) &&
           strcmp(
               left->canonical_sha256,
               right->canonical_sha256
           ) != 0;
}


static int rb_ca_corpus_compare(
    const void* left_value,
    const void* right_value
)
{
    const rb_ca_corpus_t* left =
        (const rb_ca_corpus_t*)left_value;

    const rb_ca_corpus_t* right =
        (const rb_ca_corpus_t*)right_value;

    int result;

    result =
        strcmp(
            left->root_document_id,
            right->root_document_id
        );

    if (result != 0)
    {
        return result;
    }

    result =
        strcmp(
            left->revision_id,
            right->revision_id
        );

    if (result != 0)
    {
        return result;
    }

    result =
        strcmp(
            left->canonical_sha256,
            right->canonical_sha256
        );

    if (result != 0)
    {
        return result;
    }

    return strcmp(
        left->corpus_filename,
        right->corpus_filename
    );
}



static int rb_ca_parse_index(
    const char* data,
    size_t length,
    rb_ca_index_record_t* records,
    size_t capacity,
    size_t* record_count
)
{
    const char* cursor;
    const char* end;
    size_t count = 0;

    if (data == NULL ||
        records == NULL ||
        record_count == NULL ||
        capacity == 0)
    {
        return 0;
    }

    end = data + length;
    cursor = rb_ca_skip_ws(data, end);

    if (cursor >= end ||
        *cursor != '[')
    {
        return 0;
    }

    cursor++;

    while (cursor < end)
    {
        const char* object_end;
        size_t object_length;
        char* object_data;

        cursor = rb_ca_skip_ws(cursor, end);

        if (cursor >= end)
        {
            return 0;
        }

        if (*cursor == ']')
        {
            cursor++;
            break;
        }

        if (*cursor == ',')
        {
            cursor++;
            continue;
        }

        if (*cursor != '{' ||
            count >= capacity ||
            !rb_ca_find_matching(
                cursor,
                end,
                '{',
                '}',
                &object_end
            ))
        {
            return 0;
        }

        object_length =
            (size_t)(object_end - cursor) + 1;

        object_data =
            (char*)malloc(
                object_length + 1
            );

        if (object_data == NULL)
        {
            return 0;
        }

        memcpy(
            object_data,
            cursor,
            object_length
        );

        object_data[object_length] = '\0';

        memset(
            &records[count],
            0,
            sizeof(records[count])
        );

        if (!rb_ca_json_string_value(
                object_data,
                object_length,
                "root_document_id",
                records[count].root_document_id,
                sizeof(records[count].root_document_id)
            ) ||
            !rb_ca_json_string_value(
                object_data,
                object_length,
                "revision_id",
                records[count].revision_id,
                sizeof(records[count].revision_id)
            ) ||
            !rb_ca_json_string_value(
                object_data,
                object_length,
                "previous_revision",
                records[count].previous_revision,
                sizeof(records[count].previous_revision)
            ) ||
            !rb_ca_json_string_value(
                object_data,
                object_length,
                "sha256",
                records[count].sha256,
                sizeof(records[count].sha256)
            ) ||
            !rb_ca_json_string_value(
                object_data,
                object_length,
                "status",
                records[count].status,
                sizeof(records[count].status)
            ))
        {
            free(object_data);
            return 0;
        }

        free(object_data);

        if (strlen(records[count].sha256) != 64)
        {
            return 0;
        }

        count++;
        cursor = object_end + 1;
    }

    cursor = rb_ca_skip_ws(cursor, end);

    if (cursor != end ||
        count == 0)
    {
        return 0;
    }

    *record_count = count;
    return 1;
}


static int rb_ca_index_current_for_root(
    const rb_ca_index_record_t* records,
    size_t record_count,
    const char* root_document_id,
    rb_ca_index_record_t* current
)
{
    size_t i;
    size_t j;
    size_t revision_count = 0;
    size_t terminal_count = 0;
    size_t root_count = 0;
    rb_ca_index_record_t terminal;
    rb_ca_index_record_t root_record;

    if (records == NULL ||
        root_document_id == NULL ||
        current == NULL)
    {
        return 0;
    }

    memset(&terminal, 0, sizeof(terminal));
    memset(&root_record, 0, sizeof(root_record));

    /*
     * Revision lineage is separate from the root artifact.  When approved
     * revisions exist, current means the unique approved revision that is not
     * named as Previous Revision by another approved revision of the same root.
     * The root (revision NONE) is selected only when no approved revisions
     * exist for that root.
     */
    for (i = 0; i < record_count; i++)
    {
        if (strcmp(records[i].root_document_id, root_document_id) != 0 ||
            strcmp(records[i].status, "Approved") != 0)
        {
            continue;
        }

        if (strcmp(records[i].revision_id, "NONE") == 0)
        {
            root_record = records[i];
            root_count++;
        }
        else
        {
            revision_count++;
        }
    }

    if (revision_count == 0)
    {
        if (root_count != 1)
        {
            return 0;
        }

        *current = root_record;
        return 1;
    }

    for (i = 0; i < record_count; i++)
    {
        int referenced = 0;

        if (strcmp(records[i].root_document_id, root_document_id) != 0 ||
            strcmp(records[i].status, "Approved") != 0 ||
            strcmp(records[i].revision_id, "NONE") == 0)
        {
            continue;
        }

        for (j = 0; j < record_count; j++)
        {
            if (i == j ||
                strcmp(records[j].root_document_id, root_document_id) != 0 ||
                strcmp(records[j].status, "Approved") != 0 ||
                strcmp(records[j].revision_id, "NONE") == 0)
            {
                continue;
            }

            if (strcmp(
                    records[j].previous_revision,
                    records[i].revision_id
                ) == 0)
            {
                referenced = 1;
                break;
            }
        }

        if (!referenced)
        {
            terminal = records[i];
            terminal_count++;
        }
    }

    if (terminal_count != 1)
    {
        return 0;
    }

    *current = terminal;
    return 1;
}


static int rb_ca_chainlog_has_registration(
    const char* chainlog,
    size_t chainlog_length,
    const rb_ca_index_record_t* record
)
{
    const char* cursor;
    const char* end;

    if (chainlog == NULL ||
        record == NULL)
    {
        return 0;
    }

    cursor = chainlog;
    end = chainlog + chainlog_length;

    while (cursor < end)
    {
        const char* line_end = cursor;
        char line[2048];
        size_t line_length;
        char* fields[8];
        size_t field_count = 0;
        char* token;
        char* context = NULL;

        while (line_end < end &&
            *line_end != '\n' &&
            *line_end != '\r')
        {
            line_end++;
        }

        line_length =
            (size_t)(line_end - cursor);

        if (line_length > 0 &&
            line_length < sizeof(line))
        {
            memcpy(line, cursor, line_length);
            line[line_length] = '\0';

#ifdef _WIN32
            token = strtok_s(line, "|", &context);
#else
            token = strtok_r(line, "|", &context);
#endif

            while (token != NULL &&
                field_count < 8)
            {
                fields[field_count++] = token;

#ifdef _WIN32
                token = strtok_s(NULL, "|", &context);
#else
                token = strtok_r(NULL, "|", &context);
#endif
            }

            if (field_count == 8 &&
                strcmp(fields[1], "REGISTER") == 0 &&
                strcmp(fields[2], record->root_document_id) == 0 &&
                strcmp(fields[3], record->revision_id) == 0 &&
                strcmp(fields[4], record->previous_revision) == 0 &&
                strcmp(fields[5], record->sha256) == 0)
            {
                return 1;
            }
        }

        cursor = line_end;

        while (cursor < end &&
            (*cursor == '\n' ||
             *cursor == '\r'))
        {
            cursor++;
        }
    }

    return 0;
}


static int rb_ca_corpus_matches_index(
    const rb_ca_corpus_t* corpus,
    const rb_ca_index_record_t* record
)
{
    return corpus != NULL &&
           record != NULL &&
           strcmp(
               corpus->root_document_id,
               record->root_document_id
           ) == 0 &&
           strcmp(
               corpus->revision_id,
               record->revision_id
           ) == 0 &&
           strcmp(
               corpus->canonical_sha256,
               record->sha256
           ) == 0 &&
           strcmp(
               corpus->status,
               "Approved"
           ) == 0;
}


static int rb_ca_files_equal(
    const char* left_path,
    const char* right_path
)
{
    FILE* left = NULL;
    FILE* right = NULL;
    unsigned char left_buffer[8192];
    unsigned char right_buffer[8192];
    int equal = 1;

#ifdef _WIN32
    if (fopen_s(
            &left,
            left_path,
            "rb"
        ) != 0 ||
        left == NULL)
    {
        return 0;
    }

    if (fopen_s(
            &right,
            right_path,
            "rb"
        ) != 0 ||
        right == NULL)
    {
        fclose(left);
        return 0;
    }
#else
    left = fopen(left_path, "rb");
    right = fopen(right_path, "rb");

    if (left == NULL ||
        right == NULL)
    {
        if (left != NULL)
        {
            fclose(left);
        }

        if (right != NULL)
        {
            fclose(right);
        }

        return 0;
    }
#endif

    for (;;)
    {
        size_t left_read =
            fread(
                left_buffer,
                1,
                sizeof(left_buffer),
                left
            );

        size_t right_read =
            fread(
                right_buffer,
                1,
                sizeof(right_buffer),
                right
            );

        if (left_read != right_read ||
            memcmp(
                left_buffer,
                right_buffer,
                left_read
            ) != 0)
        {
            equal = 0;
            break;
        }

        if (left_read == 0)
        {
            break;
        }
    }

    fclose(left);
    fclose(right);

    return equal;
}


static int rb_ca_write_aggregate(
    const char* output_path,
    const char* temp_path,
    const rb_ca_corpus_t* corpora,
    size_t corpus_count,
    size_t duplicate_count,
    int* rewritten
)
{
    FILE* file = NULL;
    size_t i;
    size_t total_records = 0;

    if (rewritten != NULL)
    {
        *rewritten = 0;
    }

    if (output_path == NULL ||
        temp_path == NULL ||
        corpora == NULL ||
        corpus_count == 0)
    {
        return 0;
    }

    for (i = 0;
         i < corpus_count;
         i++)
    {
        total_records +=
            corpora[i].record_count;
    }

#ifdef _WIN32
    if (fopen_s(
            &file,
            temp_path,
            "wb"
        ) != 0 ||
        file == NULL)
    {
        return 0;
    }
#else
    file = fopen(temp_path, "wb");
    if (file == NULL)
    {
        return 0;
    }
#endif

    if (fprintf(
            file,
            "{\n"
            "  \"corpus_format\": \"%s\",\n"
            "  \"corpus_format_version\": %d,\n"
            "  \"producer\": {\n"
            "    \"module_id\": \"%s\",\n"
            "    \"module_version\": \"%d.%d.%d\"\n"
            "  },\n"
            "  \"summary\": {\n"
            "    \"corpus_count\": %llu,\n"
            "    \"record_count\": %llu,\n"
            "    \"exact_duplicates_skipped\": %llu\n"
            "  },\n"
            "  \"corpora\": [\n",
            RB_CORPUS_AGGREGATOR_FORMAT,
            RB_CORPUS_AGGREGATOR_FORMAT_VERSION,
            RB_CORPUS_AGGREGATOR_MODULE_ID,
            RB_CORPUS_AGGREGATOR_VERSION_MAJOR,
            RB_CORPUS_AGGREGATOR_VERSION_MINOR,
            RB_CORPUS_AGGREGATOR_VERSION_PATCH,
            (unsigned long long)corpus_count,
            (unsigned long long)total_records,
            (unsigned long long)duplicate_count
        ) < 0)
    {
        fclose(file);
        return 0;
    }

    for (i = 0;
         i < corpus_count;
         i++)
    {
        if (fputs(
                "    ",
                file
            ) < 0 ||
            fwrite(
                corpora[i].data,
                1,
                corpora[i].data_length,
                file
            ) != corpora[i].data_length)
        {
            fclose(file);
            return 0;
        }

        if (i + 1 < corpus_count)
        {
            if (fputs(
                    ",\n",
                    file
                ) < 0)
            {
                fclose(file);
                return 0;
            }
        }
        else
        {
            if (fputc(
                    '\n',
                    file
                ) == EOF)
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
        ) < 0 ||
        fflush(file) != 0)
    {
        fclose(file);
        return 0;
    }

#ifdef _WIN32
    {
        HANDLE handle =
            (HANDLE)_get_osfhandle(
                _fileno(file)
            );

        if (handle != INVALID_HANDLE_VALUE)
        {
            (void)FlushFileBuffers(handle);
        }
    }
#endif

    if (fclose(file) != 0)
    {
        return 0;
    }

    /*
     * Byte-identical aggregate: preserve existing target exactly and remove
     * only the module-owned temporary file.
     */
    if (rb_ca_files_equal(
            output_path,
            temp_path
        ))
    {
#ifdef _WIN32
        (void)DeleteFileA(
            temp_path
        );
#else
        (void)unlink(
            temp_path
        );
#endif

        return 1;
    }

#ifdef _WIN32
    if (!MoveFileExA(
            temp_path,
            output_path,
            MOVEFILE_REPLACE_EXISTING |
            MOVEFILE_WRITE_THROUGH
        ))
    {
        (void)DeleteFileA(
            temp_path
        );

        return 0;
    }
#else
    if (rename(
            temp_path,
            output_path
        ) != 0)
    {
        (void)unlink(
            temp_path
        );

        return 0;
    }
#endif

    if (rewritten != NULL)
    {
        *rewritten = 1;
    }

    return 1;
}


static void rb_ca_free_corpora(
    rb_ca_corpus_t* corpora,
    size_t count
)
{
    size_t i;

    if (corpora == NULL)
    {
        return;
    }

    for (i = 0;
         i < count;
         i++)
    {
        free(
            corpora[i].data
        );

        corpora[i].data = NULL;
    }
}


static rb_module_result_t rb_ca_qualify(
    rb_module_qualification_result_t* result
)
{
    unsigned int passed = 0;
    rb_ca_corpus_t a;
    rb_ca_corpus_t b;
    rb_ca_corpus_t c;
    rb_ca_corpus_t ordered[3];
    char path[RB_CORPUS_AGGREGATOR_PATH_MAX];

    static char corpus_json[] =
        "{"
        "\"corpus_format\":\"STN-LABZ-RAG-CORPUS\","
        "\"producer\":{\"module_id\":\"RB-PROVENANCE\",\"module_version\":\"0.3.14\"},"
        "\"source\":{"
        "\"filename\":\"DOC.md\","
        "\"size\":1,"
        "\"raw_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"authority\":{"
        "\"type\":\"CHAIN\","
        "\"root_document_id\":\"DOC\","
        "\"revision_id\":\"NONE\","
        "\"previous_revision\":\"NONE\","
        "\"canonical_sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
        "\"status\":\"Approved\""
        "}"
        "},"
        "\"records\":[{\"chunk_id\":\"DOC:NONE:000001\",\"chunk\":{\"index\":1}}]"
        "}";

    if (result == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c));

    /* Q01 */
    if (rb_ca_is_source_corpus_name(
            "DOC.corpus.json"
        ))
    {
        passed++;
    }

    /* Q02 */
    if (!rb_ca_is_source_corpus_name(
            "DOC.chunks.json"
        ))
    {
        passed++;
    }

    /* Q03 */
    if (!rb_ca_is_source_corpus_name(
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME
        ))
    {
        passed++;
    }

    /* Q04 */
    if (!rb_ca_is_source_corpus_name(
            RB_CORPUS_AGGREGATOR_TEMP_NAME
        ))
    {
        passed++;
    }

    /* Q05 - valid Provenance corpus contract */
    if (rb_ca_parse_corpus(
            "DOC.corpus.json",
            corpus_json,
            strlen(corpus_json),
            &a
        ) &&
        strcmp(
            a.root_document_id,
            "DOC"
        ) == 0 &&
        strcmp(
            a.revision_id,
            "NONE"
        ) == 0 &&
        a.record_count == 1)
    {
        passed++;
    }

    /* Q06 - exact duplicate */
    b = a;
    b.data = NULL;

    if (rb_ca_exact_duplicate(
            &a,
            &b
        ))
    {
        passed++;
    }

    /* Q07 - root and revision distinct */
#ifdef _WIN32
    strcpy_s(
        c.root_document_id,
        sizeof(c.root_document_id),
        "DOC"
    );
    strcpy_s(
        c.revision_id,
        sizeof(c.revision_id),
        "DOC.R1"
    );
    strcpy_s(
        c.canonical_sha256,
        sizeof(c.canonical_sha256),
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
    );
#else
    snprintf(c.root_document_id, sizeof(c.root_document_id), "%s", "DOC");
    snprintf(c.revision_id, sizeof(c.revision_id), "%s", "DOC.R1");
    snprintf(c.canonical_sha256, sizeof(c.canonical_sha256), "%s",
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
#endif

    if (!rb_ca_identity_equal(
            &a,
            &c
        ))
    {
        passed++;
    }

    /* Q08 - mandatory negative conflict */
    b = a;
    b.data = NULL;
#ifdef _WIN32
    strcpy_s(
        b.canonical_sha256,
        sizeof(b.canonical_sha256),
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
    );
#else
    snprintf(b.canonical_sha256, sizeof(b.canonical_sha256), "%s",
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
#endif

    result->negative_test_executed = 1;

    if (rb_ca_identity_conflict(
            &a,
            &b
        ))
    {
        result->negative_test_passed = 1;
        passed++;
    }

    /* Q09 - different revision is not a conflict */
    if (!rb_ca_identity_conflict(
            &a,
            &c
        ))
    {
        passed++;
    }

    /* Q10 - deterministic output path */
    if (rb_ca_build_path(
            "C:\\stn-labz\\rag\\output",
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME,
            path,
            sizeof(path)
        ) &&
        strstr(
            path,
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME
        ) != NULL)
    {
        passed++;
    }

    /* Q11 - deterministic identity ordering */
    ordered[0] = c;
    ordered[1] = a;
    ordered[2] = a;
#ifdef _WIN32
    strcpy_s(
        ordered[2].root_document_id,
        sizeof(ordered[2].root_document_id),
        "AAA"
    );
#else
    snprintf(ordered[2].root_document_id,
        sizeof(ordered[2].root_document_id), "%s", "AAA");
#endif

    qsort(
        ordered,
        3,
        sizeof(ordered[0]),
        rb_ca_corpus_compare
    );

    if (strcmp(
            ordered[0].root_document_id,
            "AAA"
        ) == 0)
    {
        passed++;
    }

    /* Q12 - malformed format rejected */
    {
        static char malformed[] =
            "{\"corpus_format\":\"WRONG\",\"records\":[]}";

        rb_ca_corpus_t bad;

        if (!rb_ca_parse_corpus(
                "BAD.corpus.json",
                malformed,
                strlen(malformed),
                &bad
            ))
        {
            passed++;
        }
    }

    /* Q13 - malformed missing records rejected */
    {
        static char malformed[] =
            "{"
            "\"corpus_format\":\"STN-LABZ-RAG-CORPUS\","
            "\"filename\":\"DOC.md\","
            "\"type\":\"CHAIN\","
            "\"root_document_id\":\"DOC\","
            "\"revision_id\":\"NONE\","
            "\"previous_revision\":\"NONE\","
            "\"canonical_sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
            "\"status\":\"Approved\""
            "}";

        rb_ca_corpus_t bad;

        if (!rb_ca_parse_corpus(
                "BAD.corpus.json",
                malformed,
                strlen(malformed),
                &bad
            ))
        {
            passed++;
        }
    }

    /* Q14 - output filename contract */
    if (strcmp(
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME,
            "kb.corpus.json"
        ) == 0)
    {
        passed++;
    }


    /* Q15 - root-only authority selects revision NONE. */
    {
        static const rb_ca_index_record_t index[] =
        {
            {
                "ROOT",
                "NONE",
                "NONE",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "Approved"
            }
        };

        rb_ca_index_record_t current;

        if (rb_ca_index_current_for_root(
                index,
                1,
                "ROOT",
                &current
            ) &&
            strcmp(current.revision_id, "NONE") == 0)
        {
            passed++;
        }
    }

    /* Q16 - revision lineage selects the unique terminal approved revision. */
    {
        static const rb_ca_index_record_t index[] =
        {
            {
                "ROOT",
                "NONE",
                "NONE",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "Approved"
            },
            {
                "ROOT",
                "ROOT.R1",
                "NONE",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                "Approved"
            },
            {
                "ROOT",
                "ROOT.R2",
                "ROOT.R1",
                "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
                "Approved"
            }
        };

        rb_ca_index_record_t current;

        if (rb_ca_index_current_for_root(
                index,
                3,
                "ROOT",
                &current
            ) &&
            strcmp(current.revision_id, "ROOT.R2") == 0)
        {
            passed++;
        }
    }

    result->tests_executed = 16;
    result->tests_passed = passed;
    result->tests_failed =
        result->tests_executed -
        result->tests_passed;

    printf(
        "[CORPUS_AGGREGATOR] Qualification tests: %u/%u\n",
        result->tests_passed,
        result->tests_executed
    );

    printf(
        "[CORPUS_AGGREGATOR] Negative validation: %s\n",
        result->negative_test_passed
            ? "PASS"
            : "FAIL"
    );

    if (result->tests_failed != 0 ||
        !result->negative_test_executed ||
        !result->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}


static rb_module_result_t rb_ca_execute(
    const rb_module_execution_context_t* context
)
{
    rb_ca_corpus_t* corpora = NULL;
    rb_ca_corpus_t* selected = NULL;
    rb_ca_index_record_t* index_records = NULL;

    char* index_data = NULL;
    size_t index_length = 0;
    size_t index_count = 0;

    char* chainlog_data = NULL;
    size_t chainlog_length = 0;

    size_t corpus_count = 0;
    size_t selected_count = 0;
    size_t duplicates = 0;
    size_t superseded = 0;
    size_t total_records = 0;
    size_t i;
    size_t j;

    char output_path[RB_CORPUS_AGGREGATOR_PATH_MAX];
    char temp_path[RB_CORPUS_AGGREGATOR_PATH_MAX];
    int rewritten = 0;

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE search;
    char pattern[RB_CORPUS_AGGREGATOR_PATH_MAX];
#else
    DIR* directory;
    struct dirent* entry;
#endif

    if (context == NULL ||
        context->output_path == NULL ||
        context->output_path[0] == '\0')
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    if (!rb_ca_build_path(
            context->output_path,
            RB_CORPUS_AGGREGATOR_OUTPUT_NAME,
            output_path,
            sizeof(output_path)
        ) ||
        !rb_ca_build_path(
            context->output_path,
            RB_CORPUS_AGGREGATOR_TEMP_NAME,
            temp_path,
            sizeof(temp_path)
        ))
    {
        return RB_MODULE_ERR_EXECUTION;
    }

    printf(
        "[CORPUS_AGGREGATOR] Stage 400\n"
    );

    printf(
        "[CORPUS_AGGREGATOR][AUTH] Policy index: %s\n",
        RB_CORPUS_AGGREGATOR_POLICY_INDEX_PATH
    );

    printf(
        "[CORPUS_AGGREGATOR][AUTH] Chainlog: %s\n",
        RB_CORPUS_AGGREGATOR_POLICY_CHAINLOG_PATH
    );

    if (!rb_ca_read_file(
            RB_CORPUS_AGGREGATOR_POLICY_INDEX_PATH,
            &index_data,
            &index_length
        ) ||
        !rb_ca_read_file(
            RB_CORPUS_AGGREGATOR_POLICY_CHAINLOG_PATH,
            &chainlog_data,
            &chainlog_length
        ))
    {
        fprintf(
            stderr,
            "[CORPUS_AGGREGATOR][AUTH] Authority records unavailable - FAIL CLOSED\n"
        );

        free(index_data);
        free(chainlog_data);

        return RB_MODULE_ERR_EXECUTION;
    }

    index_records =
        (rb_ca_index_record_t*)calloc(
            RB_CORPUS_AGGREGATOR_MAX_INDEX,
            sizeof(*index_records)
        );

    corpora =
        (rb_ca_corpus_t*)calloc(
            RB_CORPUS_AGGREGATOR_MAX_CORPORA,
            sizeof(*corpora)
        );

    selected =
        (rb_ca_corpus_t*)calloc(
            RB_CORPUS_AGGREGATOR_MAX_CORPORA,
            sizeof(*selected)
        );

    if (index_records == NULL ||
        corpora == NULL ||
        selected == NULL ||
        !rb_ca_parse_index(
            index_data,
            index_length,
            index_records,
            RB_CORPUS_AGGREGATOR_MAX_INDEX,
            &index_count
        ))
    {
        fprintf(
            stderr,
            "[CORPUS_AGGREGATOR][AUTH] Policy index parse FAIL - FAIL CLOSED\n"
        );

        free(index_records);
        free(corpora);
        free(selected);
        free(index_data);
        free(chainlog_data);

        return RB_MODULE_ERR_EXECUTION;
    }

    free(index_data);
    index_data = NULL;

    printf(
        "[CORPUS_AGGREGATOR][AUTH] Index records: %llu\n",
        (unsigned long long)index_count
    );

    printf(
        "[CORPUS_AGGREGATOR] Scan directory: %s\n",
        context->output_path
    );

#ifdef _WIN32
    if (!rb_ca_build_path(
            context->output_path,
            "*",
            pattern,
            sizeof(pattern)
        ))
    {
        free(index_records);
        free(corpora);
        free(selected);
        free(chainlog_data);
        return RB_MODULE_ERR_EXECUTION;
    }

    search = FindFirstFileA(pattern, &find_data);

    if (search == INVALID_HANDLE_VALUE)
    {
        free(index_records);
        free(corpora);
        free(selected);
        free(chainlog_data);
        return RB_MODULE_ERR_EXECUTION;
    }

    do
    {
        char path[RB_CORPUS_AGGREGATOR_PATH_MAX];
        char* data = NULL;
        size_t data_length = 0;
        rb_ca_corpus_t parsed;

        if ((find_data.dwFileAttributes &
             FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            !rb_ca_is_source_corpus_name(
                find_data.cFileName
            ))
        {
            continue;
        }

        if (corpus_count >=
            RB_CORPUS_AGGREGATOR_MAX_CORPORA)
        {
            fprintf(
                stderr,
                "[CORPUS_AGGREGATOR] Corpus inventory limit exceeded - FAIL CLOSED\n"
            );

            FindClose(search);
            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        if (!rb_ca_build_path(
                context->output_path,
                find_data.cFileName,
                path,
                sizeof(path)
            ) ||
            !rb_ca_read_file(
                path,
                &data,
                &data_length
            ) ||
            !rb_ca_parse_corpus(
                find_data.cFileName,
                data,
                data_length,
                &parsed
            ))
        {
            fprintf(
                stderr,
                "[CORPUS_AGGREGATOR] Contract/read FAIL: %s - FAIL CLOSED\n",
                find_data.cFileName
            );

            free(data);
            FindClose(search);
            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        corpora[corpus_count++] = parsed;

        printf(
            "[CORPUS_AGGREGATOR] Discovered: %s root=%s revision=%s records=%llu\n",
            parsed.corpus_filename,
            parsed.root_document_id,
            parsed.revision_id,
            (unsigned long long)parsed.record_count
        );

    } while (FindNextFileA(search, &find_data));

    FindClose(search);
#else
    directory = opendir(context->output_path);

    if (directory == NULL)
    {
        free(index_records);
        free(corpora);
        free(selected);
        free(chainlog_data);
        return RB_MODULE_ERR_EXECUTION;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        char path[RB_CORPUS_AGGREGATOR_PATH_MAX];
        char* data = NULL;
        size_t data_length = 0;
        rb_ca_corpus_t parsed;

        if (!rb_ca_is_source_corpus_name(entry->d_name))
        {
            continue;
        }

        if (corpus_count >= RB_CORPUS_AGGREGATOR_MAX_CORPORA ||
            !rb_ca_build_path(
                context->output_path,
                entry->d_name,
                path,
                sizeof(path)
            ) ||
            !rb_ca_read_file(
                path,
                &data,
                &data_length
            ) ||
            !rb_ca_parse_corpus(
                entry->d_name,
                data,
                data_length,
                &parsed
            ))
        {
            free(data);
            closedir(directory);
            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        corpora[corpus_count++] = parsed;
    }

    closedir(directory);
#endif

    if (corpus_count == 0)
    {
        fprintf(
            stderr,
            "[CORPUS_AGGREGATOR] No source corpus artifacts found - FAIL CLOSED\n"
        );

        free(index_records);
        free(corpora);
        free(selected);
        free(chainlog_data);
        return RB_MODULE_ERR_EXECUTION;
    }

    qsort(
        corpora,
        corpus_count,
        sizeof(corpora[0]),
        rb_ca_corpus_compare
    );

    /*
     * One pass per unique root present in the corpus inventory.
     * The authoritative index selects current. Chainlog must contain the exact
     * current registration before that corpus is admitted to kb.corpus.json.
     */
    for (i = 0; i < corpus_count; )
    {
        const char* root =
            corpora[i].root_document_id;

        rb_ca_index_record_t current;
        size_t root_end = i;
        size_t match_count = 0;
        size_t match_index = 0;

        while (root_end < corpus_count &&
            strcmp(
                corpora[root_end].root_document_id,
                root
            ) == 0)
        {
            root_end++;
        }

        if (!rb_ca_index_current_for_root(
                index_records,
                index_count,
                root,
                &current
            ))
        {
            fprintf(
                stderr,
                "[CORPUS_AGGREGATOR][AUTH] Current revision unresolved root=%s - FAIL CLOSED\n",
                root
            );

            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        if (!rb_ca_chainlog_has_registration(
                chainlog_data,
                chainlog_length,
                &current
            ))
        {
            fprintf(
                stderr,
                "[CORPUS_AGGREGATOR][AUTH] Chain registration missing root=%s revision=%s sha256=%s - FAIL CLOSED\n",
                current.root_document_id,
                current.revision_id,
                current.sha256
            );

            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        printf(
            "[CORPUS_AGGREGATOR][AUTH] Current root=%s revision=%s sha256=%s\n",
            current.root_document_id,
            current.revision_id,
            current.sha256
        );

        for (j = i; j < root_end; j++)
        {
            if (rb_ca_corpus_matches_index(
                    &corpora[j],
                    &current
                ))
            {
                match_index = j;
                match_count++;
            }
            else
            {
                superseded++;

                printf(
                    "[CORPUS_AGGREGATOR][AUTH] Superseded skipped root=%s revision=%s source=%s\n",
                    corpora[j].root_document_id,
                    corpora[j].revision_id,
                    corpora[j].corpus_filename
                );
            }
        }

        if (match_count == 0)
        {
            fprintf(
                stderr,
                "[CORPUS_AGGREGATOR][AUTH] Current corpus missing root=%s revision=%s sha256=%s - FAIL CLOSED\n",
                current.root_document_id,
                current.revision_id,
                current.sha256
            );

            rb_ca_free_corpora(corpora, corpus_count);
            free(index_records);
            free(corpora);
            free(selected);
            free(chainlog_data);
            return RB_MODULE_ERR_EXECUTION;
        }

        if (match_count > 1)
        {
            /*
             * Multiple byte-independent files claiming the exact authoritative
             * tuple are duplicates. Keep the deterministic first file.
             */
            duplicates +=
                match_count - 1;

            printf(
                "[CORPUS_AGGREGATOR][DEDUP] Exact current duplicates skipped root=%s revision=%s count=%llu\n",
                current.root_document_id,
                current.revision_id,
                (unsigned long long)(match_count - 1)
            );

            for (j = i; j < root_end; j++)
            {
                if (j != match_index &&
                    rb_ca_corpus_matches_index(
                        &corpora[j],
                        &current
                    ))
                {
                    free(corpora[j].data);
                    corpora[j].data = NULL;
                }
            }
        }

        selected[selected_count] =
            corpora[match_index];

        corpora[match_index].data = NULL;

        total_records +=
            selected[selected_count].record_count;

        printf(
            "[CORPUS_AGGREGATOR][AUTH] Selected root=%s revision=%s records=%llu source=%s\n",
            selected[selected_count].root_document_id,
            selected[selected_count].revision_id,
            (unsigned long long)selected[selected_count].record_count,
            selected[selected_count].corpus_filename
        );

        selected_count++;
        i = root_end;
    }

    free(chainlog_data);
    chainlog_data = NULL;

    rb_ca_free_corpora(
        corpora,
        corpus_count
    );

    if (selected_count == 0)
    {
        free(index_records);
        free(corpora);
        free(selected);
        return RB_MODULE_ERR_EXECUTION;
    }

    qsort(
        selected,
        selected_count,
        sizeof(selected[0]),
        rb_ca_corpus_compare
    );

    if (!rb_ca_write_aggregate(
            output_path,
            temp_path,
            selected,
            selected_count,
            duplicates,
            &rewritten
        ))
    {
        fprintf(
            stderr,
            "[CORPUS_AGGREGATOR] Aggregate write FAIL - FAIL CLOSED\n"
        );

        rb_ca_free_corpora(selected, selected_count);
        free(index_records);
        free(corpora);
        free(selected);
        return RB_MODULE_ERR_EXECUTION;
    }

    printf(
        "[CORPUS_AGGREGATOR] Source corpora discovered: %llu\n",
        (unsigned long long)corpus_count
    );

    printf(
        "[CORPUS_AGGREGATOR] Current controlled identities included: %llu\n",
        (unsigned long long)selected_count
    );

    printf(
        "[CORPUS_AGGREGATOR] Superseded corpora skipped: %llu\n",
        (unsigned long long)superseded
    );

    printf(
        "[CORPUS_AGGREGATOR] Exact current duplicates skipped: %llu\n",
        (unsigned long long)duplicates
    );

    printf(
        "[CORPUS_AGGREGATOR] Total records: %llu\n",
        (unsigned long long)total_records
    );

    printf(
        "[CORPUS_AGGREGATOR] Aggregate artifact: %s\n",
        output_path
    );

    if (rewritten)
    {
        printf(
            "[CORPUS_AGGREGATOR] Aggregate write: PASS (content changed)\n"
        );
    }
    else
    {
        printf(
            "[CORPUS_AGGREGATOR] Aggregate unchanged: WRITE SKIPPED\n"
        );
    }

    rb_ca_free_corpora(
        selected,
        selected_count
    );

    free(index_records);
    free(corpora);
    free(selected);

    return RB_MODULE_OK;
}

static void rb_ca_shutdown(
    void
)
{
}


static const rb_module_descriptor_t
rb_ca_descriptor =
{
    RB_CORPUS_AGGREGATOR_MODULE_ID,
    RB_CORPUS_AGGREGATOR_MODULE_NAME,

    RB_CORPUS_AGGREGATOR_VERSION_MAJOR,
    RB_CORPUS_AGGREGATOR_VERSION_MINOR,
    RB_CORPUS_AGGREGATOR_VERSION_PATCH,

    RB_MODULE_API_MAJOR,
    RB_MODULE_API_MINOR,

    RB_CORPUS_AGGREGATOR_EXECUTION_STAGE,

    rb_ca_qualify,
    rb_ca_execute,
    rb_ca_shutdown
};


RB_MODULE_EXPORT const rb_module_descriptor_t*
rb_module_get_descriptor(
    void
)
{
    return &rb_ca_descriptor;
}
