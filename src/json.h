#ifndef JSON_H
#define JSON_H

#include <stdint.h>

typedef enum { JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_OBJECT, JSON_ARRAY } JsonType;

typedef struct JsonValue {
    JsonType type;
    union {
        int   bool_val;
        double num_val;
        char  *str_val;
        struct { struct JsonPair *pairs; int count, cap; } obj;
        struct { struct JsonValue **items; int count, cap; } arr;
    };
} JsonValue;

typedef struct JsonPair { char *key; JsonValue value; } JsonPair;

JsonValue     json_parse(const char *text);
void          json_free(JsonValue *v);
JsonValue    *json_get(JsonValue *obj, const char *key);
JsonValue    *json_arr_get(JsonValue *arr, int idx);
const char   *json_get_str(JsonValue *obj, const char *key, const char *def);
double        json_get_num(JsonValue *obj, const char *key, double def);
int           json_get_bool(JsonValue *obj, const char *key, int def);

#endif
