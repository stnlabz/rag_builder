#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdint.h>
#include <stdarg.h>

#include "provenance.h"


#define RB_PROV_INPUT_SUFFIX      ".chunks.json"
#define RB_PROV_OUTPUT_SUFFIX     ".corpus.json"

#define RB_PROV_CHUNK_CONTRACT    "RB-RETRIEVAL-CHUNKS"
#define RB_PROV_CHUNK_VERSION     1


#define RB_PROV_LOG_FILENAME      "rb_provenance.log"

static FILE* g_rb_prov_log = NULL;

static void rb_prov_log_close_file(
    void
)
{
    if (g_rb_prov_log != NULL)
    {
        fflush(g_rb_prov_log);
        fclose(g_rb_prov_log);
        g_rb_prov_log = NULL;
    }
}


static int rb_prov_log_open_file(
    const char* directory
)
{
    char path[
        RB_PROVENANCE_PATH_MAX
    ];

    int written;

    rb_prov_log_close_file();

    if (directory == NULL ||
        directory[0] == '\0')
    {
        return 0;
    }

    written =
        snprintf(
            path,
            sizeof(path),
            "%s\\%s",
            directory,
            RB_PROV_LOG_FILENAME
        );

    if (written < 0 ||
        (size_t)written >=
            sizeof(path))
    {
        return 0;
    }

    if (fopen_s(
            &g_rb_prov_log,
            path,
            "ab"
        ) != 0 ||
        g_rb_prov_log == NULL)
    {
        g_rb_prov_log = NULL;
        return 0;
    }

    return 1;
}


static void rb_prov_log_timestamp(
    char* buffer,
    size_t buffer_size
)
{
    SYSTEMTIME now;

    if (buffer == NULL ||
        buffer_size == 0)
    {
        return;
    }

    GetLocalTime(
        &now
    );

    (void)snprintf(
        buffer,
        buffer_size,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds
    );
}


static int rb_prov_log_v(
    FILE* terminal,
    const char* format,
    va_list arguments
)
{
    int terminal_result = 0;

    va_list log_arguments;

    if (format == NULL)
    {
        return -1;
    }

    va_copy(
        log_arguments,
        arguments
    );

    if (terminal != NULL)
    {
        terminal_result =
            vfprintf(
                terminal,
                format,
                arguments
            );

        fflush(
            terminal
        );
    }

    if (g_rb_prov_log != NULL)
    {
        char timestamp[64];

        rb_prov_log_timestamp(
            timestamp,
            sizeof(timestamp)
        );

        fprintf(
            g_rb_prov_log,
            "%s ",
            timestamp
        );

        (void)vfprintf(
            g_rb_prov_log,
            format,
            log_arguments
        );

        if (format[0] != '\0')
        {
            size_t format_length =
                strlen(format);

            if (format_length > 0 &&
                format[format_length - 1] != '\n')
            {
                fputc(
                    '\n',
                    g_rb_prov_log
                );
            }
        }

        fflush(
            g_rb_prov_log
        );
    }

    va_end(
        log_arguments
    );

    return terminal_result;
}


static int rb_prov_log_printf(
    const char* format,
    ...
)
{
    int result;

    va_list arguments;

    va_start(
        arguments,
        format
    );

    result =
        rb_prov_log_v(
            stdout,
            format,
            arguments
        );

    va_end(
        arguments
    );

    return result;
}


static int rb_prov_log_fprintf(
    FILE* stream,
    const char* format,
    ...
)
{
    int result;

    va_list arguments;

    va_start(
        arguments,
        format
    );

    result =
        rb_prov_log_v(
            stream,
            format,
            arguments
        );

    va_end(
        arguments
    );

    return result;
}


typedef enum
{
    RB_PROV_OK = 0,

    RB_PROV_ERR_INVALID_ARGUMENT,
    RB_PROV_ERR_MEMORY,
    RB_PROV_ERR_OPEN,
    RB_PROV_ERR_READ,
    RB_PROV_ERR_WRITE,
    RB_PROV_ERR_JSON,
    RB_PROV_ERR_CONTRACT,
    RB_PROV_ERR_SOURCE,
    RB_PROV_ERR_HASH,
    RB_PROV_ERR_HEADER,
    RB_PROV_ERR_POLICY_INDEX,
    RB_PROV_ERR_POLICY_NOT_FOUND,
    RB_PROV_ERR_POLICY_UNAUTHORIZED,
    RB_PROV_ERR_POLICY_CONTRADICTION,
    RB_PROV_ERR_CHAINLOG,
    RB_PROV_ERR_CHAINLOG_NOT_FOUND,
    RB_PROV_ERR_CHAINLOG_CONTRADICTION

} rb_prov_result_t;


static const char* rb_prov_result_name(
    rb_prov_result_t result
)
{
    switch (result)
    {
    case RB_PROV_OK:
        return "OK";

    case RB_PROV_ERR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";

    case RB_PROV_ERR_MEMORY:
        return "MEMORY";

    case RB_PROV_ERR_OPEN:
        return "OPEN";

    case RB_PROV_ERR_READ:
        return "READ";

    case RB_PROV_ERR_WRITE:
        return "WRITE";

    case RB_PROV_ERR_JSON:
        return "JSON";

    case RB_PROV_ERR_CONTRACT:
        return "CONTRACT";

    case RB_PROV_ERR_SOURCE:
        return "SOURCE";

    case RB_PROV_ERR_HASH:
        return "HASH";

    case RB_PROV_ERR_HEADER:
        return "HEADER";

    case RB_PROV_ERR_POLICY_INDEX:
        return "POLICY_INDEX";

    case RB_PROV_ERR_POLICY_NOT_FOUND:
        return "POLICY_NOT_FOUND";

    case RB_PROV_ERR_POLICY_UNAUTHORIZED:
        return "POLICY_UNAUTHORIZED";

    case RB_PROV_ERR_POLICY_CONTRADICTION:
        return "POLICY_CONTRADICTION";

    case RB_PROV_ERR_CHAINLOG:
        return "CHAINLOG";

    case RB_PROV_ERR_CHAINLOG_NOT_FOUND:
        return "CHAINLOG_NOT_FOUND";

    case RB_PROV_ERR_CHAINLOG_CONTRADICTION:
        return "CHAINLOG_CONTRADICTION";

    default:
        return "UNKNOWN";
    }
}


static void rb_prov_log_flush(
    void
)
{
    fflush(stdout);
    fflush(stderr);
}


typedef struct
{
    char filename[
        RB_PROVENANCE_PATH_MAX
    ];

    size_t size;

    const char* chunks_begin;
    const char* chunks_end;

    size_t chunk_count;

} rb_prov_chunk_document_t;

typedef struct
{
    char root_document_id[
        RB_PROVENANCE_ID_MAX
    ];

    char revision_id[
        RB_PROVENANCE_ID_MAX
    ];

    char previous_revision[
        RB_PROVENANCE_ID_MAX
    ];

    char canonical_sha256[
        RB_PROVENANCE_SHA256_HEX
    ];

    char status[
        RB_PROVENANCE_STATUS_MAX
    ];

} rb_prov_controlled_header_t;

typedef struct
{
    char root_document_id[
        RB_PROVENANCE_ID_MAX
    ];

    char revision_id[
        RB_PROVENANCE_ID_MAX
    ];

    char previous_revision[
        RB_PROVENANCE_ID_MAX
    ];

    char canonical_sha256[
        RB_PROVENANCE_SHA256_HEX
    ];

    char status[
        RB_PROVENANCE_STATUS_MAX
    ];

} rb_prov_policy_record_t;


/*
 * ------------------------------------------------------------
 * Utility
 * ------------------------------------------------------------
 */

