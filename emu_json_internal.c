/* Extracted from emu-debug. Copyright 2026 Hans-Einar. MIT. */
#include "emu_json_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
bool utf8_valid(const unsigned char *aText, size_t aLength)
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

bool json_syntax_valid(const char *aText, size_t aLength)
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

int json_parse(const char *aText, size_t aLength,
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
int json_next(const struct json_document *aDocument, int aToken)
{
    int next = aToken + 1;
    int end = aDocument->tokens[aToken].end;
    while (next < aDocument->count &&
           aDocument->tokens[next].start < end)
        next++;
    return next;
}

bool token_raw_equals(const struct json_document *aDocument,
                             int aToken, const char *aText)
{
    const struct json_token *token = &aDocument->tokens[aToken];
    size_t length = (size_t)(token->end - token->start);
    return token->type == JSON_STRING && strlen(aText) == length &&
           memcmp(aDocument->text + token->start, aText, length) == 0;
}

bool json_integer(const struct json_document *aDocument, int aToken,
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

bool json_string_decode(const struct json_document *aDocument,
                               int aToken, char *aOutput, size_t aCapacity,
                               bool aAllowNull, size_t *aLength)
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
            if (codepoint == 0u && !aAllowNull)
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
    if (aLength)
        *aLength = used;
    return true;
}

bool json_string(const struct json_document *aDocument, int aToken,
                        char *aOutput, size_t aCapacity)
{
    return json_string_decode(aDocument, aToken, aOutput, aCapacity, false,
                              NULL);
}

int json_member(const struct json_document *aDocument, int aObject,
                       const char *aName)
{
    int cursor;
    int found = -1;
    const struct json_token *object;
    if (aObject < 0 || aObject >= aDocument->count) return -1;
    object = &aDocument->tokens[aObject];
    if (object->type != JSON_OBJECT)
        return -1;
    cursor = aObject + 1;
    while (cursor < aDocument->count &&
           aDocument->tokens[cursor].start < object->end)
    {
        const struct json_token *key = &aDocument->tokens[cursor];
        size_t capacity;
        size_t decoded_length;
        char *decoded;
        bool matches;
        int value = cursor + 1;
        if (key->type != JSON_STRING || value >= aDocument->count ||
            aDocument->tokens[value].start >= object->end)
            return -2;
        capacity = (size_t)(key->end - key->start) + 1u;
        decoded = (char *)malloc(capacity);
        if (!decoded)
            return -2;
        if (!json_string_decode(aDocument, cursor, decoded, capacity, true,
                                &decoded_length))
        {
            free(decoded);
            return -2;
        }
        matches = decoded_length == strlen(aName) &&
                  memcmp(decoded, aName, decoded_length) == 0;
        free(decoded);
        if (matches)
        {
            if (found >= 0)
                return -2;
            found = value;
        }
        cursor = json_next(aDocument, value);
    }
    return found;
}

void output_raw(struct output *aOutput, const char *aText)
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

void output_format(struct output *aOutput, const char *aFormat, ...)
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

void output_string(struct output *aOutput, const char *aText)
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


static bool parse_document(struct json_document *d, const char *text, size_t length, bool strict, bool multiline)
{
    int i;
    char *decoded;
    if (!d || !text || !length || length > 65535 || memchr(text, 0, length) ||
        !utf8_valid((const unsigned char *)text, length) || !json_syntax_valid(text, length)) return false;
    d->text = text; d->length = length;
    d->count = json_parse(text, length, d->tokens, strict ? 16384 : 4096);
    if (d->count < 1 || d->tokens[0].type != JSON_OBJECT) return false;
    if (!strict) return true;
    if (!multiline && (memchr(text, '\r', length) || memchr(text, '\n', length))) return false;
    decoded = malloc(2*(length + 1));
    if (!decoded) return false;
    {
        unsigned values = (unsigned)d->count;
        for (i = 0; i < d->count; ++i) {
            int parent = d->tokens[i].parent, depth = 1;
            while (parent >= 0) { depth++; parent = d->tokens[parent].parent; }
            if (depth > 16 || (d->tokens[i].type == JSON_ARRAY && d->tokens[i].size > 1024) ||
                (d->tokens[i].type == JSON_OBJECT && d->tokens[i].size > 512)) goto invalid;
            /* Keys are tokens but not JSON values. */
            if (d->tokens[i].type == JSON_OBJECT) values -= (unsigned)d->tokens[i].size / 2;
            if (d->tokens[i].type == JSON_STRING && !json_string_decode(d,i,decoded,length+1,true,NULL)) goto invalid;
            if (d->tokens[i].type == JSON_OBJECT) {
                int key = i + 1;
                while (key < d->count && d->tokens[key].start < d->tokens[i].end) {
                    int prior;size_t key_length,prior_length;
                    char *other=decoded+length+1;
                    if (!json_string_decode(d,key,decoded,length+1,true,&key_length)) goto invalid;
                    for(prior=i+1;prior<key;prior=json_next(d,prior+1)) {
                        if(!json_string_decode(d,prior,other,length+1,true,&prior_length))goto invalid;
                        if(key_length==prior_length&&!memcmp(decoded,other,key_length))goto invalid;
                    }
                    key = json_next(d,key+1);
                }
            }
        }
        if (values > 8192) goto invalid;
    }
    free(decoded); return true;
invalid:
    free(decoded); return false;
}

bool json_u64(const struct json_document *d, int t, uint64_t *value)
{
    char text[21]; size_t i; uint64_t n = 0;
    if (!json_string(d,t,text,sizeof text) || !text[0] || (text[0]=='0' && text[1])) return false;
    for (i=0;text[i];i++) {
        unsigned digit=(unsigned)(text[i]-'0');
        if (digit>9 || n>(UINT64_MAX-digit)/10) return false;
        n=n*10+digit;
    }
    *value=n; return true;
}

void json_compact(const struct json_document *d, int t, struct output *o)
{
    int i, end; bool in_string=false, escaped=false;
    if (t<0 || t>=d->count) {o->failed=true; return;}
    i=d->tokens[t].start; end=d->tokens[t].end;
    if (d->tokens[t].type==JSON_STRING) {i--;end++;}
    for (;i<end;i++) {
        char c=d->text[i], one[2]={c,0};
        if (in_string || !strchr(" \t\r\n",c)) output_raw(o,one);
        if (in_string && escaped) {escaped=false;continue;}
        if (in_string && c=='\\') escaped=true;
        else if(c=='"') in_string=!in_string;
    }
}

bool json_document_parse(struct json_document *d, const char *text, size_t length, bool strict)
{
    return parse_document(d,text,length,strict,false);
}
bool json_document_parse_multiline(struct json_document *d, const char *text, size_t length)
{
    return parse_document(d,text,length,true,true);
}
