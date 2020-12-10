#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#include "nc_palloc.h"
#include "nc_array.h"

#define ERR_SYNTAX "Syntax error"
#define ERR_BAD_CONF_FILE "Bad or empty config file"
#define ERR_MULTI_LOG "Multi log conf"
#define ERR_MULTI_LISTEN "Multi listen conf"

#define TK_STORE 7
#define TK_BUFFER_TYPE 8

#define TOKEN_VALUE_TYPE_NONE 0
#define TOKEN_VALUE_TYPE_INTEGER 1
#define TOKEN_VALUE_TYPE_FLOAT 2
#define TOKEN_VALUE_TYPE_STRING 3

#define MAX_KEY_LEN 64

struct Token {
  int id;
  int value_type;
  size_t len;
  union {
    int i;
    double d;
    char *s;
  } i_value;
};

struct Value {
  struct nc_array value_lst;
};

struct KeyValue {
  char key[MAX_KEY_LEN];
  struct Value *value;
};

struct LogConfig {
  char *level;
  char *file;
};

struct ListenConfig {
  char *socket;
  char *host;
  int port;
};

struct NcConfig {
  int max_clients;
  int worker_threads;
  struct LogConfig *log;
  struct ListenConfig *listen;
};

struct ParseContext {
  int line;
  int success;
  struct NcConfig *conf;
  struct nc_pool *pool;
  char error[1025];
};

// Token
struct Token *
token_create(int id, int value_type, const char *ts, const char *te,
             struct ParseContext *ctx)
{
  size_t len = te - ts;
  size_t sz = sizeof(struct Token) + len + 1;
  struct Token *tk = (struct Token *)nc_palloc(ctx->pool, sz);
  memset(tk, 0, sz);

  tk->id = id;
  tk->value_type = value_type;
  tk->len = len;

  char *s = nc_pnalloc(ctx->pool, len + 1);
  memset(s, 0, len + 1);
  memcpy(s, ts, len);
  if (value_type == TOKEN_VALUE_TYPE_INTEGER) {
    tk->i_value.i = atoi(s);
  } else if (value_type == TOKEN_VALUE_TYPE_FLOAT) {
    tk->i_value.d = strtod(s, NULL);
  } else {
    tk->i_value.s = s;
  }

  // printf("Token(%d, %d, %s)\n", tk->id, tk->value_type, s);
  return tk;
}

// Value
struct Value *
value_create(struct ParseContext *ctx)
{
  struct Value *value = (struct Value *)nc_palloc(ctx->pool, sizeof(*value));
  nc_array_init(&value->value_lst, 10, sizeof(struct Token *));

  return value;
}

void
value_append_token(struct Value *value, struct Token *token)
{
  struct Token **tokens = nc_array_push(&value->value_lst);
  tokens[0] = token;
}

// KeyValue
struct KeyValue *
keyvalue_create(struct Token *token, struct Value *value,
                struct ParseContext *ctx)
{
  struct KeyValue *kv = (struct KeyValue *)nc_pcalloc(ctx->pool, sizeof(*kv));
  memcpy(kv->key, token->i_value.s, token->len);
  kv->value = value;

  return kv;
}

void
keyvalue_destroy(struct KeyValue *kv)
{
  struct Value *value = kv->value;
  if (value && value->value_lst.elems) {
    nc_free(value->value_lst.elems);
  }
}

// LogConfig
struct LogConfig *
log_config_create()
{
  struct LogConfig *log_conf = (struct LogConfig *)nc_zalloc(sizeof(*log_conf));
  return log_conf;
}

void
log_config_set(struct LogConfig *log_conf, struct KeyValue *kv,
               struct ParseContext *ctx)
{
  struct Token **tokens = kv->value->value_lst.elems;

  if (strcmp(kv->key, "level") == 0) {
    log_conf->level = strdup(tokens[0]->i_value.s);
  } else if (strcmp(kv->key, "file") == 0) {
    log_conf->file = strdup(tokens[0]->i_value.s);
  }
}

// ListenConfig
struct ListenConfig *
listen_config_create()
{
  struct ListenConfig *conf = (struct ListenConfig *)nc_zalloc(sizeof(*conf));
  return conf;
}

void
listen_config_set(struct ListenConfig *conf, struct KeyValue *kv,
                  struct ParseContext *ctx)
{
  struct Token **tokens = kv->value->value_lst.elems;

  if (strcmp(kv->key, "socket") == 0) {
    conf->socket = strdup(tokens[0]->i_value.s);
  } else if (strcmp(kv->key, "host") == 0) {
    conf->host = strdup(tokens[0]->i_value.s);
  } else if (strcmp(kv->key, "port") == 0) {
    conf->port = tokens[0]->i_value.i;
  }
}

// NcConfig
struct NcConfig *
nc_config_create()
{
  struct NcConfig *conf = nc_zalloc(sizeof(*conf));

  return conf;
}

void
nc_config_set(struct NcConfig *conf, struct KeyValue *kv,
              struct ParseContext *ctx)
{
}

// ParseContext
struct ParseContext *
parse_context_create()
{
  struct ParseContext *ctx = nc_zalloc(sizeof(*ctx));
  ctx->success = 1;
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
  char cfg[] = "port 80;\n"
               "log {\n"
               "  level info;\n"
               "  file /var/log/nc.log;\n"
               "}\n"
               "listen {\n"
               "  port 3388;\n"
               "  socket /var/run/nc.sock;\n"
               "}\n";
  struct ParseContext *ctx = ParseConfig(cfg);

  if (!ctx->success) {
    printf("error: %s\n", ctx->error);
  } else {
    struct NcConfig *conf = ctx->conf;
    if (conf->log) {
      printf("log:\n");
      printf("  level: %s\n", conf->log->level);
      printf("  file: %s\n", conf->log->file);
    }
    if (conf->listen) {
      printf("listen:\n");
      printf("  socket: %s\n", conf->listen->socket);
      printf("  port: %d\n", conf->listen->port);
    }
  }

  return 0;
}
