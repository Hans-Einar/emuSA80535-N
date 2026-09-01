/* emu-debug 1.0 bounded NDJSON server.
 * Copyright 2026 Hans-Einar
 * Licensed under the repository MIT license.
 *
 * The small JSON tokenizer below is repository-owned. It intentionally
 * supports the complete JSON lexical grammar while exposing only the bounded
 * object/array/string/integer access needed by the frozen protocol.
 */

#include "emu_debug.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#ifndef EMU_DEBUG_BUILD_COMMIT
#define EMU_DEBUG_BUILD_COMMIT "unknown"
#endif

#define PROTOCOL_MAJOR 1
#define PROTOCOL_MINOR 0
#define MAX_RECORD_BYTES 65536u
#define MAX_TOKENS 4096u
#define MAX_REQUEST_IDS 8192u
#define REQUEST_ID_SLOTS 16384u
#define MAX_SAFE_INTEGER UINT64_C(9007199254740991)

enum json_type
{
    JSON_UNDEFINED = 0,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_PRIMITIVE
};

struct json_token
{
    enum json_type type;
    int start;
    int end;
    int size;
    int parent;
};

struct json_parser
{
    unsigned position;
    unsigned next_token;
    int parent;
};

struct json_syntax
{
    const char *text;
    size_t length;
    size_t position;
    unsigned depth;
};

struct json_document
{
    const char *text;
    size_t length;
    struct json_token tokens[MAX_TOKENS];
    int count;
};

struct output
{
    char data[MAX_RECORD_BYTES + 1u];
    size_t length;
    bool failed;
};

struct request
{
    uint64_t id;
    char command[64];
    int arguments;
};

struct server
{
    struct em8051_debugger *debugger;
    uint64_t request_ids[REQUEST_ID_SLOTS];
    size_t request_id_count;
    bool first_command;
    bool hello_complete;
    bool terminate;
    bool fatal_after_response;
};

