#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#include "nc_palloc.h"
#include "nc_array.h"

#define ERR_SYNTAX "Syntax error"
#define ERR_BAD_CONF_FILE "Bad or empty config file"

#define TK_LOG 3
#define TK_LP 4
#define TK_RP 5
#define TK_LISTEN 6
#define TK_STORE 7
#define TK_BUFFER_TYPE 8

#define TOKEN_VALUE_TYPE_NONE 0
#define TOKEN_VALUE_TYPE_INTEGER 1
#define TOKEN_VALUE_TYPE_FLOAT 2
#define TOKEN_VALUE_TYPE_STRING 3

#define MAX_KEY_LEN 64

typedef struct Token {
  int id;
  int value_type;
  size_t len;
  char value[0];
} Token;

typedef struct Value {
  struct nc_array arr;
} Value;

typedef struct KeyValue {
  char key[MAX_KEY_LEN];
  Value *value;
} KeyValue;

typedef struct NcConfig {
  int max_clients;
  int worker_threads;
} NcConfig;

typedef struct ParseContext {
  int line;
  int success;
  NcConfig *conf;
  struct nc_pool *pool;
  char error[1025];
} ParseContext;

Token *
token_create(int id, int value_type, const char *ts, const char *te,
             ParseContext *ctx)
{
  size_t len = te - ts;
  size_t sz = sizeof(Token) + len + 1;
  Token *tk = (Token *)nc_palloc(ctx->pool, sz);
  memset(tk, 0, sz);
  tk->id = id;
  tk->value_type = value_type;
  tk->len = len;
  memcpy(tk->value, ts, len);

  printf("Token(%d, %d, %s)\n", tk->id, tk->value_type, tk->value);

  return tk;
}

Value *
value_create(ParseContext *ctx)
{
  Value *p_values = (Value *)nc_palloc(ctx->pool, sizeof(*p_values));
  nc_array_init(&p_values->arr, 5, sizeof(char *));

  return p_values;
}

void
value_append_token(Value *value, Token *token)
{
  Token **p_elem = nc_array_push(&value->arr);
  p_elem[0] = token;
}

KeyValue *
keyvalue_create(Token *token, Value *value, ParseContext *ctx)
{
  KeyValue *kv = (KeyValue *)nc_pcalloc(ctx->pool, sizeof(*kv));
  memcpy(kv->key, token->value, token->len);
  kv->value = value;

  return kv;
}

void
keyvalue_destroy(KeyValue *kv)
{
  if (kv->value && kv->value->arr.elems) {
    nc_free(kv->value->arr.elems);
  }
}

NcConfig *
nc_config_create()
{
  NcConfig *conf = nc_zalloc(sizeof(*conf));

  return conf;
}

void
nc_config_set(NcConfig *conf, KeyValue *kv, ParseContext *ctx)
{
}

ParseContext *
parse_context_create()
{
  ParseContext *ctx = nc_zalloc(sizeof(*ctx));
  ctx->line = 1;
  ctx->pool = nc_pool_create(NC_DEFAULT_POOL_SIZE);

  return ctx;
}

#define KEYWORD(ID)                                                            \
  {                                                                            \
    tk = token_create(ID, TOKEN_VALUE_TYPE_NONE, ts, te, ctx);                 \
    Parse(parser, ID, tk, ctx);                                                \
  }

#define SYMBOL(ID)                                                             \
  {                                                                            \
    Parse(parser, ID, NULL, ctx);                                              \
  }

#define ID()                                                                   \
  {                                                                            \
    tk = token_create(TK_ID, TOKEN_VALUE_TYPE_NONE, ts, te, ctx);              \
    Parse(parser, TK_ID, tk, ctx);                                             \
  }

#define INTEGER()                                                              \
  {                                                                            \
    tk = token_create(TK_ID, TOKEN_VALUE_TYPE_INTEGER, ts, te, ctx);           \
    Parse(parser, TK_ID, tk, ctx);                                             \
  }

#define FLOAT()                                                                \
  {                                                                            \
    tk = token_create(TK_ID, TOKEN_VALUE_TYPE_FLOAT, ts, te, ctx);             \
    Parse(parser, TK_ID, tk, ctx);                                             \
  }

#define STRING()                                                               \
  {                                                                            \
    tk = token_create(TK_ID, TOKEN_VALUE_TYPE_STRING, ts + 1, te - 1, ctx);    \
    Parse(parser, TK_ID, tk, ctx);                                             \
  }

#include "parser.c"
#include "lexer.c"

int
main(int argc, char *argv[])
{
  char cfg[] = "port 80;\n";
  ParseContext *ctx = ParseConfig(cfg);

  return 0;
}
