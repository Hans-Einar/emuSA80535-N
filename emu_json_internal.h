/* Private shared JSON utilities. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_JSON_INTERNAL_H
#define EMU_JSON_INTERNAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MAX_RECORD_BYTES 65536u
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
    struct json_token tokens[16384];
    int count;
};

struct output
{
    char data[MAX_RECORD_BYTES + 1u];
    size_t length;
    bool failed;
};

bool utf8_valid(const unsigned char *aText, size_t aLength);
bool json_syntax_valid(const char *aText, size_t aLength);
int json_parse(const char *aText, size_t aLength,
                      struct json_token *aTokens, unsigned aCapacity);
int json_next(const struct json_document *aDocument, int aToken);
bool token_raw_equals(const struct json_document *aDocument,
                             int aToken, const char *aText);
bool json_integer(const struct json_document *aDocument, int aToken,
                         int64_t aMinimum, uint64_t aMaximum,
                         int64_t *aValue);
bool json_string_decode(const struct json_document *aDocument,
                               int aToken, char *aOutput, size_t aCapacity,
                               bool aAllowNull, size_t *aLength);
bool json_string(const struct json_document *aDocument, int aToken,
                        char *aOutput, size_t aCapacity);
int json_member(const struct json_document *aDocument, int aObject,
                       const char *aName);
void output_raw(struct output *aOutput, const char *aText);
void output_format(struct output *aOutput, const char *aFormat, ...);
void output_string(struct output *aOutput, const char *aText);
bool json_document_parse(struct json_document *, const char *, size_t, bool strict);
bool json_document_parse_multiline(struct json_document *, const char *, size_t);
bool json_u64(const struct json_document *, int, uint64_t *);
void json_compact(const struct json_document *, int, struct output *);
#endif