static bool utf8_valid(const unsigned char *aText, size_t aLength)
{
    size_t i = 0;
    while (i < aLength)
    {
        unsigned char first = aText[i++];
        uint32_t codepoint;
        unsigned need;
        if (first < 0x80u)
            continue;
        if (first >= 0xc2u && first <= 0xdfu)
        {
            codepoint = first & 0x1fu;
            need = 1;
        }
        else if (first >= 0xe0u && first <= 0xefu)
        {
            codepoint = first & 0x0fu;
            need = 2;
        }
        else if (first >= 0xf0u && first <= 0xf4u)
        {
            codepoint = first & 0x07u;
            need = 3;
        }
        else
            return false;
        if (i + need > aLength)
            return false;
        while (need-- != 0u)
        {
            unsigned char continuation = aText[i++];
            if ((continuation & 0xc0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6) | (continuation & 0x3fu);
        }
        if ((codepoint < 0x80u) ||
            (codepoint < 0x800u && first >= 0xe0u) ||
            (codepoint < 0x10000u && first >= 0xf0u) ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
            codepoint > 0x10ffffu)
            return false;
    }
    return true;
}

static struct json_token *json_allocate(struct json_parser *aParser,
                                         struct json_token *aTokens,
                                         unsigned aCapacity)
{
    struct json_token *token;
    if (aParser->next_token >= aCapacity)
        return NULL;
    token = &aTokens[aParser->next_token++];
    token->type = JSON_UNDEFINED;
    token->start = -1;
    token->end = -1;
    token->size = 0;
    token->parent = -1;
    return token;
}

static bool json_number(const char *aText, size_t aLength)
{
    size_t i = 0;
    if (i < aLength && aText[i] == '-')
        i++;
    if (i >= aLength)
        return false;
    if (aText[i] == '0')
        i++;
    else
    {
        if (aText[i] < '1' || aText[i] > '9')
            return false;
        do { i++; } while (i < aLength && aText[i] >= '0' && aText[i] <= '9');
    }
    if (i < aLength && aText[i] == '.')
    {
        i++;
        if (i >= aLength || aText[i] < '0' || aText[i] > '9')
            return false;
        do { i++; } while (i < aLength && aText[i] >= '0' && aText[i] <= '9');
    }
    if (i < aLength && (aText[i] == 'e' || aText[i] == 'E'))
    {
        i++;
        if (i < aLength && (aText[i] == '+' || aText[i] == '-'))
            i++;
        if (i >= aLength || aText[i] < '0' || aText[i] > '9')
            return false;
        do { i++; } while (i < aLength && aText[i] >= '0' && aText[i] <= '9');
    }
    return i == aLength;
}

static bool json_primitive_valid(const char *aText, size_t aLength)
{
    return (aLength == 4u && memcmp(aText, "true", 4u) == 0) ||
           (aLength == 5u && memcmp(aText, "false", 5u) == 0) ||
           (aLength == 4u && memcmp(aText, "null", 4u) == 0) ||
           json_number(aText, aLength);
}

static void syntax_space(struct json_syntax *aSyntax)
{
    while (aSyntax->position < aSyntax->length &&
           strchr(" \t\r\n", aSyntax->text[aSyntax->position]))
        aSyntax->position++;
}

static bool syntax_string(struct json_syntax *aSyntax)
{
    if (aSyntax->position >= aSyntax->length ||
        aSyntax->text[aSyntax->position++] != '"')
        return false;
    while (aSyntax->position < aSyntax->length)
    {
        unsigned char value =
            (unsigned char)aSyntax->text[aSyntax->position++];
        if (value == '"')
            return true;
        if (value < 0x20u)
            return false;
        if (value == '\\')
        {
            unsigned hex;
            if (aSyntax->position >= aSyntax->length)
                return false;
            value = (unsigned char)aSyntax->text[aSyntax->position++];
            if (value != 'u')
            {
                if (!strchr("\"\\/bfnrt", (int)value))
                    return false;
                continue;
            }
            for (hex = 0; hex < 4u; hex++)
            {
                unsigned char digit;
                if (aSyntax->position >= aSyntax->length)
                    return false;
                digit = (unsigned char)aSyntax->text[aSyntax->position++];
                if (!((digit >= '0' && digit <= '9') ||
                      (digit >= 'a' && digit <= 'f') ||
                      (digit >= 'A' && digit <= 'F')))
                    return false;
            }
        }
    }
    return false;
}

static bool syntax_value(struct json_syntax *aSyntax);

static bool syntax_array(struct json_syntax *aSyntax)
{
    aSyntax->position++;
    syntax_space(aSyntax);
    if (aSyntax->position < aSyntax->length &&
        aSyntax->text[aSyntax->position] == ']')
    {
        aSyntax->position++;
        return true;
    }
    for (;;)
    {
        if (!syntax_value(aSyntax))
            return false;
        syntax_space(aSyntax);
        if (aSyntax->position >= aSyntax->length)
            return false;
        if (aSyntax->text[aSyntax->position] == ']')
        {
            aSyntax->position++;
            return true;
        }
        if (aSyntax->text[aSyntax->position++] != ',')
            return false;
        syntax_space(aSyntax);
    }
}

static bool syntax_object(struct json_syntax *aSyntax)
{
    aSyntax->position++;
    syntax_space(aSyntax);
    if (aSyntax->position < aSyntax->length &&
        aSyntax->text[aSyntax->position] == '}')
    {
        aSyntax->position++;
        return true;
    }
    for (;;)
    {
        if (!syntax_string(aSyntax))
            return false;
        syntax_space(aSyntax);
        if (aSyntax->position >= aSyntax->length ||
            aSyntax->text[aSyntax->position++] != ':')
            return false;
        syntax_space(aSyntax);
        if (!syntax_value(aSyntax))
            return false;
        syntax_space(aSyntax);
        if (aSyntax->position >= aSyntax->length)
            return false;
        if (aSyntax->text[aSyntax->position] == '}')
        {
            aSyntax->position++;
            return true;
        }
        if (aSyntax->text[aSyntax->position++] != ',')
            return false;
        syntax_space(aSyntax);
    }
}

static bool syntax_value(struct json_syntax *aSyntax)
{
    size_t start;
    bool valid;
    if (aSyntax->position >= aSyntax->length || aSyntax->depth >= 64u)
        return false;
    aSyntax->depth++;
    if (aSyntax->text[aSyntax->position] == '{')
        valid = syntax_object(aSyntax);
    else if (aSyntax->text[aSyntax->position] == '[')
        valid = syntax_array(aSyntax);
    else if (aSyntax->text[aSyntax->position] == '"')
        valid = syntax_string(aSyntax);
    else
    {
        start = aSyntax->position;
        while (aSyntax->position < aSyntax->length &&
               !strchr(" \t\r\n,]}", aSyntax->text[aSyntax->position]))
            aSyntax->position++;
        valid = aSyntax->position != start &&
                json_primitive_valid(aSyntax->text + start,
                                     aSyntax->position - start);
    }
    aSyntax->depth--;
    return valid;
}

static bool json_syntax_valid(const char *aText, size_t aLength)
{
    struct json_syntax syntax;
    syntax.text = aText;
    syntax.length = aLength;
    syntax.position = 0;
    syntax.depth = 0;
    syntax_space(&syntax);
    if (!syntax_value(&syntax))
        return false;
    syntax_space(&syntax);
    return syntax.position == syntax.length;
}

static int json_parse_string(struct json_parser *aParser, const char *aText,
                             size_t aLength, struct json_token *aTokens,
                             unsigned aCapacity)
{
    unsigned start = aParser->position + 1u;
    unsigned i;
    for (i = start; i < aLength; i++)
    {
        unsigned char value = (unsigned char)aText[i];
        if (value == '"')
        {
            struct json_token *token = json_allocate(aParser, aTokens,
                                                     aCapacity);
            if (!token)
                return -2;
            token->type = JSON_STRING;
            token->start = (int)start;
            token->end = (int)i;
            token->parent = aParser->parent;
            if (aParser->parent >= 0)
                aTokens[aParser->parent].size++;
            aParser->position = i;
            return 0;
        }
        if (value < 0x20u)
            return -1;
        if (value == '\\')
        {
            if (++i >= aLength)
                return -1;
            value = (unsigned char)aText[i];
            if (value == 'u')
            {
                unsigned hex;
                for (hex = 0; hex < 4u; hex++)
                {
                    unsigned char digit;
                    if (++i >= aLength)
                        return -1;
                    digit = (unsigned char)aText[i];
                    if (!((digit >= '0' && digit <= '9') ||
                          (digit >= 'a' && digit <= 'f') ||
                          (digit >= 'A' && digit <= 'F')))
                        return -1;
                }
            }
            else if (!strchr("\"\\/bfnrt", (int)value))
                return -1;
        }
    }
    return -1;
}

static int json_parse(const char *aText, size_t aLength,
                      struct json_token *aTokens, unsigned aCapacity)
{
    struct json_parser parser;
    parser.position = 0;
    parser.next_token = 0;
    parser.parent = -1;
    for (; parser.position < aLength; parser.position++)
    {
        char value = aText[parser.position];
        if (value == '{' || value == '[')
        {
            struct json_token *token = json_allocate(&parser, aTokens,
                                                     aCapacity);
            if (!token)
                return -2;
            if (parser.parent >= 0)
                aTokens[parser.parent].size++;
            token->type = value == '{' ? JSON_OBJECT : JSON_ARRAY;
            token->start = (int)parser.position;
            token->parent = parser.parent;
            parser.parent = (int)(parser.next_token - 1u);
        }
        else if (value == '}' || value == ']')
        {
            enum json_type expected = value == '}' ? JSON_OBJECT : JSON_ARRAY;
            int open = parser.parent;
            if (open < 0 || aTokens[open].type != expected)
                return -1;
            aTokens[open].end = (int)parser.position + 1;
            parser.parent = aTokens[open].parent;
        }
        else if (value == '"')
        {
            int result = json_parse_string(&parser, aText, aLength, aTokens,
                                           aCapacity);
            if (result != 0)
                return result;
        }
        else if (value == ':' || value == ',')
        {
            /* Object-key parentage is repaired below after tokenization. */
        }
        else if (value == ' ' || value == '\t' || value == '\r' ||
                 value == '\n')
        {
        }
        else
        {
            unsigned start = parser.position;
            struct json_token *token;
            while (parser.position < aLength &&
                   !strchr(" \t\r\n,]}:", aText[parser.position]))
                parser.position++;
            if (parser.position == start ||
                !json_primitive_valid(aText + start,
                                      parser.position - start))
                return -1;
            token = json_allocate(&parser, aTokens, aCapacity);
            if (!token)
                return -2;
            token->type = JSON_PRIMITIVE;
            token->start = (int)start;
            token->end = (int)parser.position;
            token->parent = parser.parent;
            if (parser.parent >= 0)
                aTokens[parser.parent].size++;
            parser.position--;
        }
    }
    if (parser.parent != -1 || parser.next_token == 0u)
        return -1;
    return (int)parser.next_token;
}

/* The tokenizer records all direct children. Object traversal uses token
 * extents, so it does not depend on whether size counts keys or values. */
static int json_next(const struct json_document *aDocument, int aToken)
{
    int next = aToken + 1;
    int end = aDocument->tokens[aToken].end;
    while (next < aDocument->count &&
           aDocument->tokens[next].start < end)
        next++;
    return next;
}

static bool token_raw_equals(const struct json_document *aDocument,
                             int aToken, const char *aText)
{
    const struct json_token *token = &aDocument->tokens[aToken];
    size_t length = (size_t)(token->end - token->start);
    return token->type == JSON_STRING && strlen(aText) == length &&
           memcmp(aDocument->text + token->start, aText, length) == 0;
}

static int json_member(const struct json_document *aDocument, int aObject,
                       const char *aName)
{
    int cursor;
    int found = -1;
    const struct json_token *object = &aDocument->tokens[aObject];
    if (object->type != JSON_OBJECT)
        return -1;
    cursor = aObject + 1;
    while (cursor < aDocument->count &&
           aDocument->tokens[cursor].start < object->end)
    {
        int value = cursor + 1;
        if (aDocument->tokens[cursor].type != JSON_STRING ||
            value >= aDocument->count ||
            aDocument->tokens[value].start >= object->end)
            return -2;
        if (token_raw_equals(aDocument, cursor, aName))
        {
            if (found >= 0)
                return -2;
            found = value;
        }
        cursor = json_next(aDocument, value);
    }
    return found;
}

static bool json_integer(const struct json_document *aDocument, int aToken,
                         int64_t aMinimum, uint64_t aMaximum,
                         int64_t *aValue)
{
    const struct json_token *token;
    char buffer[32];
    size_t length;
    char *end;
    int64_t value;
    if (aToken < 0 || aToken >= aDocument->count)
        return false;
    token = &aDocument->tokens[aToken];
    length = (size_t)(token->end - token->start);
    if (token->type != JSON_PRIMITIVE || length == 0u ||
        length >= sizeof(buffer) ||
        memchr(aDocument->text + token->start, '.', length) ||
        memchr(aDocument->text + token->start, 'e', length) ||
        memchr(aDocument->text + token->start, 'E', length))
        return false;
    memcpy(buffer, aDocument->text + token->start, length);
    buffer[length] = '\0';
    errno = 0;
    value = strtoll(buffer, &end, 10);
    if (errno != 0 || *end != '\0' || value < aMinimum ||
        (value >= 0 && (uint64_t)value > aMaximum))
        return false;
    *aValue = value;
    return true;
}

static int hex_value(char aDigit)
{
    if (aDigit >= '0' && aDigit <= '9') return aDigit - '0';
    if (aDigit >= 'a' && aDigit <= 'f') return aDigit - 'a' + 10;
    if (aDigit >= 'A' && aDigit <= 'F') return aDigit - 'A' + 10;
    return -1;
}

static bool append_utf8(char *aOutput, size_t aCapacity, size_t *aUsed,
                        uint32_t aCodepoint)
{
    unsigned char encoded[4];
    size_t count;
    if (aCodepoint <= 0x7fu)
    {
        encoded[0] = (unsigned char)aCodepoint; count = 1;
    }
    else if (aCodepoint <= 0x7ffu)
    {
        encoded[0] = (unsigned char)(0xc0u | (aCodepoint >> 6));
        encoded[1] = (unsigned char)(0x80u | (aCodepoint & 0x3fu)); count = 2;
    }
    else if (aCodepoint <= 0xffffu)
    {
        encoded[0] = (unsigned char)(0xe0u | (aCodepoint >> 12));
        encoded[1] = (unsigned char)(0x80u | ((aCodepoint >> 6) & 0x3fu));
        encoded[2] = (unsigned char)(0x80u | (aCodepoint & 0x3fu)); count = 3;
    }
    else if (aCodepoint <= 0x10ffffu)
    {
        encoded[0] = (unsigned char)(0xf0u | (aCodepoint >> 18));
        encoded[1] = (unsigned char)(0x80u | ((aCodepoint >> 12) & 0x3fu));
        encoded[2] = (unsigned char)(0x80u | ((aCodepoint >> 6) & 0x3fu));
        encoded[3] = (unsigned char)(0x80u | (aCodepoint & 0x3fu)); count = 4;
    }
    else return false;
    if (*aUsed + count >= aCapacity)
        return false;
    memcpy(aOutput + *aUsed, encoded, count);
    *aUsed += count;
    return true;
}

static bool json_string(const struct json_document *aDocument, int aToken,
                        char *aOutput, size_t aCapacity)
{
    const struct json_token *token;
    size_t used = 0;
    int i;
    if (aToken < 0 || aToken >= aDocument->count || aCapacity == 0u)
        return false;
    token = &aDocument->tokens[aToken];
    if (token->type != JSON_STRING)
        return false;
    for (i = token->start; i < token->end; i++)
    {
        unsigned char value = (unsigned char)aDocument->text[i];
        if (value != '\\')
        {
            if (used + 1u >= aCapacity)
                return false;
            aOutput[used++] = (char)value;
            continue;
        }
        value = (unsigned char)aDocument->text[++i];
        if (value == 'u')
        {
            uint32_t codepoint = 0;
            unsigned digit;
            for (digit = 0; digit < 4u; digit++)
                codepoint = (codepoint << 4) |
                    (uint32_t)hex_value(aDocument->text[++i]);
            if (codepoint >= 0xd800u && codepoint <= 0xdbffu)
            {
                uint32_t low = 0;
                if (i + 6 >= token->end || aDocument->text[i + 1] != '\\' ||
                    aDocument->text[i + 2] != 'u')
                    return false;
                i += 2;
                for (digit = 0; digit < 4u; digit++)
                    low = (low << 4) |
                        (uint32_t)hex_value(aDocument->text[++i]);
                if (low < 0xdc00u || low > 0xdfffu)
                    return false;
                codepoint = 0x10000u + ((codepoint - 0xd800u) << 10) +
                            (low - 0xdc00u);
            }
            else if (codepoint >= 0xdc00u && codepoint <= 0xdfffu)
                return false;
            if (codepoint == 0u)
                return false;
            if (!append_utf8(aOutput, aCapacity, &used, codepoint))
                return false;
        }
        else
        {
            char decoded;
            switch (value)
            {
            case '"': decoded = '"'; break;
            case '\\': decoded = '\\'; break;
            case '/': decoded = '/'; break;
            case 'b': decoded = '\b'; break;
            case 'f': decoded = '\f'; break;
            case 'n': decoded = '\n'; break;
            case 'r': decoded = '\r'; break;
            case 't': decoded = '\t'; break;
            default: return false;
            }
            if (used + 1u >= aCapacity)
                return false;
            aOutput[used++] = decoded;
        }
    }
    aOutput[used] = '\0';
    return true;
}

static void output_raw(struct output *aOutput, const char *aText)
{
    size_t length = strlen(aText);
    if (aOutput->failed || aOutput->length + length > MAX_RECORD_BYTES)
    {
        aOutput->failed = true;
        return;
    }
    memcpy(aOutput->data + aOutput->length, aText, length);
    aOutput->length += length;
}

static void output_format(struct output *aOutput, const char *aFormat, ...)
{
    va_list arguments;
    int written;
    if (aOutput->failed)
        return;
    va_start(arguments, aFormat);
    written = vsnprintf(aOutput->data + aOutput->length,
                        sizeof(aOutput->data) - aOutput->length,
                        aFormat, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written > MAX_RECORD_BYTES - aOutput->length)
    {
        aOutput->failed = true;
        return;
    }
    aOutput->length += (size_t)written;
}

static void output_string(struct output *aOutput, const char *aText)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *cursor = (const unsigned char *)aText;
    output_raw(aOutput, "\"");
    while (!aOutput->failed && *cursor)
    {
        unsigned char value = *cursor++;
        switch (value)
        {
        case '"': output_raw(aOutput, "\\\""); break;
        case '\\': output_raw(aOutput, "\\\\"); break;
        case '\b': output_raw(aOutput, "\\b"); break;
        case '\f': output_raw(aOutput, "\\f"); break;
        case '\n': output_raw(aOutput, "\\n"); break;
        case '\r': output_raw(aOutput, "\\r"); break;
        case '\t': output_raw(aOutput, "\\t"); break;
        default:
            if (value < 0x20u)
            {
                char escaped[7] = "\\u0000";
                escaped[4] = hex[value >> 4];
                escaped[5] = hex[value & 0x0fu];
                output_raw(aOutput, escaped);
            }
            else
            {
                char byte[2]; byte[0] = (char)value; byte[1] = '\0';
                output_raw(aOutput, byte);
            }
            break;
        }
    }
    output_raw(aOutput, "\"");
}