static int rb_prov_has_suffix(
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

    value_length =
        strlen(value);

    suffix_length =
        strlen(suffix);

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

static int rb_prov_sha256_valid(
    const char* value
)
{
    size_t index;

    if (value == NULL ||
        strlen(value) != 64)
    {
        return 0;
    }

    for (index = 0;
        index < 64;
        index++)
    {
        if (!isxdigit(
            (unsigned char)value[index]
        ))
        {
            return 0;
        }
    }

    return 1;
}

static const char* rb_prov_skip_ws(
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


/*
 * ------------------------------------------------------------
 * File handling
 * ------------------------------------------------------------
 */

static rb_prov_result_t rb_prov_read_file(
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
        return RB_PROV_ERR_INVALID_ARGUMENT;
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
        return RB_PROV_ERR_OPEN;
    }

    if (fseek(
        file,
        0,
        SEEK_END
    ) != 0)
    {
        fclose(file);

        return RB_PROV_ERR_READ;
    }

    file_size =
        ftell(file);

    if (file_size < 0)
    {
        fclose(file);

        return RB_PROV_ERR_READ;
    }

    if (fseek(
        file,
        0,
        SEEK_SET
    ) != 0)
    {
        fclose(file);

        return RB_PROV_ERR_READ;
    }

    buffer =
        (char*)malloc(
            (size_t)file_size + 1
        );

    if (buffer == NULL)
    {
        fclose(file);

        return RB_PROV_ERR_MEMORY;
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

        return RB_PROV_ERR_READ;
    }

    buffer[read_count] =
        '\0';

    *data =
        buffer;

    *length =
        read_count;

    return RB_PROV_OK;
}


/*
 * ------------------------------------------------------------
 * SHA-256
 * ------------------------------------------------------------
 */

typedef struct
{
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char buffer[64];
    size_t buffer_length;

} rb_prov_sha256_context_t;


static const uint32_t rb_prov_sha256_k[64] =
{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};


static uint32_t rb_prov_sha256_rotr(
    uint32_t value,
    unsigned int bits
)
{
    return
        (value >> bits) |
        (value << (32U - bits));
}


static void rb_prov_sha256_transform(
    rb_prov_sha256_context_t* context,
    const unsigned char block[64]
)
{
    uint32_t words[64];

    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;

    uint32_t s0;
    uint32_t s1;
    uint32_t ch;
    uint32_t maj;
    uint32_t temp1;
    uint32_t temp2;

    size_t index;

    for (index = 0;
        index < 16;
        index++)
    {
        size_t offset;

        offset =
            index * 4;

        words[index] =
            ((uint32_t)block[offset] << 24) |
            ((uint32_t)block[offset + 1] << 16) |
            ((uint32_t)block[offset + 2] << 8) |
            ((uint32_t)block[offset + 3]);
    }

    for (index = 16;
        index < 64;
        index++)
    {
        uint32_t x;
        uint32_t y;

        x =
            words[index - 15];

        y =
            words[index - 2];

        s0 =
            rb_prov_sha256_rotr(x, 7) ^
            rb_prov_sha256_rotr(x, 18) ^
            (x >> 3);

        s1 =
            rb_prov_sha256_rotr(y, 17) ^
            rb_prov_sha256_rotr(y, 19) ^
            (y >> 10);

        words[index] =
            words[index - 16] +
            s0 +
            words[index - 7] +
            s1;
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];

    for (index = 0;
        index < 64;
        index++)
    {
        s1 =
            rb_prov_sha256_rotr(e, 6) ^
            rb_prov_sha256_rotr(e, 11) ^
            rb_prov_sha256_rotr(e, 25);

        ch =
            (e & f) ^
            ((~e) & g);

        temp1 =
            h +
            s1 +
            ch +
            rb_prov_sha256_k[index] +
            words[index];

        s0 =
            rb_prov_sha256_rotr(a, 2) ^
            rb_prov_sha256_rotr(a, 13) ^
            rb_prov_sha256_rotr(a, 22);

        maj =
            (a & b) ^
            (a & c) ^
            (b & c);

        temp2 =
            s0 +
            maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}


static void rb_prov_sha256_init(
    rb_prov_sha256_context_t* context
)
{
    memset(
        context,
        0,
        sizeof(*context)
    );

    context->state[0] = 0x6a09e667U;
    context->state[1] = 0xbb67ae85U;
    context->state[2] = 0x3c6ef372U;
    context->state[3] = 0xa54ff53aU;
    context->state[4] = 0x510e527fU;
    context->state[5] = 0x9b05688cU;
    context->state[6] = 0x1f83d9abU;
    context->state[7] = 0x5be0cd19U;
}


static void rb_prov_sha256_update(
    rb_prov_sha256_context_t* context,
    const unsigned char* data,
    size_t length
)
{
    size_t index = 0;

    context->bit_count +=
        (uint64_t)length * 8ULL;

    while (index < length)
    {
        size_t available;
        size_t copy_length;

        available =
            64 -
            context->buffer_length;

        copy_length =
            length - index;

        if (copy_length >
            available)
        {
            copy_length =
                available;
        }

        memcpy(
            context->buffer +
            context->buffer_length,
            data + index,
            copy_length
        );

        context->buffer_length +=
            copy_length;

        index +=
            copy_length;

        if (context->buffer_length ==
            64)
        {
            rb_prov_sha256_transform(
                context,
                context->buffer
            );

            context->buffer_length =
                0;
        }
    }
}


static void rb_prov_sha256_final(
    rb_prov_sha256_context_t* context,
    unsigned char digest[32]
)
{
    uint64_t bit_count;

    size_t index;

    bit_count =
        context->bit_count;

    context->buffer[
        context->buffer_length++
    ] =
        0x80;

    if (context->buffer_length >
        56)
    {
        while (context->buffer_length <
            64)
        {
            context->buffer[
                context->buffer_length++
            ] =
                0;
        }

        rb_prov_sha256_transform(
            context,
            context->buffer
        );

        context->buffer_length =
            0;
    }

    while (context->buffer_length <
        56)
    {
        context->buffer[
            context->buffer_length++
        ] =
            0;
    }

    for (index = 0;
        index < 8;
        index++)
    {
        context->buffer[
            63 - index
        ] =
            (unsigned char)
            (bit_count & 0xFFU);

        bit_count >>=
            8;
    }

    rb_prov_sha256_transform(
        context,
        context->buffer
    );

    for (index = 0;
        index < 8;
        index++)
    {
        digest[
            index * 4
        ] =
            (unsigned char)
            ((context->state[index] >> 24) &
             0xFFU);

        digest[
            index * 4 + 1
        ] =
            (unsigned char)
            ((context->state[index] >> 16) &
             0xFFU);

        digest[
            index * 4 + 2
        ] =
            (unsigned char)
            ((context->state[index] >> 8) &
             0xFFU);

        digest[
            index * 4 + 3
        ] =
            (unsigned char)
            (context->state[index] &
             0xFFU);
    }
}


static rb_prov_result_t rb_prov_sha256_buffer(
    const unsigned char* data,
    size_t length,
    char output[
        RB_PROVENANCE_SHA256_HEX
    ]
)
{
    rb_prov_sha256_context_t context;

    unsigned char digest[32];

    size_t index;

    static const char hex[] =
        "0123456789abcdef";

    if (data == NULL ||
        output == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    rb_prov_sha256_init(
        &context
    );

    rb_prov_sha256_update(
        &context,
        data,
        length
    );

    rb_prov_sha256_final(
        &context,
        digest
    );

    for (index = 0;
        index < 32;
        index++)
    {
        output[index * 2] =
            hex[
                (digest[index] >> 4) &
                0x0F
            ];

        output[
            index * 2 + 1
        ] =
            hex[
                digest[index] &
                0x0F
            ];
    }

    output[64] =
        '\0';

    return RB_PROV_OK;
}

/*
 * ------------------------------------------------------------
 * JSON span handling
 * ------------------------------------------------------------
 */

static int rb_prov_find_matching(
    const char* start,
    const char* end,
    char opening,
    char closing,
    const char** matched
)
{
    const char* cursor;

    unsigned int depth = 0;

    int in_string = 0;
    int escaped = 0;

    if (start == NULL ||
        end == NULL ||
        matched == NULL ||
        start >= end ||
        *start != opening)
    {
        return 0;
    }

    for (cursor = start;
        cursor < end;
        cursor++)
    {
        char c;

        c =
            *cursor;

        if (in_string)
        {
            if (escaped)
            {
                escaped = 0;

                continue;
            }

            if (c == '\\')
            {
                escaped = 1;

                continue;
            }

            if (c == '"')
            {
                in_string = 0;
            }

            continue;
        }

        if (c == '"')
        {
            in_string = 1;

            continue;
        }

        if (c == opening)
        {
            depth++;

            continue;
        }

        if (c == closing)
        {
            if (depth == 0)
            {
                return 0;
            }

            depth--;

            if (depth == 0)
            {
                *matched =
                    cursor;

                return 1;
            }
        }
    }

    return 0;
}

static int rb_prov_find_key(
    const char* start,
    const char* end,
    const char* key,
    const char** value
)
{
    char pattern[256];

    const char* cursor;

    int written;

    if (start == NULL ||
        end == NULL ||
        key == NULL ||
        value == NULL)
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

    if (written < 0 ||
        (size_t)written >=
        sizeof(pattern))
    {
        return 0;
    }

    cursor =
        start;

    while (cursor < end)
    {
        const char* found;
        const char* after;

        found =
            strstr(
                cursor,
                pattern
            );

        if (found == NULL ||
            found >= end)
        {
            return 0;
        }

        after =
            found +
            strlen(pattern);

        after =
            rb_prov_skip_ws(
                after,
                end
            );

        if (after < end &&
            *after == ':')
        {
            after++;

            after =
                rb_prov_skip_ws(
                    after,
                    end
                );

            if (after < end)
            {
                *value =
                    after;

                return 1;
            }

            return 0;
        }

        cursor =
            found + 1;
    }

    return 0;
}

static int rb_prov_extract_json_string(
    const char* start,
    const char* end,
    const char* key,
    char* output,
    size_t output_size
)
{
    const char* value;
    const char* cursor;

    size_t length = 0;

    int escaped = 0;

    if (!rb_prov_find_key(
        start,
        end,
        key,
        &value
    ))
    {
        return 0;
    }

    if (*value != '"')
    {
        return 0;
    }

    value++;

    for (cursor = value;
        cursor < end;
        cursor++)
    {
        char c;

        c =
            *cursor;

        if (escaped)
        {
            return 0;
        }

        if (c == '\\')
        {
            escaped = 1;

            continue;
        }

        if (c == '"')
        {
            if (length == 0 ||
                length >= output_size)
            {
                return 0;
            }

            memcpy(
                output,
                value,
                length
            );

            output[length] =
                '\0';

            return 1;
        }

        length++;
    }

    return 0;
}

static int rb_prov_extract_json_size(
    const char* start,
    const char* end,
    const char* key,
    size_t* output
)
{
    const char* value;

    size_t result = 0;

    int found = 0;

    if (!rb_prov_find_key(
        start,
        end,
        key,
        &value
    ))
    {
        return 0;
    }

    while (value < end &&
        isdigit(
            (unsigned char)*value
        ))
    {
        unsigned int digit;

        digit =
            (unsigned int)
            (*value - '0');

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

        value++;

        found = 1;
    }

    if (!found)
    {
        return 0;
    }

    *output =
        result;

    return 1;
}


/*
 * ------------------------------------------------------------
 * Chunk contract
 * ------------------------------------------------------------
 */

static rb_prov_result_t rb_prov_parse_chunk_document(
    const char* data,
    size_t length,
    rb_prov_chunk_document_t* document
)
{
    const char* end;

    const char* source_value;
    const char* source_end;

    const char* chunks_value;
    const char* chunks_end;

    char contract[64];

    size_t contract_version;

    const char* cursor;

    size_t count = 0;

    if (data == NULL ||
        document == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    memset(
        document,
        0,
        sizeof(*document)
    );

    end =
        data + length;

    if (!rb_prov_extract_json_string(
            data,
            end,
            "contract",
            contract,
            sizeof(contract)
        ) ||
        strcmp(
            contract,
            RB_PROV_CHUNK_CONTRACT
        ) != 0)
    {
        return RB_PROV_ERR_CONTRACT;
    }

    if (!rb_prov_extract_json_size(
            data,
            end,
            "contract_version",
            &contract_version
        ) ||
        contract_version !=
        RB_PROV_CHUNK_VERSION)
    {
        return RB_PROV_ERR_CONTRACT;
    }

    if (!rb_prov_find_key(
        data,
        end,
        "source",
        &source_value
    ) ||
        *source_value != '{' ||
        !rb_prov_find_matching(
            source_value,
            end,
            '{',
            '}',
            &source_end
        ))
    {
        return RB_PROV_ERR_CONTRACT;
    }

    if (!rb_prov_extract_json_string(
            source_value,
            source_end + 1,
            "filename",
            document->filename,
            sizeof(
                document->filename
            )
        ))
    {
        return RB_PROV_ERR_CONTRACT;
    }

    if (!rb_prov_extract_json_size(
            source_value,
            source_end + 1,
            "size",
            &document->size
        ))
    {
        return RB_PROV_ERR_CONTRACT;
    }

    if (!rb_prov_find_key(
        data,
        end,
        "chunks",
        &chunks_value
    ) ||
        *chunks_value != '[' ||
        !rb_prov_find_matching(
            chunks_value,
            end,
            '[',
            ']',
            &chunks_end
        ))
    {
        return RB_PROV_ERR_CONTRACT;
    }

    cursor =
        chunks_value + 1;

    for (;;)
    {
        const char* object_end;

        cursor =
            rb_prov_skip_ws(
                cursor,
                chunks_end
            );

        if (cursor >= chunks_end)
        {
            break;
        }

        if (*cursor == ',')
        {
            cursor++;

            continue;
        }

        if (*cursor != '{')
        {
            return RB_PROV_ERR_CONTRACT;
        }

        if (!rb_prov_find_matching(
            cursor,
            chunks_end,
            '{',
            '}',
            &object_end
        ))
        {
            return RB_PROV_ERR_CONTRACT;
        }

        count++;

        cursor =
            object_end + 1;
    }

    if (count == 0)
    {
        return RB_PROV_ERR_CONTRACT;
    }

    document->chunks_begin =
        chunks_value + 1;

    document->chunks_end =
        chunks_end;

    document->chunk_count =
        count;

    return RB_PROV_OK;
}


/*
 * ------------------------------------------------------------
 * Controlled document header
 * ------------------------------------------------------------
 */

static int rb_prov_line_value(
    const char* data,
    size_t length,
    const char* prefix,
    char* output,
    size_t output_size
)
{
    const char* cursor;
    const char* end;

    size_t prefix_length;

    if (data == NULL ||
        prefix == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    cursor =
        data;

    end =
        data + length;

    prefix_length =
        strlen(prefix);

    while (cursor < end)
    {
        const char* line_end;

        const char* value_start;
        const char* value_end;

        size_t value_length;

        line_end =
            cursor;

        while (line_end < end &&
            *line_end != '\r' &&
            *line_end != '\n')
        {
            line_end++;
        }

        if ((size_t)(
            line_end - cursor
        ) >= prefix_length &&
            strncmp(
                cursor,
                prefix,
                prefix_length
            ) == 0)
        {
            value_start =
                cursor +
                prefix_length;

            while (value_start <
                line_end &&
                isspace(
                    (unsigned char)
                    *value_start
                ))
            {
                value_start++;
            }

            value_end =
                line_end;

            while (value_end >
                value_start &&
                isspace(
                    (unsigned char)
                    value_end[-1]
                ))
            {
                value_end--;
            }

            value_length =
                (size_t)(
                    value_end -
                    value_start
                );

            if (value_length == 0 ||
                value_length >=
                output_size)
            {
                return 0;
            }

            memcpy(
                output,
                value_start,
                value_length
            );

            output[value_length] =
                '\0';

            return 1;
        }

        cursor =
            line_end;

        while (cursor < end &&
            (*cursor == '\r' ||
             *cursor == '\n'))
        {
            cursor++;
        }
    }

    return 0;
}

static rb_prov_result_t rb_prov_parse_controlled_header(
    const char* data,
    size_t length,
    rb_prov_controlled_header_t* header
)
{
    if (data == NULL ||
        header == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    memset(
        header,
        0,
        sizeof(*header)
    );

    if (!rb_prov_line_value(
        data,
        length,
        "Root Document ID:",
        header->root_document_id,
        sizeof(
            header->root_document_id
        )
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    if (!rb_prov_line_value(
        data,
        length,
        "Revision ID:",
        header->revision_id,
        sizeof(
            header->revision_id
        )
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    if (!rb_prov_line_value(
        data,
        length,
        "Previous Revision:",
        header->previous_revision,
        sizeof(
            header->previous_revision
        )
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    if (!rb_prov_line_value(
        data,
        length,
        "sha256:",
        header->canonical_sha256,
        sizeof(
            header->canonical_sha256
        )
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    if (!rb_prov_sha256_valid(
        header->canonical_sha256
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    if (!rb_prov_line_value(
        data,
        length,
        "**Status:**",
        header->status,
        sizeof(
            header->status
        )
    ))
    {
        return RB_PROV_ERR_HEADER;
    }

    return RB_PROV_OK;
}


/*
 * ------------------------------------------------------------
 * Policy index
 * ------------------------------------------------------------
 */

static rb_prov_result_t rb_prov_parse_policy_record(
    const char* start,
    const char* end,
    rb_prov_policy_record_t* record
)
{
    if (start == NULL ||
        end == NULL ||
        record == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    memset(
        record,
        0,
        sizeof(*record)
    );

    if (!rb_prov_extract_json_string(
        start,
        end,
        "root_document_id",
        record->root_document_id,
        sizeof(
            record->root_document_id
        )
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    if (!rb_prov_extract_json_string(
        start,
        end,
        "revision_id",
        record->revision_id,
        sizeof(
            record->revision_id
        )
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    if (!rb_prov_extract_json_string(
        start,
        end,
        "previous_revision",
        record->previous_revision,
        sizeof(
            record->previous_revision
        )
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    if (!rb_prov_extract_json_string(
        start,
        end,
        "sha256",
        record->canonical_sha256,
        sizeof(
            record->canonical_sha256
        )
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    if (!rb_prov_sha256_valid(
        record->canonical_sha256
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    if (!rb_prov_extract_json_string(
        start,
        end,
        "status",
        record->status,
        sizeof(
            record->status
        )
    ))
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    return RB_PROV_OK;
}

static rb_prov_result_t rb_prov_policy_lookup(
    const char* data,
    size_t length,
    rb_prov_controlled_header_t* header
)
{
    const char* cursor;
    const char* end;

    unsigned int identity_matches = 0;

    if (data == NULL ||
        header == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    cursor =
        data;

    end =
        data + length;

    cursor =
        rb_prov_skip_ws(
            cursor,
            end
        );

    if (cursor >= end ||
        *cursor != '[')
    {
        return RB_PROV_ERR_POLICY_INDEX;
    }

    cursor++;

    for (;;)
    {
        const char* object_end;

        rb_prov_policy_record_t record;

        rb_prov_result_t result;

        cursor =
            rb_prov_skip_ws(
                cursor,
                end
            );

        if (cursor >= end)
        {
            return RB_PROV_ERR_POLICY_INDEX;
        }

        if (*cursor == ']')
        {
            break;
        }

        if (*cursor == ',')
        {
            cursor++;

            continue;
        }

        if (*cursor != '{')
        {
            return RB_PROV_ERR_POLICY_INDEX;
        }

        if (!rb_prov_find_matching(
            cursor,
            end,
            '{',
            '}',
            &object_end
        ))
        {
            return RB_PROV_ERR_POLICY_INDEX;
        }

        result =
            rb_prov_parse_policy_record(
                cursor,
                object_end + 1,
                &record
            );

        if (result !=
            RB_PROV_OK)
        {
            return result;
        }

        if (strcmp(
                record.root_document_id,
                header->root_document_id
            ) == 0 &&
            strcmp(
                record.revision_id,
                header->revision_id
            ) == 0)
        {
            identity_matches++;

            if (identity_matches > 1)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][INDEX] CONTRADICTION: duplicate identity root=%s revision=%s\n",
                    header->root_document_id,
                    header->revision_id
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_POLICY_CONTRADICTION;
            }

            rb_prov_log_printf(
                "[PROVENANCE][INDEX] Candidate root=%s revision=%s previous=%s sha256=%s status=%s\n",
                record.root_document_id,
                record.revision_id,
                record.previous_revision,
                record.canonical_sha256,
                record.status
            );

            rb_prov_log_printf(
                "[PROVENANCE][HEADER] Expected root=%s revision=%s previous=%s sha256=%s status=%s\n",
                header->root_document_id,
                header->revision_id,
                header->previous_revision,
                header->canonical_sha256,
                header->status
            );

            if (strcmp(
                    record.previous_revision,
                    header->previous_revision
                ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][INDEX] Lineage authority override previous_revision header=%s index=%s\n",
                    header->previous_revision,
                    record.previous_revision
                );

                strcpy_s(
                    header->previous_revision,
                    sizeof(header->previous_revision),
                    record.previous_revision
                );
            }

            if (_stricmp(
                    record.canonical_sha256,
                    header->canonical_sha256
                ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][INDEX] CONTRADICTION field=sha256 index=%s header=%s\n",
                    record.canonical_sha256,
                    header->canonical_sha256
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_POLICY_CONTRADICTION;
            }

            if (strcmp(
                record.status,
                "Approved"
            ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][INDEX] UNAUTHORIZED index_status=%s\n",
                    record.status
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_POLICY_UNAUTHORIZED;
            }

            if (strcmp(
                header->status,
                "Approved"
            ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][HEADER] UNAUTHORIZED header_status=%s\n",
                    header->status
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_POLICY_UNAUTHORIZED;
            }
        }

        cursor =
            object_end + 1;
    }

    if (identity_matches != 1)
    {
        rb_prov_log_printf(
            "[PROVENANCE][INDEX] No exact identity match root=%s revision=%s matches=%u\n",
            header->root_document_id,
            header->revision_id,
            identity_matches
        );

        rb_prov_log_flush();

        return RB_PROV_ERR_POLICY_NOT_FOUND;
    }

    rb_prov_log_printf(
        "[PROVENANCE][INDEX] Exact identity and authority match: PASS\n"
    );

    rb_prov_log_flush();

    return RB_PROV_OK;
}


/*
 * ------------------------------------------------------------
 * Chainlog
 * ------------------------------------------------------------
 */

static rb_prov_result_t rb_prov_chainlog_verify(
    const char* data,
    size_t length,
    const rb_prov_controlled_header_t* header
)
{
    const char* cursor;
    const char* end;

    unsigned int matches = 0;

    if (data == NULL ||
        header == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    cursor =
        data;

    end =
        data + length;

    while (cursor < end)
    {
        const char* line_end;

        char line[4096];

        size_t line_length;

        char* context = NULL;
        char* token;

        char* root = NULL;
        char* revision = NULL;
        char* previous = NULL;
        char* hash = NULL;

        line_end =
            cursor;

        while (line_end < end &&
            *line_end != '\r' &&
            *line_end != '\n')
        {
            line_end++;
        }

        line_length =
            (size_t)(
                line_end - cursor
            );

        if (line_length >=
            sizeof(line))
        {
            return RB_PROV_ERR_CHAINLOG;
        }

        memcpy(
            line,
            cursor,
            line_length
        );

        line[line_length] =
            '\0';

        token =
            strtok_s(
                line,
                "|",
                &context
            );

        /*
         * Chain log contract:
         *
         * timestamp|REGISTER|root_document_id|revision_id|
         * previous_revision|sha256|previous_chain_hash|chain_hash
         */
        if (token != NULL)
        {
            token =
                strtok_s(
                    NULL,
                    "|",
                    &context
                );
        }

        if (token != NULL &&
            strcmp(
                token,
                "REGISTER"
            ) == 0)
        {
            root =
                strtok_s(
                    NULL,
                    "|",
                    &context
                );

            revision =
                strtok_s(
                    NULL,
                    "|",
                    &context
                );

            previous =
                strtok_s(
                    NULL,
                    "|",
                    &context
                );

            hash =
                strtok_s(
                    NULL,
                    "|",
                    &context
                );
        }

        if (root != NULL &&
            revision != NULL &&
            previous != NULL &&
            hash != NULL &&
            strcmp(
                root,
                header->root_document_id
            ) == 0 &&
            strcmp(
                revision,
                header->revision_id
            ) == 0)
        {
            matches++;

            rb_prov_log_printf(
                "[PROVENANCE][CHAINLOG] Candidate root=%s revision=%s previous=%s sha256=%s\n",
                root,
                revision,
                previous,
                hash
            );

            if (matches > 1)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][CHAINLOG] CONTRADICTION: duplicate REGISTER root=%s revision=%s\n",
                    header->root_document_id,
                    header->revision_id
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_CHAINLOG_CONTRADICTION;
            }

            if (strcmp(
                    previous,
                    header->previous_revision
                ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][CHAINLOG] CONTRADICTION field=previous_revision chainlog=%s header=%s\n",
                    previous,
                    header->previous_revision
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_CHAINLOG_CONTRADICTION;
            }

            if (_stricmp(
                    hash,
                    header->canonical_sha256
                ) != 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][CHAINLOG] CONTRADICTION field=sha256 chainlog=%s header=%s\n",
                    hash,
                    header->canonical_sha256
                );

                rb_prov_log_flush();

                return
                    RB_PROV_ERR_CHAINLOG_CONTRADICTION;
            }
        }

        cursor =
            line_end;

        while (cursor < end &&
            (*cursor == '\r' ||
             *cursor == '\n'))
        {
            cursor++;
        }
    }

    if (matches != 1)
    {
        rb_prov_log_printf(
            "[PROVENANCE][CHAINLOG] No exact REGISTER match root=%s revision=%s matches=%u\n",
            header->root_document_id,
            header->revision_id,
            matches
        );

        rb_prov_log_flush();

        return RB_PROV_ERR_CHAINLOG_NOT_FOUND;
    }

    rb_prov_log_printf(
        "[PROVENANCE][CHAINLOG] REGISTER identity and authority match: PASS\n"
    );

    rb_prov_log_flush();

    return RB_PROV_OK;
}


/*
 * ------------------------------------------------------------
 * JSON output
 * ------------------------------------------------------------
 */

static int rb_prov_json_string(
    FILE* file,
    const char* value
)
{
    const unsigned char* cursor;

    if (file == NULL ||
        value == NULL)
    {
        return 0;
    }

    if (fputc(
        '"',
        file
    ) == EOF)
    {
        return 0;
    }

    cursor =
        (const unsigned char*)value;

    while (*cursor != '\0')
    {
        switch (*cursor)
        {
        case '"':
            if (fputs(
                "\\\"",
                file
            ) < 0)
            {
                return 0;
            }
            break;

        case '\\':
            if (fputs(
                "\\\\",
                file
            ) < 0)
            {
                return 0;
            }
            break;

        case '\n':
            if (fputs(
                "\\n",
                file
            ) < 0)
            {
                return 0;
            }
            break;

        case '\r':
            if (fputs(
                "\\r",
                file
            ) < 0)
            {
                return 0;
            }
            break;

        case '\t':
            if (fputs(
                "\\t",
                file
            ) < 0)
            {
                return 0;
            }
            break;

        default:
            if (*cursor < 0x20)
            {
                if (fprintf(
                    file,
                    "\\u%04x",
                    (unsigned int)
                    *cursor
                ) < 0)
                {
                    return 0;
                }
            }
            else
            {
                if (fputc(
                    *cursor,
                    file
                ) == EOF)
                {
                    return 0;
                }
            }
            break;
        }

        cursor++;
    }

    return fputc(
        '"',
        file
    ) != EOF;
}

static int rb_prov_file_matches_buffer(
    const char* path,
    const char* data,
    size_t length
)
{
    char* existing = NULL;
    size_t existing_length = 0;
    rb_prov_result_t result;
    int matches;

    if (path == NULL ||
        data == NULL)
    {
        return 0;
    }

    result =
        rb_prov_read_file(
            path,
            &existing,
            &existing_length
        );

    if (result != RB_PROV_OK)
    {
        return 0;
    }

    matches =
        existing_length == length &&
        memcmp(
            existing,
            data,
            length
        ) == 0;

    free(existing);

    return matches;
}


static int rb_prov_write_corpus(
    const char* path,
    const rb_prov_chunk_document_t* chunks,
    const rb_prov_controlled_header_t* header,
    const char* raw_sha256,
    int* rewritten
)
{
    FILE* memory = NULL;
    char* buffer = NULL;
    size_t buffer_length;
    const char* cursor;
    size_t chunk_number = 0;
    int success = 0;

    if (rewritten != NULL)
    {
        *rewritten = 0;
    }

    if (path == NULL ||
        chunks == NULL ||
        header == NULL ||
        raw_sha256 == NULL)
    {
        return 0;
    }

    memory = tmpfile();

    if (memory == NULL)
    {
        return 0;
    }

    if (fputs(
        "{\n"
        "  \"corpus_format\": \"STN-LABZ-RAG-CORPUS\",\n"
        "  \"corpus_format_version\": 1,\n"
        "  \"producer\": {\n"
        "    \"module_id\": \"RB-PROVENANCE\",\n"
        "    \"module_version\": \"0.3.14\"\n"
        "  },\n"
        "  \"source\": {\n"
        "    \"filename\": ",
        memory
    ) < 0 ||
        !rb_prov_json_string(memory, chunks->filename) ||
        fprintf(
            memory,
            ",\n"
            "    \"size\": %llu,\n"
            "    \"raw_sha256\": ",
            (unsigned long long)chunks->size
        ) < 0 ||
        !rb_prov_json_string(memory, raw_sha256) ||
        fputs(
            ",\n"
            "    \"authority\": {\n"
            "      \"type\": \"CHAIN\",\n"
            "      \"root_document_id\": ",
            memory
        ) < 0 ||
        !rb_prov_json_string(memory, header->root_document_id) ||
        fputs(",\n      \"revision_id\": ", memory) < 0 ||
        !rb_prov_json_string(memory, header->revision_id) ||
        fputs(",\n      \"previous_revision\": ", memory) < 0 ||
        !rb_prov_json_string(memory, header->previous_revision) ||
        fputs(",\n      \"canonical_sha256\": ", memory) < 0 ||
        !rb_prov_json_string(memory, header->canonical_sha256) ||
        fputs(",\n      \"status\": ", memory) < 0 ||
        !rb_prov_json_string(memory, header->status) ||
        fputs(
            "\n"
            "    }\n"
            "  },\n"
            "  \"records\": [\n",
            memory
        ) < 0)
    {
        fclose(memory);
        return 0;
    }

    cursor = chunks->chunks_begin;

    while (cursor < chunks->chunks_end)
    {
        const char* object_end;
        size_t object_length;
        size_t index_value;
        char chunk_id[RB_PROVENANCE_ID_MAX * 3];

        cursor =
            rb_prov_skip_ws(
                cursor,
                chunks->chunks_end
            );

        if (cursor >= chunks->chunks_end)
        {
            break;
        }

        if (*cursor == ',')
        {
            cursor++;
            continue;
        }

        if (*cursor != '{' ||
            !rb_prov_find_matching(
                cursor,
                chunks->chunks_end,
                '{',
                '}',
                &object_end
            ) ||
            !rb_prov_extract_json_size(
                cursor,
                object_end + 1,
                "index",
                &index_value
            ))
        {
            fclose(memory);
            return 0;
        }

        chunk_number++;

        if (index_value != chunk_number)
        {
            fclose(memory);
            return 0;
        }

        if (snprintf(
                chunk_id,
                sizeof(chunk_id),
                "%s:%s:%06llu",
                header->root_document_id,
                header->revision_id,
                (unsigned long long)index_value
            ) < 0)
        {
            fclose(memory);
            return 0;
        }

        if (fputs("    {\n      \"chunk_id\": ", memory) < 0 ||
            !rb_prov_json_string(memory, chunk_id) ||
            fputs(",\n      \"chunk\": ", memory) < 0)
        {
            fclose(memory);
            return 0;
        }

        object_length =
            (size_t)(object_end - cursor + 1);

        if (fwrite(
                cursor,
                1,
                object_length,
                memory
            ) != object_length)
        {
            fclose(memory);
            return 0;
        }

        if (chunk_number < chunks->chunk_count)
        {
            if (fputs("\n    },\n", memory) < 0)
            {
                fclose(memory);
                return 0;
            }
        }
        else
        {
            if (fputs("\n    }\n", memory) < 0)
            {
                fclose(memory);
                return 0;
            }
        }

        cursor = object_end + 1;
    }

    if (chunk_number != chunks->chunk_count ||
        fputs("  ]\n}\n", memory) < 0 ||
        fflush(memory) != 0)
    {
        fclose(memory);
        return 0;
    }

    if (fseek(memory, 0, SEEK_END) != 0)
    {
        fclose(memory);
        return 0;
    }

    {
        long size = ftell(memory);

        if (size < 0 ||
            fseek(memory, 0, SEEK_SET) != 0)
        {
            fclose(memory);
            return 0;
        }

        buffer_length = (size_t)size;
    }

    buffer =
        (char*)malloc(
            buffer_length + 1
        );

    if (buffer == NULL)
    {
        fclose(memory);
        return 0;
    }

    if (fread(
            buffer,
            1,
            buffer_length,
            memory
        ) != buffer_length)
    {
        free(buffer);
        fclose(memory);
        return 0;
    }

    buffer[buffer_length] = '\0';
    fclose(memory);
    memory = NULL;

    /*
     * Existing byte-identical corpus artifacts are preserved exactly.
     * No truncate, timestamp churn, or needless rewrite.
     */
    if (rb_prov_file_matches_buffer(
            path,
            buffer,
            buffer_length
        ))
    {
        free(buffer);
        return 1;
    }

    {
        FILE* file = NULL;

        if (fopen_s(
                &file,
                path,
                "wb"
            ) != 0 ||
            file == NULL)
        {
            free(buffer);
            return 0;
        }

        success =
            fwrite(
                buffer,
                1,
                buffer_length,
                file
            ) == buffer_length &&
            fflush(file) == 0 &&
            fclose(file) == 0;

        if (!success && file != NULL)
        {
            /* fclose may already have been attempted above. */
        }
    }

    free(buffer);

    if (success &&
        rewritten != NULL)
    {
        *rewritten = 1;
    }

    return success;
}


/*
 * ------------------------------------------------------------
 * Path construction
 * ------------------------------------------------------------
 */

static int rb_prov_build_path(
    const char* directory,
    const char* filename,
    char* output,
    size_t output_size
)
{
    int written;

    if (directory == NULL ||
        filename == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    written =
        snprintf(
            output,
            output_size,
            "%s\\%s",
            directory,
            filename
        );

    return written >= 0 &&
        (size_t)written <
        output_size;
}

static int rb_prov_build_output_path(
    const char* directory,
    const char* chunk_filename,
    char* output,
    size_t output_size
)
{
    size_t filename_length;
    size_t suffix_length;
    size_t base_length;

    int written;

    if (directory == NULL ||
        chunk_filename == NULL ||
        output == NULL ||
        output_size == 0)
    {
        return 0;
    }

    filename_length =
        strlen(
            chunk_filename
        );

    suffix_length =
        strlen(
            RB_PROV_INPUT_SUFFIX
        );

    if (filename_length <
        suffix_length ||
        !rb_prov_has_suffix(
            chunk_filename,
            RB_PROV_INPUT_SUFFIX
        ))
    {
        return 0;
    }

    base_length =
        filename_length -
        suffix_length;

    written =
        snprintf(
            output,
            output_size,
            "%s\\%.*s%s",
            directory,
            (int)base_length,
            chunk_filename,
            RB_PROV_OUTPUT_SUFFIX
        );

    return written >= 0 &&
        (size_t)written <
        output_size;
}


/*
 * ------------------------------------------------------------
 * Current-run source membership
 * ------------------------------------------------------------
 */

static int rb_prov_current_source_exists(
    const rb_module_execution_context_t* context,
    const char* source_filename
)
{
    char source_path[
        RB_PROVENANCE_PATH_MAX
    ];

    DWORD attributes;

    if (context == NULL ||
        context->source_path == NULL ||
        source_filename == NULL ||
        source_filename[0] == '\0')
    {
        return 0;
    }

    if (!rb_prov_build_path(
        context->source_path,
        source_filename,
        source_path,
        sizeof(source_path)
    ))
    {
        return 0;
    }

    attributes =
        GetFileAttributesA(
            source_path
        );

    if (attributes ==
        INVALID_FILE_ATTRIBUTES)
    {
        return 0;
    }

    return
        (attributes &
         FILE_ATTRIBUTE_DIRECTORY) == 0;
}


/*
 * ------------------------------------------------------------
 * Controlled authority resolution
 * ------------------------------------------------------------
 */

static rb_prov_result_t rb_prov_resolve_controlled(
    const rb_module_execution_context_t* context,
    const rb_prov_chunk_document_t* chunks,
    rb_prov_controlled_header_t* header,
    char raw_sha256[
        RB_PROVENANCE_SHA256_HEX
    ]
)
{
    char input_path[
        RB_PROVENANCE_PATH_MAX
    ];

    char* input_data = NULL;
    size_t input_length = 0;

    char* index_data = NULL;
    size_t index_length = 0;

    char* chainlog_data = NULL;
    size_t chainlog_length = 0;

    rb_prov_result_t result;

    if (context == NULL ||
        chunks == NULL ||
        header == NULL ||
        raw_sha256 == NULL)
    {
        return RB_PROV_ERR_INVALID_ARGUMENT;
    }

    rb_prov_log_printf(
        "[PROVENANCE][AUTH] Begin source=%s\n",
        chunks->filename
    );

    if (!rb_prov_build_path(
            context->source_path,
            chunks->filename,
            input_path,
            sizeof(input_path)
        ))
    {
        return RB_PROV_ERR_SOURCE;
    }

    rb_prov_log_printf(
        "[PROVENANCE][AUTH] Input path: %s\n",
        input_path
    );

    result =
        rb_prov_read_file(
            input_path,
            &input_data,
            &input_length
        );

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][AUTH] Input read FAIL result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return RB_PROV_ERR_SOURCE;
    }

    rb_prov_log_printf(
        "[PROVENANCE][AUTH] Input read PASS bytes=%llu declared=%llu\n",
        (unsigned long long)input_length,
        (unsigned long long)chunks->size
    );

    if (input_length != chunks->size)
    {
        rb_prov_log_printf(
            "[PROVENANCE][AUTH] SOURCE SIZE MISMATCH input=%llu chunks=%llu\n",
            (unsigned long long)input_length,
            (unsigned long long)chunks->size
        );

        free(input_data);
        return RB_PROV_ERR_SOURCE;
    }

    result =
        rb_prov_sha256_buffer(
            (const unsigned char*)input_data,
            input_length,
            raw_sha256
        );

    if (result != RB_PROV_OK)
    {
        free(input_data);
        return result;
    }

    rb_prov_log_printf(
        "[PROVENANCE][AUTH] Raw SHA-256 calculated: %s\n",
        raw_sha256
    );

    result =
        rb_prov_parse_controlled_header(
            input_data,
            input_length,
            header
        );

    free(input_data);
    input_data = NULL;

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][HEADER] Controlled header NOT ESTABLISHED result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return RB_PROV_ERR_POLICY_NOT_FOUND;
    }

    rb_prov_log_printf(
        "[PROVENANCE][HEADER] Parsed root=%s revision=%s previous=%s sha256=%s status=%s\n",
        header->root_document_id,
        header->revision_id,
        header->previous_revision,
        header->canonical_sha256,
        header->status
    );

    /*
     * Controlled authority is established by Chain's persistent records:
     *
     *   1. policy.index.json
     *   2. policy.index.json.chainlog
     *
     * The policy-directory source copy is NOT an authority input here and
     * is deliberately not compared byte-for-byte with the RAG source.
     *
     * Root documents and revisions are separate controlled identities.
     * Exact identity is (root_document_id, revision_id).
     */

    rb_prov_log_printf(
        "[PROVENANCE][INDEX] Loading: %s\n",
        RB_PROVENANCE_POLICY_INDEX_PATH
    );

    result =
        rb_prov_read_file(
            RB_PROVENANCE_POLICY_INDEX_PATH,
            &index_data,
            &index_length
        );

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][INDEX] Read FAIL result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return RB_PROV_ERR_POLICY_INDEX;
    }

    rb_prov_log_printf(
        "[PROVENANCE][INDEX] Read PASS bytes=%llu\n",
        (unsigned long long)index_length
    );

    result =
        rb_prov_policy_lookup(
            index_data,
            index_length,
            header
        );

    free(index_data);
    index_data = NULL;

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][INDEX] Verification FAIL result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return result;
    }

    rb_prov_log_printf(
        "[PROVENANCE][INDEX] Authority record: MATCH\n"
    );

    rb_prov_log_printf(
        "[PROVENANCE][CHAINLOG] Loading: %s\n",
        RB_PROVENANCE_POLICY_CHAINLOG_PATH
    );

    result =
        rb_prov_read_file(
            RB_PROVENANCE_POLICY_CHAINLOG_PATH,
            &chainlog_data,
            &chainlog_length
        );

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][CHAINLOG] Read FAIL result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return RB_PROV_ERR_CHAINLOG;
    }

    rb_prov_log_printf(
        "[PROVENANCE][CHAINLOG] Read PASS bytes=%llu\n",
        (unsigned long long)chainlog_length
    );

    result =
        rb_prov_chainlog_verify(
            chainlog_data,
            chainlog_length,
            header
        );

    free(chainlog_data);
    chainlog_data = NULL;

    if (result != RB_PROV_OK)
    {
        rb_prov_log_printf(
            "[PROVENANCE][CHAINLOG] Verification FAIL result=%d(%s)\n",
            (int)result,
            rb_prov_result_name(result)
        );

        return result;
    }

    rb_prov_log_printf(
        "[PROVENANCE][CHAINLOG] Authority record: MATCH\n"
    );

    rb_prov_log_printf(
        "[PROVENANCE][AUTH] Controlled authority resolution: PASS root=%s revision=%s\n",
        header->root_document_id,
        header->revision_id
    );

    rb_prov_log_flush();

    return RB_PROV_OK;
}

#define RB_PROV_SEEN_MAX 1024

typedef struct
{
    char root_document_id[
        RB_PROVENANCE_ID_MAX
    ];

    char revision_id[
        RB_PROVENANCE_ID_MAX
    ];

    char canonical_sha256[
        RB_PROVENANCE_SHA256_HEX
    ];

} rb_prov_seen_identity_t;


static int rb_prov_identity_check(
    const rb_prov_seen_identity_t* seen,
    size_t seen_count,
    const rb_prov_controlled_header_t* header
)
{
    size_t index;

    if (seen == NULL ||
        header == NULL)
    {
        return -1;
    }

    for (index = 0;
        index < seen_count;
        index++)
    {
        if (strcmp(
                seen[index].root_document_id,
                header->root_document_id
            ) == 0 &&
            strcmp(
                seen[index].revision_id,
                header->revision_id
            ) == 0)
        {
            if (_stricmp(
                    seen[index].canonical_sha256,
                    header->canonical_sha256
                ) == 0)
            {
                return 1;
            }

            return -1;
        }
    }

    return 0;
}


static int rb_prov_identity_add(
    rb_prov_seen_identity_t* seen,
    size_t* seen_count,
    const rb_prov_controlled_header_t* header
)
{
    size_t index;

    if (seen == NULL ||
        seen_count == NULL ||
        header == NULL)
    {
        return 0;
    }

    index =
        *seen_count;

    if (index >=
        RB_PROV_SEEN_MAX)
    {
        return 0;
    }

    strcpy_s(
        seen[index].root_document_id,
        sizeof(
            seen[index].root_document_id
        ),
        header->root_document_id
    );

    strcpy_s(
        seen[index].revision_id,
        sizeof(
            seen[index].revision_id
        ),
        header->revision_id
    );

    strcpy_s(
        seen[index].canonical_sha256,
        sizeof(
            seen[index].canonical_sha256
        ),
        header->canonical_sha256
    );

    *seen_count =
        index + 1;

    return 1;
}



/*
 * ------------------------------------------------------------
 * Qualification
 * ------------------------------------------------------------
 */

static rb_module_result_t rb_prov_qualify(
    rb_module_qualification_result_t* result
)
{
    unsigned int passed = 0;

    if (result == NULL)
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    memset(
        result,
        0,
        sizeof(*result)
    );

    if (rb_prov_sha256_valid(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    ))
    {
        passed++;
    }

    if (!rb_prov_sha256_valid(
        "xyz"
    ))
    {
        passed++;
    }

    if (rb_prov_has_suffix(
        "test.chunks.json",
        ".chunks.json"
    ))
    {
        passed++;
    }

    if (!rb_prov_has_suffix(
        "test.rag.json",
        ".chunks.json"
    ))
    {
        passed++;
    }

    {
        const char text[] =
            "{\"a\":{\"b\":1}}";

        const char* match = NULL;

        if (rb_prov_find_matching(
                text,
                text + strlen(text),
                '{',
                '}',
                &match
            ) &&
            match ==
            text +
            strlen(text) -
            1)
        {
            passed++;
        }
    }

    {
        const char text[] =
            "{\"name\":\"value\"}";

        char value[32];

        if (rb_prov_extract_json_string(
                text,
                text + strlen(text),
                "name",
                value,
                sizeof(value)
            ) &&
            strcmp(
                value,
                "value"
            ) == 0)
        {
            passed++;
        }
    }

    {
        const char text[] =
            "{\"size\":6166}";

        size_t value;

        if (rb_prov_extract_json_size(
                text,
                text + strlen(text),
                "size",
                &value
            ) &&
            value == 6166)
        {
            passed++;
        }
    }

    {
        static const char text[] =
            "{"
            "\"contract\":\"RB-RETRIEVAL-CHUNKS\","
            "\"contract_version\":1,"
            "\"source\":{"
            "\"filename\":\"test.md\","
            "\"size\":10"
            "},"
            "\"chunks\":["
            "{\"index\":1,\"text\":\"x\"}"
            "]"
            "}";

        rb_prov_chunk_document_t document;

        if (rb_prov_parse_chunk_document(
                text,
                strlen(text),
                &document
            ) == RB_PROV_OK &&
            document.chunk_count == 1)
        {
            passed++;
        }
    }

    {
        static const char text[] =
            "{"
            "\"contract\":\"WRONG\","
            "\"contract_version\":1,"
            "\"source\":{"
            "\"filename\":\"test.md\","
            "\"size\":10"
            "},"
            "\"chunks\":["
            "{\"index\":1}"
            "]"
            "}";

        rb_prov_chunk_document_t document;

        if (rb_prov_parse_chunk_document(
                text,
                strlen(text),
                &document
            ) ==
            RB_PROV_ERR_CONTRACT)
        {
            passed++;
        }
    }

    {
        static const char text[] =
            "{"
            "\"contract\":\"RB-RETRIEVAL-CHUNKS\","
            "\"contract_version\":2,"
            "\"source\":{"
            "\"filename\":\"test.md\","
            "\"size\":10"
            "},"
            "\"chunks\":["
            "{\"index\":1}"
            "]"
            "}";

        rb_prov_chunk_document_t document;

        if (rb_prov_parse_chunk_document(
                text,
                strlen(text),
                &document
            ) ==
            RB_PROV_ERR_CONTRACT)
        {
            passed++;
        }
    }

    {
        static const char text[] =
            "Root Document ID: GH-001\n"
            "Revision ID: GH-001.R0\n"
            "Previous Revision: NONE\n"
            "sha256: "
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
            "**Status:** Approved\n";

        rb_prov_controlled_header_t header;

        if (rb_prov_parse_controlled_header(
                text,
                strlen(text),
                &header
            ) == RB_PROV_OK &&
            strcmp(
                header.root_document_id,
                "GH-001"
            ) == 0)
        {
            passed++;
        }
    }

    {
        static const char text[] =
            "Revision ID: GH-001.R0\n";

        rb_prov_controlled_header_t header;

        if (rb_prov_parse_controlled_header(
                text,
                strlen(text),
                &header
            ) ==
            RB_PROV_ERR_HEADER)
        {
            passed++;
        }
    }

    {
        static const char index[] =
            "[{"
            "\"root_document_id\":\"GH-001\","
            "\"revision_id\":\"GH-001.R0\","
            "\"status\":\"Approved\","
            "\"previous_revision\":\"NONE\","
            "\"sha256\":"
            "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
            "}]";

        rb_prov_controlled_header_t header;

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            header.root_document_id,
            sizeof(
                header.root_document_id
            ),
            "GH-001"
        );

        strcpy_s(
            header.revision_id,
            sizeof(
                header.revision_id
            ),
            "GH-001.R0"
        );

        strcpy_s(
            header.previous_revision,
            sizeof(
                header.previous_revision
            ),
            "NONE"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(
                header.canonical_sha256
            ),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.status,
            sizeof(
                header.status
            ),
            "Approved"
        );

        if (rb_prov_policy_lookup(
                index,
                strlen(index),
                &header
            ) == RB_PROV_OK)
        {
            passed++;
        }
    }

    {
        static const char index[] =
            "[{"
            "\"root_document_id\":\"GH-001\","
            "\"revision_id\":\"GH-001.R0\","
            "\"status\":\"Pending\","
            "\"previous_revision\":\"NONE\","
            "\"sha256\":"
            "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
            "}]";

        rb_prov_controlled_header_t header;

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            header.root_document_id,
            sizeof(
                header.root_document_id
            ),
            "GH-001"
        );

        strcpy_s(
            header.revision_id,
            sizeof(
                header.revision_id
            ),
            "GH-001.R0"
        );

        strcpy_s(
            header.previous_revision,
            sizeof(
                header.previous_revision
            ),
            "NONE"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(
                header.canonical_sha256
            ),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.status,
            sizeof(
                header.status
            ),
            "Approved"
        );

        if (rb_prov_policy_lookup(
                index,
                strlen(index),
                &header
            ) ==
            RB_PROV_ERR_POLICY_UNAUTHORIZED)
        {
            passed++;
        }
    }

    {
        static const char chainlog[] =
            "2026-08-18T19:25:06Z|REGISTER|"
            "GH-001|GH-001.R0|NONE|"
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef|"
            "0000000000000000000000000000000000000000000000000000000000000000|"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";

        rb_prov_controlled_header_t header;

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            header.root_document_id,
            sizeof(
                header.root_document_id
            ),
            "GH-001"
        );

        strcpy_s(
            header.revision_id,
            sizeof(
                header.revision_id
            ),
            "GH-001.R0"
        );

        strcpy_s(
            header.previous_revision,
            sizeof(
                header.previous_revision
            ),
            "NONE"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(
                header.canonical_sha256
            ),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        if (rb_prov_chainlog_verify(
                chainlog,
                strlen(chainlog),
                &header
            ) == RB_PROV_OK)
        {
            passed++;
        }
    }

    {
        static const unsigned char value[] =
            "deterministic";

        char first[
            RB_PROVENANCE_SHA256_HEX
        ];

        char second[
            RB_PROVENANCE_SHA256_HEX
        ];

        if (rb_prov_sha256_buffer(
                value,
                sizeof(value) - 1,
                first
            ) == RB_PROV_OK &&
            rb_prov_sha256_buffer(
                value,
                sizeof(value) - 1,
                second
            ) == RB_PROV_OK &&
            strcmp(
                first,
                second
            ) == 0 &&
            strcmp(
                first,
                "0badac3c6df445ad3aea62da1350683923aba37c685978afed96a515d12921a3"
            ) == 0)
        {
            passed++;
        }
    }

    /*
     * 17 - Exact identity + exact hash is a duplicate.
     */
    {
        rb_prov_seen_identity_t seen[1];
        rb_prov_controlled_header_t header;

        memset(
            seen,
            0,
            sizeof(seen)
        );

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            seen[0].root_document_id,
            sizeof(seen[0].root_document_id),
            "DOC"
        );

        strcpy_s(
            seen[0].revision_id,
            sizeof(seen[0].revision_id),
            "DOC.R1"
        );

        strcpy_s(
            seen[0].canonical_sha256,
            sizeof(seen[0].canonical_sha256),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.root_document_id,
            sizeof(header.root_document_id),
            "DOC"
        );

        strcpy_s(
            header.revision_id,
            sizeof(header.revision_id),
            "DOC.R1"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(header.canonical_sha256),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        if (rb_prov_identity_check(
                seen,
                1,
                &header
            ) == 1)
        {
            passed++;
        }
    }

    /*
     * 18 - Same root with a different revision is NOT a duplicate.
     */
    {
        rb_prov_seen_identity_t seen[1];
        rb_prov_controlled_header_t header;

        memset(
            seen,
            0,
            sizeof(seen)
        );

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            seen[0].root_document_id,
            sizeof(seen[0].root_document_id),
            "DOC"
        );

        strcpy_s(
            seen[0].revision_id,
            sizeof(seen[0].revision_id),
            "NONE"
        );

        strcpy_s(
            seen[0].canonical_sha256,
            sizeof(seen[0].canonical_sha256),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.root_document_id,
            sizeof(header.root_document_id),
            "DOC"
        );

        strcpy_s(
            header.revision_id,
            sizeof(header.revision_id),
            "DOC.R1"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(header.canonical_sha256),
            "1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        if (rb_prov_identity_check(
                seen,
                1,
                &header
            ) == 0)
        {
            passed++;
        }
    }

    /*
     * 19 - Same root + revision with a conflicting hash fails closed.
     */
    {
        rb_prov_seen_identity_t seen[1];
        rb_prov_controlled_header_t header;

        memset(
            seen,
            0,
            sizeof(seen)
        );

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            seen[0].root_document_id,
            sizeof(seen[0].root_document_id),
            "DOC"
        );

        strcpy_s(
            seen[0].revision_id,
            sizeof(seen[0].revision_id),
            "DOC.R1"
        );

        strcpy_s(
            seen[0].canonical_sha256,
            sizeof(seen[0].canonical_sha256),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.root_document_id,
            sizeof(header.root_document_id),
            "DOC"
        );

        strcpy_s(
            header.revision_id,
            sizeof(header.revision_id),
            "DOC.R1"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(header.canonical_sha256),
            "1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        if (rb_prov_identity_check(
                seen,
                1,
                &header
            ) == -1)
        {
            passed++;
        }
    }


    /*
     * 20 - Chain/index lineage is authoritative when the source header carries
     * a different Previous Revision value for the same controlled identity.
     */
    {
        static const char index[] =
            "[{"
            "\"root_document_id\":\"DOC\","
            "\"revision_id\":\"DOC.R1\","
            "\"status\":\"Approved\","
            "\"previous_revision\":\"NONE\","
            "\"sha256\":"
            "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
            "}]";

        rb_prov_controlled_header_t header;

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            header.root_document_id,
            sizeof(header.root_document_id),
            "DOC"
        );

        strcpy_s(
            header.revision_id,
            sizeof(header.revision_id),
            "DOC.R1"
        );

        strcpy_s(
            header.previous_revision,
            sizeof(header.previous_revision),
            "DOC"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(header.canonical_sha256),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        strcpy_s(
            header.status,
            sizeof(header.status),
            "Approved"
        );

        if (rb_prov_policy_lookup(
                index,
                strlen(index),
                &header
            ) == RB_PROV_OK &&
            strcmp(
                header.previous_revision,
                "NONE"
            ) == 0)
        {
            passed++;
        }
    }

    result->tests_executed =
        20;

    result->tests_passed =
        passed;

    rb_prov_log_printf(
        "[PROVENANCE] Qualification tests: %u/20\n",
        passed
    );

    result->tests_failed =
        result->tests_executed -
        result->tests_passed;

    result->negative_test_executed =
        1;

    {
        static const char chainlog[] =
            "2026-08-18T19:25:06Z|REGISTER|"
            "GH-001|GH-001.R0|NONE|"
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff|"
            "0000000000000000000000000000000000000000000000000000000000000000|"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";

        rb_prov_controlled_header_t header;

        memset(
            &header,
            0,
            sizeof(header)
        );

        strcpy_s(
            header.root_document_id,
            sizeof(
                header.root_document_id
            ),
            "GH-001"
        );

        strcpy_s(
            header.revision_id,
            sizeof(
                header.revision_id
            ),
            "GH-001.R0"
        );

        strcpy_s(
            header.previous_revision,
            sizeof(
                header.previous_revision
            ),
            "NONE"
        );

        strcpy_s(
            header.canonical_sha256,
            sizeof(
                header.canonical_sha256
            ),
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        );

        if (rb_prov_chainlog_verify(
                chainlog,
                strlen(chainlog),
                &header
            ) ==
            RB_PROV_ERR_CHAINLOG_CONTRADICTION)
        {
            result->negative_test_passed =
                1;
        }
    }

    rb_prov_log_printf(
        "[PROVENANCE] Qualification negative validation: %s\n",
        result->negative_test_passed
            ? "PASS"
            : "FAIL"
    );

    if (result->tests_failed != 0 ||
        !result->negative_test_passed)
    {
        return RB_MODULE_ERR_QUALIFICATION;
    }

    return RB_MODULE_OK;
}



/*
 * ------------------------------------------------------------
 * Execution
 * ------------------------------------------------------------
 */

static rb_module_result_t rb_prov_execute(
    const rb_module_execution_context_t* context
)
{
    WIN32_FIND_DATAA find_data;

    HANDLE search;

    char pattern[
        RB_PROVENANCE_PATH_MAX
    ];

    unsigned int processed = 0;
    unsigned int skipped = 0;
    unsigned int duplicates = 0;
    unsigned int unchanged = 0;
    unsigned int written = 0;

    rb_prov_seen_identity_t seen[
        RB_PROV_SEEN_MAX
    ];

    size_t seen_count = 0;

    memset(
        seen,
        0,
        sizeof(seen)
    );

    if (context == NULL ||
        context->source_path == NULL ||
        context->source_path[0] == '\0' ||
        context->output_path == NULL ||
        context->output_path[0] == '\0')
    {
        return RB_MODULE_ERR_INVALID_ARGUMENT;
    }

    setvbuf(
        stdout,
        NULL,
        _IONBF,
        0
    );

    setvbuf(
        stderr,
        NULL,
        _IONBF,
        0
    );

    if (!rb_prov_log_open_file(
        context->output_path
    ))
    {
        fprintf(
            stderr,
            "[PROVENANCE][LOG] Unable to open %s\\%s\n",
            context->output_path,
            RB_PROV_LOG_FILENAME
        );

        return RB_MODULE_ERR_EXECUTION;
    }

    rb_prov_log_printf(
        "[PROVENANCE][LOG] Persistent log: %s\\%s\n",
        context->output_path,
        RB_PROV_LOG_FILENAME
    );

    rb_prov_log_printf(
        "[PROVENANCE][LOG] Session begin module=%s version=%u.%u.%u\n",
        RB_PROVENANCE_MODULE_ID,
        RB_PROVENANCE_VERSION_MAJOR,
        RB_PROVENANCE_VERSION_MINOR,
        RB_PROVENANCE_VERSION_PATCH
    );

    if (snprintf(
        pattern,
        sizeof(pattern),
        "%s\\*%s",
        context->output_path,
        RB_PROV_INPUT_SUFFIX
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
            rb_prov_log_printf(
                "[PROVENANCE] Chunk artifacts: 0\n"
            );

            return RB_MODULE_OK;
        }

        return RB_MODULE_ERR_EXECUTION;
    }

    do
    {
        char chunk_path[
            RB_PROVENANCE_PATH_MAX
        ];

        char output_path[
            RB_PROVENANCE_PATH_MAX
        ];

        char* chunk_data = NULL;
        size_t chunk_length = 0;

        rb_prov_chunk_document_t chunks;

        rb_prov_controlled_header_t header;

        char raw_sha256[
            RB_PROVENANCE_SHA256_HEX
        ];

        rb_prov_result_t result;

        if (find_data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (!rb_prov_has_suffix(
            find_data.cFileName,
            RB_PROV_INPUT_SUFFIX
        ))
        {
            continue;
        }

        if (!rb_prov_build_path(
            context->output_path,
            find_data.cFileName,
            chunk_path,
            sizeof(chunk_path)
        ))
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        if (!rb_prov_build_output_path(
            context->output_path,
            find_data.cFileName,
            output_path,
            sizeof(output_path)
        ))
        {
            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        rb_prov_log_printf(
            "[PROVENANCE] Chunks: %s\n",
            chunk_path
        );

        result =
            rb_prov_read_file(
                chunk_path,
                &chunk_data,
                &chunk_length
            );

        if (result !=
            RB_PROV_OK)
        {
            rb_prov_log_fprintf(stderr,
                "[PROVENANCE] Chunk read FAIL\n"
            );

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        result =
            rb_prov_parse_chunk_document(
                chunk_data,
                chunk_length,
                &chunks
            );

        if (result !=
            RB_PROV_OK)
        {
            rb_prov_log_fprintf(stderr,
                "[PROVENANCE] Chunk contract FAIL: %d\n",
                (int)result
            );

            free(chunk_data);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        rb_prov_log_printf(
            "[PROVENANCE] Chunk contract PASS: %u record(s)\n",
            (unsigned int)
            chunks.chunk_count
        );

        /*
         * output\ is not the work queue.
         *
         * A chunk artifact participates in this execution only when its
         * declared source document still exists in the current source set.
         * This prevents stale .chunks.json files from prior runs from being
         * reprocessed merely because they remain in the output directory.
         */
        if (!rb_prov_current_source_exists(
            context,
            chunks.filename
        ))
        {
            rb_prov_log_printf(
                "[PROVENANCE] Stale artifact ignored: %s\n",
                find_data.cFileName
            );

            free(chunk_data);

            continue;
        }

        result =
            rb_prov_resolve_controlled(
                context,
                &chunks,
                &header,
                raw_sha256
            );


        if (result ==
            RB_PROV_ERR_POLICY_NOT_FOUND)
        {
            rb_prov_log_printf(
                "[PROVENANCE] Controlled authority: NOT ESTABLISHED\n"
            );

            rb_prov_log_printf(
                "[PROVENANCE] Source skipped: %s\n",
                chunks.filename
            );

            skipped++;

            free(chunk_data);

            continue;
        }

        if (result !=
            RB_PROV_OK)
        {
            rb_prov_log_fprintf(stderr,
                "[PROVENANCE] Controlled authority FAIL: %d(%s) source=%s - FAIL CLOSED\n",
                (int)result,
                rb_prov_result_name(result),
                chunks.filename
            );

            free(chunk_data);

            FindClose(search);

            return RB_MODULE_ERR_EXECUTION;
        }

        {
            int identity_state =
                rb_prov_identity_check(
                    seen,
                    seen_count,
                    &header
                );

            if (identity_state > 0)
            {
                rb_prov_log_printf(
                    "[PROVENANCE][DEDUP] Exact duplicate skipped root=%s revision=%s sha256=%s source=%s\n",
                    header.root_document_id,
                    header.revision_id,
                    header.canonical_sha256,
                    chunks.filename
                );

                duplicates++;

                free(chunk_data);

                continue;
            }

            if (identity_state < 0)
            {
                rb_prov_log_fprintf(
                    stderr,
                    "[PROVENANCE][DEDUP] CONFLICT root=%s revision=%s canonical_sha256=%s - FAIL CLOSED\n",
                    header.root_document_id,
                    header.revision_id,
                    header.canonical_sha256
                );

                free(chunk_data);

                FindClose(search);

                return RB_MODULE_ERR_EXECUTION;
            }

            if (!rb_prov_identity_add(
                seen,
                &seen_count,
                &header
            ))
            {
                rb_prov_log_fprintf(
                    stderr,
                    "[PROVENANCE][DEDUP] Identity table full - FAIL CLOSED\n"
                );

                free(chunk_data);

                FindClose(search);

                return RB_MODULE_ERR_EXECUTION;
            }

            rb_prov_log_printf(
                "[PROVENANCE][DEDUP] Unique identity accepted root=%s revision=%s\n",
                header.root_document_id,
                header.revision_id
            );
        }

        rb_prov_log_printf(
            "[PROVENANCE] Controlled source: AUTHORITY ESTABLISHED\n"
        );

        rb_prov_log_printf(
            "[PROVENANCE] Raw SHA-256: %s\n",
            raw_sha256
        );

        rb_prov_log_printf(
            "[PROVENANCE] Policy index: MATCH\n"
        );

        rb_prov_log_printf(
            "[PROVENANCE] Chain registration: MATCH\n"
        );

        rb_prov_log_printf(
            "[PROVENANCE] Root Document ID: %s\n",
            header.root_document_id
        );

        rb_prov_log_printf(
            "[PROVENANCE] Revision ID: %s\n",
            header.revision_id
        );

        rb_prov_log_printf(
            "[PROVENANCE] Previous Revision: %s\n",
            header.previous_revision
        );

        rb_prov_log_printf(
            "[PROVENANCE] Canonical SHA-256: %s\n",
            header.canonical_sha256
        );

        rb_prov_log_printf(
            "[PROVENANCE] Status: %s\n",
            header.status
        );

        {
            int corpus_rewritten = 0;

            if (!rb_prov_write_corpus(
                output_path,
                &chunks,
                &header,
                raw_sha256,
                &corpus_rewritten
            ))
            {
                rb_prov_log_fprintf(stderr,
                    "[PROVENANCE] Corpus artifact write FAIL: %s\n",
                    output_path
                );

                free(chunk_data);

                FindClose(search);

                return RB_MODULE_ERR_EXECUTION;
            }

            rb_prov_log_printf(
                "[PROVENANCE] Corpus artifact: %s\n",
                output_path
            );

            if (corpus_rewritten)
            {
                written++;

                rb_prov_log_printf(
                    "[PROVENANCE] Artifact write: PASS (content changed)\n"
                );
            }
            else
            {
                unchanged++;

                rb_prov_log_printf(
                    "[PROVENANCE] Artifact unchanged: WRITE SKIPPED\n"
                );
            }
        }

        processed++;

        free(chunk_data);

    } while (
        FindNextFileA(
            search,
            &find_data
        )
    );

    FindClose(search);

    rb_prov_log_printf(
        "[PROVENANCE] Controlled artifacts processed: %u\n",
        processed
    );

    rb_prov_log_printf(
        "[PROVENANCE] Unestablished sources skipped: %u\n",
        skipped
    );

    rb_prov_log_printf(
        "[PROVENANCE] Exact duplicates skipped: %u\n",
        duplicates
    );

    rb_prov_log_printf(
        "[PROVENANCE] Corpus artifacts written: %u\n",
        written
    );

    rb_prov_log_printf(
        "[PROVENANCE] Corpus artifacts unchanged: %u\n",
        unchanged
    );

    rb_prov_log_printf(
        "[PROVENANCE][LOG] Session end result=PASS processed=%u skipped=%u duplicates=%u written=%u unchanged=%u\n",
        processed,
        skipped,
        duplicates,
        written,
        unchanged
    );

    rb_prov_log_close_file();

    return RB_MODULE_OK;
}

static void rb_prov_shutdown(
    void
)
{
    rb_prov_log_close_file();
}


/*
 * ------------------------------------------------------------
 * DLL descriptor
 * ------------------------------------------------------------
 */

static const rb_module_descriptor_t
rb_prov_descriptor =
{
    RB_PROVENANCE_MODULE_ID,
    RB_PROVENANCE_MODULE_NAME,

    RB_PROVENANCE_VERSION_MAJOR,
    RB_PROVENANCE_VERSION_MINOR,
    RB_PROVENANCE_VERSION_PATCH,

    RB_MODULE_API_MAJOR,
    RB_MODULE_API_MINOR,

    RB_PROVENANCE_EXECUTION_STAGE,

    rb_prov_qualify,
    rb_prov_execute,
    rb_prov_shutdown
};

RB_MODULE_EXPORT const rb_module_descriptor_t*
rb_module_get_descriptor(
    void
)
{
    return &rb_prov_descriptor;
}
