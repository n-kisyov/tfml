#include "json.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { const char *text; int pos, len; } Parser;

static void json_value_free(JsonValue *v);

static char peek(Parser *p) { return p->pos < p->len ? p->text[p->pos] : 0; }
static char next(Parser *p)  { return p->pos < p->len ? p->text[p->pos++] : 0; }

static void skip_ws(Parser *p) {
    while (p->pos < p->len && (p->text[p->pos] == ' ' || p->text[p->pos] == '\t' ||
           p->text[p->pos] == '\n' || p->text[p->pos] == '\r')) p->pos++;
}

static JsonValue parse_value(Parser *p);

static char *parse_string(Parser *p) {
    if (next(p) != '"') return NULL;
    char buf[4096]; int bi = 0;
    while (p->pos < p->len && bi < 4095) {
        char c = next(p);
        if (c == '"') { buf[bi] = 0; return strdup_safe(buf); }
        if (c == '\\') {
            char ec = next(p);
            switch (ec) {
                case '"': buf[bi++]='"'; break; case '\\': buf[bi++]='\\'; break;
                case '/': buf[bi++]='/'; break; case 'b': buf[bi++]='\b'; break;
                case 'f': buf[bi++]='\f'; break; case 'n': buf[bi++]='\n'; break;
                case 'r': buf[bi++]='\r'; break; case 't': buf[bi++]='\t'; break;
                case 'u': { char h[5]={0}; for(int i=0;i<4;i++)h[i]=next(p);
                           buf[bi++]=(char)strtol(h,NULL,16); break; }
                default: buf[bi++]=ec; break;
            }
        } else buf[bi++]=c;
    }
    buf[bi]=0; return strdup_safe(buf);
}

static JsonValue parse_object(Parser *p) {
    JsonValue v = {.type=JSON_OBJECT};
    next(p); skip_ws(p);
    if (peek(p)=='}') { next(p); return v; }
    while (1) {
        skip_ws(p); char *key = parse_string(p); if (!key) break;
        skip_ws(p); if (next(p)!=':') { free(key); break; }
        skip_ws(p); JsonValue val = parse_value(p);
        int idx = v.obj.count++;
        if (v.obj.count > v.obj.cap) { v.obj.cap=v.obj.cap?v.obj.cap*2:4;
            v.obj.pairs=(JsonPair*)xrealloc(v.obj.pairs,v.obj.cap*sizeof(JsonPair)); }
        v.obj.pairs[idx].key=key; v.obj.pairs[idx].value=val;
        skip_ws(p); char c=next(p); if (c=='}') break; if (c!=',') break;
    }
    return v;
}

static JsonValue parse_array(Parser *p) {
    JsonValue v = {.type=JSON_ARRAY};
    next(p); skip_ws(p);
    if (peek(p)==']') { next(p); return v; }
    while (1) {
        skip_ws(p); JsonValue item = parse_value(p);
        int idx = v.arr.count++;
        if (v.arr.count > v.arr.cap) { v.arr.cap=v.arr.cap?v.arr.cap*2:4;
            v.arr.items=(JsonValue**)xrealloc(v.arr.items,v.arr.cap*sizeof(JsonValue*)); }
        v.arr.items[idx]=(JsonValue*)malloc(sizeof(JsonValue));
        if(v.arr.items[idx]) *v.arr.items[idx]=item;
        skip_ws(p); char c=next(p); if (c==']') break; if (c!=',') break;
    }
    return v;
}

static JsonValue parse_value(Parser *p) {
    JsonValue v = {0};
    skip_ws(p); char c = peek(p);
    if (c=='{') return parse_object(p);
    if (c=='[') return parse_array(p);
    if (c=='"') { v.type=JSON_STRING; v.str_val=parse_string(p); return v; }
    if (c=='t'||c=='f') { v.type=JSON_BOOL;
        if(strncmp(p->text+p->pos,"true",4)==0){v.bool_val=1;p->pos+=4;}
        else if(strncmp(p->text+p->pos,"false",5)==0){v.bool_val=0;p->pos+=5;} return v; }
    if (c=='n') { if(strncmp(p->text+p->pos,"null",4)==0){v.type=JSON_NULL;p->pos+=4;} return v; }
    if ((c>='0'&&c<='9')||c=='-'||c=='+'||c=='.') { char *e; v.type=JSON_NUMBER;
        v.num_val=strtod(p->text+p->pos,&e); p->pos=(int)(e-p->text); return v; }
    return v;
}

JsonValue json_parse(const char *text) { Parser p={text,0,(int)strlen(text)}; return parse_value(&p); }

static void json_value_free(JsonValue *v) {
    if (!v) return;
    switch(v->type) {
    case JSON_STRING: free(v->str_val); break;
    case JSON_OBJECT: for(int i=0;i<v->obj.count;i++){free(v->obj.pairs[i].key);json_value_free(&v->obj.pairs[i].value);}
                      free(v->obj.pairs); break;
    case JSON_ARRAY: for(int i=0;i<v->arr.count;i++){json_value_free(v->arr.items[i]);free(v->arr.items[i]);}
                     free(v->arr.items); break;
    default: break;
    }
    memset(v,0,sizeof(JsonValue));
}

void json_free(JsonValue *v) { json_value_free(v); }

JsonValue *json_get(JsonValue *obj, const char *key) {
    if(!obj||obj->type!=JSON_OBJECT) return NULL;
    for(int i=0;i<obj->obj.count;i++) if(strcmp(obj->obj.pairs[i].key,key)==0) return &obj->obj.pairs[i].value;
    return NULL;
}

JsonValue *json_arr_get(JsonValue *arr, int idx) {
    if(!arr||arr->type!=JSON_ARRAY||idx<0||idx>=arr->arr.count) return NULL;
    return arr->arr.items[idx];
}

const char *json_get_str(JsonValue *obj, const char *key, const char *def) {
    JsonValue *v=json_get(obj,key); return (v&&v->type==JSON_STRING)?v->str_val:def;
}

double json_get_num(JsonValue *obj, const char *key, double def) {
    JsonValue *v=json_get(obj,key); return (v&&v->type==JSON_NUMBER)?v->num_val:def;
}

int json_get_bool(JsonValue *obj, const char *key, int def) {
    JsonValue *v=json_get(obj,key); return (v&&v->type==JSON_BOOL)?v->bool_val:def;
}