static void response_prefix(struct output *aOutput, const struct request *aRequest,
                            bool aSuccess)
{
    memset(aOutput, 0, sizeof(*aOutput));
    output_format(aOutput, "{\"type\":\"response\",\"id\":%" PRIu64
                  ",\"command\":", aRequest->id);
    output_string(aOutput, aRequest->command);
    output_raw(aOutput, aSuccess ? ",\"success\":true,\"body\":" :
                                   ",\"success\":false,\"error\":");
}

static void response_error(struct output *aOutput, const struct request *aRequest,
                           const char *aCode, const char *aMessage)
{
    response_prefix(aOutput, aRequest, false);
    output_raw(aOutput, "{\"code\":"); output_string(aOutput, aCode);
    output_raw(aOutput, ",\"message\":"); output_string(aOutput, aMessage);
    output_raw(aOutput, ",\"retryable\":false,\"data\":{}}}");
}

static bool response_write(struct output *aOutput,
                           const struct request *aRequest)
{
    if (aOutput->failed)
        response_error(aOutput, aRequest, "RESPONSE_TOO_LARGE",
                       "response exceeds maxRecordBytes");
    if (aOutput->failed || aOutput->length > MAX_RECORD_BYTES)
        return false;
    if (fwrite(aOutput->data, 1, aOutput->length, stdout) != aOutput->length ||
        fputc('\n', stdout) == EOF || fflush(stdout) != 0)
        return false;
    return true;
}

static void snapshot_json(struct output *aOutput,
                          const struct em8051_debug_snapshot *aSnapshot)
{
    unsigned i;
    output_raw(aOutput, "{\"state\":\"idle\",\"resultKind\":");
    output_string(aOutput,
        aSnapshot->result_kind == EM8051_DEBUG_YIELD ? "yield" :
                                                       "architectural-stop");
    output_raw(aOutput, ",\"reason\":");
    output_string(aOutput, em8051_debug_reason_name(aSnapshot->reason));
    output_format(aOutput, ",\"pc\":%u,\"registers\":{\"a\":%u,\"b\":%u,"
                  "\"psw\":%u,\"sp\":%u,\"dptr\":%u,\"r\":[",
                  (unsigned)aSnapshot->pc, (unsigned)aSnapshot->a,
                  (unsigned)aSnapshot->b, (unsigned)aSnapshot->psw,
                  (unsigned)aSnapshot->sp, (unsigned)aSnapshot->dptr);
    for (i = 0; i < 8u; i++)
    {
        if (i != 0u) output_raw(aOutput, ",");
        output_format(aOutput, "%u", (unsigned)aSnapshot->r[i]);
    }
    output_raw(aOutput, "]},\"variant\":");
    output_string(aOutput, em8051_debug_variant_name(aSnapshot->variant));
    output_format(aOutput, ",\"instructionCount\":%" PRIu64
                  ",\"machineCycleCount\":%" PRIu64,
                  aSnapshot->instruction_count,
                  aSnapshot->machine_cycle_count);
    if (aSnapshot->reason == EM8051_DEBUG_REASON_EXCEPTION)
    {
        output_raw(aOutput, ",\"exception\":{\"code\":");
        output_string(aOutput,
                      em8051_debug_exception_code(aSnapshot->exception_code));
        output_raw(aOutput, ",\"message\":");
        output_string(aOutput,
                      em8051_debug_exception_message(aSnapshot->exception_code));
        output_raw(aOutput, "}");
    }
    output_raw(aOutput, "}");
}

static bool id_insert(struct server *aServer, uint64_t aId)
{
    size_t slot = (size_t)((aId * UINT64_C(11400714819323198485)) >> 50);
    size_t probe;
    for (probe = 0; probe < REQUEST_ID_SLOTS; probe++)
    {
        uint64_t *entry = &aServer->request_ids[(slot + probe) &
                                               (REQUEST_ID_SLOTS - 1u)];
        if (*entry == aId)
            return false;
        if (*entry == 0u)
        {
            if (aServer->request_id_count >= MAX_REQUEST_IDS)
                return false;
            *entry = aId;
            aServer->request_id_count++;
            return true;
        }
    }
    return false;
}

static bool request_parse(const struct json_document *aDocument,
                          struct request *aRequest)
{
    int type, id, command, arguments;
    int64_t numeric_id;
    if (aDocument->tokens[0].type != JSON_OBJECT)
        return false;
    type = json_member(aDocument, 0, "type");
    id = json_member(aDocument, 0, "id");
    command = json_member(aDocument, 0, "command");
    arguments = json_member(aDocument, 0, "arguments");
    if (type < 0 || id < 0 || command < 0 || arguments == -2 ||
        !token_raw_equals(aDocument, type, "request") ||
        !json_integer(aDocument, id, 1, MAX_SAFE_INTEGER, &numeric_id) ||
        !json_string(aDocument, command, aRequest->command,
                     sizeof(aRequest->command)) ||
        aRequest->command[0] == '\0' ||
        (arguments >= 0 && aDocument->tokens[arguments].type != JSON_OBJECT))
        return false;
    aRequest->id = (uint64_t)numeric_id;
    aRequest->arguments = arguments;
    return true;
}

static bool argument_integer(const struct json_document *aDocument,
                             const struct request *aRequest, const char *aName,
                             int64_t aMinimum, uint64_t aMaximum,
                             int64_t *aValue)
{
    int token;
    if (aRequest->arguments < 0)
        return false;
    token = json_member(aDocument, aRequest->arguments, aName);
    return token >= 0 && json_integer(aDocument, token, aMinimum, aMaximum,
                                      aValue);
}

static const char *debug_status_code(enum em8051_debug_status aStatus)
{
    switch (aStatus)
    {
    case EM8051_DEBUG_INVALID_ARGUMENT: return "INVALID_REQUEST";
    case EM8051_DEBUG_INVALID_STATE: return "INVALID_STATE";
    case EM8051_DEBUG_IMAGE_READ: return "IMAGE_READ";
    case EM8051_DEBUG_IMAGE_SIZE: return "IMAGE_SIZE";
    case EM8051_DEBUG_IMAGE_HASH: return "IMAGE_HASH";
    case EM8051_DEBUG_RANGE: return "RANGE";
    case EM8051_DEBUG_BREAKPOINT_LIMIT: return "BREAKPOINT_LIMIT";
    case EM8051_DEBUG_INTERNAL: return "INTERNAL_ERROR";
    default: return "INTERNAL_ERROR";
    }
}

static const char *debug_status_message(enum em8051_debug_status aStatus)
{
    switch (aStatus)
    {
    case EM8051_DEBUG_INVALID_ARGUMENT: return "invalid command arguments";
    case EM8051_DEBUG_INVALID_STATE: return "no valid instruction boundary is available";
    case EM8051_DEBUG_IMAGE_READ: return "raw image could not be read";
    case EM8051_DEBUG_IMAGE_SIZE: return "raw image must be exactly 65536 bytes";
    case EM8051_DEBUG_IMAGE_HASH: return "raw image SHA-256 does not match";
    case EM8051_DEBUG_RANGE: return "requested CODE window leaves uint16 range";
    case EM8051_DEBUG_BREAKPOINT_LIMIT: return "CODE breakpoint limit exceeded";
    default: return "internal emulator error";
    }
}

static bool capability_supported(const char *aCapability)
{
    static const char *capabilities[] =
    {
        "rawCode64k", "deterministicReset", "snapshotBasicRegisters",
        "decodeCode", "replaceCodeBreakpoints", "boundedRun",
        "stepInstruction"
    };
    unsigned i;
    for (i = 0; i < sizeof(capabilities) / sizeof(capabilities[0]); i++)
        if (strcmp(aCapability, capabilities[i]) == 0)
            return true;
    return false;
}

static bool lowercase_sha256(const char *aDigest)
{
    unsigned i;
    if (strlen(aDigest) != 64u)
        return false;
    for (i = 0; i < 64u; i++)
        if (!((aDigest[i] >= '0' && aDigest[i] <= '9') ||
              (aDigest[i] >= 'a' && aDigest[i] <= 'f')))
            return false;
    return true;
}

static void handle_hello(struct server *aServer,
                         const struct json_document *aDocument,
                         const struct request *aRequest,
                         struct output *aOutput)
{
    int protocol, major, minor, required;
    int64_t numeric_major, numeric_minor;
    int cursor;
    if (aRequest->arguments < 0 ||
        (protocol = json_member(aDocument, aRequest->arguments, "protocol")) < 0 ||
        aDocument->tokens[protocol].type != JSON_OBJECT ||
        (major = json_member(aDocument, protocol, "major")) < 0 ||
        (minor = json_member(aDocument, protocol, "minor")) < 0 ||
        !json_integer(aDocument, major, 0, 65535u, &numeric_major) ||
        !json_integer(aDocument, minor, 0, 65535u, &numeric_minor) ||
        (required = json_member(aDocument, aRequest->arguments,
                                "requiredCapabilities")) < 0 ||
        aDocument->tokens[required].type != JSON_ARRAY)
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "invalid hello arguments");
        return;
    }
    if (numeric_major != PROTOCOL_MAJOR)
    {
        response_error(aOutput, aRequest, "UNSUPPORTED_PROTOCOL",
                       "unsupported emu-debug protocol major");
        aServer->fatal_after_response = true;
        return;
    }
    cursor = required + 1;
    while (cursor < aDocument->count &&
           aDocument->tokens[cursor].start < aDocument->tokens[required].end)
    {
        char capability[64];
        if (!json_string(aDocument, cursor, capability, sizeof(capability)))
        {
            response_error(aOutput, aRequest, "INVALID_REQUEST",
                           "requiredCapabilities must contain strings");
            return;
        }
        if (!capability_supported(capability))
        {
            response_error(aOutput, aRequest, "UNSUPPORTED_CAPABILITY",
                           "unsupported required capability");
            return;
        }
        cursor = json_next(aDocument, cursor);
    }
    aServer->hello_complete = true;
    response_prefix(aOutput, aRequest, true);
    output_raw(aOutput,
        "{\"protocol\":{\"major\":1,\"minor\":0},"
        "\"product\":\"emuSA80535-N\","
        "\"productVersion\":\"1.0.0\",\"commit\":");
    output_string(aOutput, EMU_DEBUG_BUILD_COMMIT);
    output_raw(aOutput,
        ",\"variants\":[\"sab80535\"],\"capabilities\":["
        "\"rawCode64k\",\"deterministicReset\","
        "\"snapshotBasicRegisters\",\"decodeCode\","
        "\"replaceCodeBreakpoints\",\"boundedRun\","
        "\"stepInstruction\"],\"limits\":{"
        "\"maxBreakpoints\":1024,\"maxRunChunkInstructions\":1000000,"
        "\"maxDisassembleInstructions\":256,"
        "\"maxRecordBytes\":65536}}");
    output_raw(aOutput, "}");
}

static void handle_load(struct server *aServer,
                        const struct json_document *aDocument,
                        const struct request *aRequest,
                        struct output *aOutput)
{
    int path_token, format_token, hash_token;
    const struct json_token *path_json;
    char *path;
    char format[32];
    char expected[65];
    char actual[EM8051_DEBUG_SHA256_HEX_SIZE] = {0};
    enum em8051_debug_status status;
    if (aRequest->arguments < 0 ||
        (path_token = json_member(aDocument, aRequest->arguments, "path")) < 0 ||
        (format_token = json_member(aDocument, aRequest->arguments, "format")) < 0 ||
        (hash_token = json_member(aDocument, aRequest->arguments,
                                  "expectedSha256")) < 0 ||
        !json_string(aDocument, format_token, format, sizeof(format)) ||
        strcmp(format, "raw-code-64k") != 0 ||
        !json_string(aDocument, hash_token, expected, sizeof(expected)) ||
        !lowercase_sha256(expected))
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "invalid raw image load arguments");
        return;
    }
    path_json = &aDocument->tokens[path_token];
    path = (char *)malloc((size_t)(path_json->end - path_json->start) * 3u + 1u);
    if (!path)
    {
        response_error(aOutput, aRequest, "INTERNAL_ERROR",
                       "path allocation failed");
        return;
    }
    if (!json_string(aDocument, path_token, path,
                     (size_t)(path_json->end - path_json->start) * 3u + 1u))
    {
        free(path);
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "image path is not valid UTF-8 JSON text");
        return;
    }
    status = em8051_debugger_load(aServer->debugger, path, expected, actual);
    free(path);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true);
    output_raw(aOutput, "{\"sha256\":"); output_string(aOutput, actual);
    output_raw(aOutput, "}}");
}

static void handle_reset(struct server *aServer,
                         const struct json_document *aDocument,
                         const struct request *aRequest,
                         struct output *aOutput)
{
    int64_t seed, entry;
    struct em8051_debug_snapshot snapshot;
    enum em8051_debug_status status;
    if (!argument_integer(aDocument, aRequest, "seed", 0, UINT32_MAX, &seed) ||
        !argument_integer(aDocument, aRequest, "entryAddress", 0, UINT16_MAX,
                          &entry))
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "reset requires uint32 seed and uint16 entryAddress");
        return;
    }
    status = em8051_debugger_reset(aServer->debugger, (uint32_t)seed,
                                   (uint16_t)entry, &snapshot);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true); snapshot_json(aOutput, &snapshot);
    output_raw(aOutput, "}");
}

static void handle_state(struct server *aServer, const struct request *aRequest,
                         struct output *aOutput)
{
    struct em8051_debug_snapshot snapshot;
    enum em8051_debug_status status =
        em8051_debugger_get_state(aServer->debugger, &snapshot);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true); snapshot_json(aOutput, &snapshot);
    output_raw(aOutput, "}");
}

static void handle_decode(struct server *aServer,
                          const struct json_document *aDocument,
                          const struct request *aRequest,
                          struct output *aOutput)
{
    int64_t reference, byte_offset, instruction_offset, count;
    struct em8051_debug_decoded records[EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS];
    enum em8051_debug_status status;
    size_t i;
    if (!argument_integer(aDocument, aRequest, "reference", 0, UINT16_MAX,
                          &reference) ||
        !argument_integer(aDocument, aRequest, "byteOffset", -65536, 65536,
                          &byte_offset) ||
        !argument_integer(aDocument, aRequest, "instructionOffset", -65536,
                          65536, &instruction_offset) ||
        !argument_integer(aDocument, aRequest, "instructionCount", 1,
                          EM8051_DEBUG_MAX_DECODE_INSTRUCTIONS, &count))
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "invalid decodeCode arguments");
        return;
    }
    status = em8051_debugger_decode_code(aServer->debugger,
        (uint16_t)reference, (int32_t)byte_offset, (int32_t)instruction_offset,
        (size_t)count, records);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true);
    output_raw(aOutput, "{\"instructions\":[");
    for (i = 0; i < (size_t)count; i++)
    {
        if (i != 0u) output_raw(aOutput, ",");
        output_format(aOutput, "{\"address\":%u,\"size\":%u,\"valid\":%s,"
                      "\"text\":", (unsigned)records[i].address,
                      (unsigned)records[i].size,
                      records[i].valid ? "true" : "false");
        output_string(aOutput, records[i].text);
        if (!records[i].valid)
            output_raw(aOutput, ",\"reason\":\"unknown-predecessor\"");
        output_raw(aOutput, "}");
    }
    output_raw(aOutput, "]}}");
}

static void handle_breakpoints(struct server *aServer,
                               const struct json_document *aDocument,
                               const struct request *aRequest,
                               struct output *aOutput)
{
    int addresses_token;
    int cursor;
    uint16_t addresses[EM8051_DEBUG_MAX_BREAKPOINTS];
    size_t count = 0;
    enum em8051_debug_status status;
    if (aRequest->arguments < 0 ||
        (addresses_token = json_member(aDocument, aRequest->arguments,
                                       "addresses")) < 0 ||
        aDocument->tokens[addresses_token].type != JSON_ARRAY)
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "addresses must be an array");
        return;
    }
    cursor = addresses_token + 1;
    while (cursor < aDocument->count &&
           aDocument->tokens[cursor].start <
               aDocument->tokens[addresses_token].end)
    {
        int64_t value;
        if (count >= EM8051_DEBUG_MAX_BREAKPOINTS)
        {
            response_error(aOutput, aRequest, "BREAKPOINT_LIMIT",
                           "at most 1024 CODE breakpoints are supported");
            return;
        }
        if (!json_integer(aDocument, cursor, 0, UINT16_MAX, &value))
        {
            response_error(aOutput, aRequest, "INVALID_REQUEST",
                           "addresses must be unique uint16 values");
            return;
        }
        addresses[count++] = (uint16_t)value;
        cursor = json_next(aDocument, cursor);
    }
    status = em8051_debugger_replace_breakpoints(aServer->debugger, addresses,
                                                  count);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       status == EM8051_DEBUG_INVALID_ARGUMENT ?
                       "addresses must be unique uint16 values" :
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true);
    output_raw(aOutput, "{\"accepted\":[");
    for (cursor = 0; (size_t)cursor < count; cursor++)
    {
        if (cursor != 0) output_raw(aOutput, ",");
        output_format(aOutput, "%u", (unsigned)addresses[cursor]);
    }
    output_raw(aOutput, "],\"rejected\":[],\"limit\":1024}}");
}

static void handle_run(struct server *aServer,
                       const struct json_document *aDocument,
                       const struct request *aRequest,
                       struct output *aOutput)
{
    int64_t maximum;
    struct em8051_debug_snapshot snapshot;
    enum em8051_debug_status status;
    if (!argument_integer(aDocument, aRequest, "maxInstructions", 1,
                          EM8051_DEBUG_MAX_RUN_INSTRUCTIONS, &maximum))
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "run requires a negotiated positive instruction bound");
        return;
    }
    status = em8051_debugger_run(aServer->debugger, (uint32_t)maximum,
                                 &snapshot);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true); snapshot_json(aOutput, &snapshot);
    output_raw(aOutput, "}");
}

static void handle_step(struct server *aServer, const struct request *aRequest,
                        struct output *aOutput)
{
    struct em8051_debug_snapshot snapshot;
    enum em8051_debug_status status =
        em8051_debugger_step(aServer->debugger, &snapshot);
    if (status != EM8051_DEBUG_OK)
    {
        response_error(aOutput, aRequest, debug_status_code(status),
                       debug_status_message(status));
        return;
    }
    response_prefix(aOutput, aRequest, true); snapshot_json(aOutput, &snapshot);
    output_raw(aOutput, "}");
}

static void dispatch(struct server *aServer,
                     const struct json_document *aDocument,
                     const struct request *aRequest,
                     struct output *aOutput)
{
    if (!id_insert(aServer, aRequest->id))
    {
        response_error(aOutput, aRequest, "INVALID_REQUEST",
                       "request id must be positive and unique for the session");
        return;
    }
    if (aServer->first_command && strcmp(aRequest->command, "hello") != 0)
    {
        aServer->first_command = false;
        response_error(aOutput, aRequest, "INVALID_STATE",
                       "hello must be the first command");
        return;
    }
    if (!aServer->first_command && strcmp(aRequest->command, "hello") == 0)
    {
        response_error(aOutput, aRequest, "INVALID_STATE",
                       "hello is accepted exactly once");
        return;
    }
    if (aServer->first_command)
    {
        aServer->first_command = false;
        handle_hello(aServer, aDocument, aRequest, aOutput);
        return;
    }
    if (!aServer->hello_complete)
    {
        response_error(aOutput, aRequest, "INVALID_STATE",
                       "hello did not establish a compatible session");
        return;
    }
    if (strcmp(aRequest->command, "load") == 0)
        handle_load(aServer, aDocument, aRequest, aOutput);
    else if (strcmp(aRequest->command, "reset") == 0)
        handle_reset(aServer, aDocument, aRequest, aOutput);
    else if (strcmp(aRequest->command, "getState") == 0)
        handle_state(aServer, aRequest, aOutput);
    else if (strcmp(aRequest->command, "decodeCode") == 0)
        handle_decode(aServer, aDocument, aRequest, aOutput);
    else if (strcmp(aRequest->command, "replaceCodeBreakpoints") == 0)
        handle_breakpoints(aServer, aDocument, aRequest, aOutput);
    else if (strcmp(aRequest->command, "run") == 0)
        handle_run(aServer, aDocument, aRequest, aOutput);
    else if (strcmp(aRequest->command, "stepInstruction") == 0)
        handle_step(aServer, aRequest, aOutput);
    else if (strcmp(aRequest->command, "terminate") == 0)
    {
        response_prefix(aOutput, aRequest, true);
        output_raw(aOutput, "{\"terminated\":true}}");
        aServer->terminate = true;
    }
    else
        response_error(aOutput, aRequest, "UNSUPPORTED_COMMAND",
                       "unsupported emu-debug command");
}

static int protocol_loop(void)
{
    struct server server;
    char *line = (char *)malloc(MAX_RECORD_BYTES + 1u);
    size_t length = 0;
    int character;
    int result = 0;
    memset(&server, 0, sizeof(server));
    server.first_command = true;
    server.debugger = em8051_debugger_create();
    if (!line || !server.debugger)
    {
        fprintf(stderr, "emu-debug: bounded startup allocation failed\n");
        free(line);
        em8051_debugger_destroy(server.debugger);
        return 70;
    }
    while (!server.terminate && (character = fgetc(stdin)) != EOF)
    {
        if (character != '\n')
        {
            if (length >= MAX_RECORD_BYTES)
            {
                fprintf(stderr, "emu-debug: protocol record exceeds maxRecordBytes\n");
                result = 65;
                break;
            }
            line[length++] = (char)character;
            continue;
        }
        if (length != 0u && line[length - 1u] == '\r')
            length--;
        if (length == 0u ||
            !utf8_valid((const unsigned char *)line, length))
        {
            fprintf(stderr, "emu-debug: empty or malformed UTF-8 record\n");
            result = 65;
            break;
        }
        else
        {
            struct json_document document;
            struct request request;
            struct output output;
            memset(&document, 0, sizeof(document));
            document.text = line;
            document.length = length;
            document.count = json_syntax_valid(line, length) ?
                json_parse(line, length, document.tokens, MAX_TOKENS) : -1;
            if (document.count <= 0 || !request_parse(&document, &request))
            {
                fprintf(stderr, "emu-debug: malformed JSON request record\n");
                result = 65;
                break;
            }
            dispatch(&server, &document, &request, &output);
            if (!response_write(&output, &request))
            {
                fprintf(stderr, "emu-debug: protocol output failure\n");
                result = 70;
                break;
            }
            if (server.fatal_after_response)
            {
                result = 65;
                break;
            }
        }
        length = 0;
    }
    if (result == 0 && !server.terminate)
    {
        if (ferror(stdin))
        {
            fprintf(stderr, "emu-debug: stdin read failure\n");
            result = 74;
        }
        else if (length != 0u)
        {
            fprintf(stderr, "emu-debug: unterminated record at EOF\n");
            result = 65;
        }
    }
    em8051_debugger_destroy(server.debugger);
    free(line);
    return result;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0)
    {
        printf("emuSA80535-N emu-debug 1.0.0 %s\n", EMU_DEBUG_BUILD_COMMIT);
        return 0;
    }
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--headless-debug") != 0))
    {
        fprintf(stderr, "usage: emu-debug [--headless-debug|--version]\n");
        return 64;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 ||
        _setmode(_fileno(stdout), _O_BINARY) == -1 ||
        _setmode(_fileno(stderr), _O_BINARY) == -1)
    {
        fprintf(stderr, "emu-debug: could not set binary stdio mode\n");
        return 74;
    }
#endif
    return protocol_loop();
}
