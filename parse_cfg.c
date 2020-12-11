#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "conf.h"

#include "nc_palloc.h"

#define CONF_COPY_SDS(to, from) to = sdscpylen(to, from, sdslen(from))

#define CONF_SET_NUM_SLOT(field, token) field = token->i_value.i
#define CONF_SET_SDS_SLOT(field, token)                                        \
  field = sdscpylen(field, token->i_value.s, token->len)

// Token
struct Token *
token_create(int id, int value_type, const char *ts, const char *te,
             struct ParseContext *ctx)
{
  size_t len = te - ts;
  struct Token *tk = (struct Token *)nc_palloc(ctx->pool, sizeof(*tk));
  tk->id = id;
  tk->value_type = value_type;
  tk->len = len;

  char *s = nc_pnalloc(ctx->pool, len + 1);
  memcpy(s, ts, len);
  s[len] = '\0';
  if (value_type == TOKEN_VALUE_TYPE_INTEGER) {
    tk->i_value.i = atoi(s);
  } else if (value_type == TOKEN_VALUE_TYPE_FLOAT) {
    tk->i_value.d = strtod(s, NULL);
  } else {
    // FIXME(xcc): handle escaped string
    tk->i_value.s = s;
  }

  // printf("Token(%d, %d, %s)\n", tk->id, tk->value_type, s);
  return tk;
}

// Value
struct Value *
value_create(struct ParseContext *ctx)
{
  struct Value *value = (struct Value *)nc_pcalloc(ctx->pool, sizeof(*value));

  return value;
}

void
value_append_token(struct Value *value, struct Token *token)
{
  assert(value->n < MAX_VALUE_TOKENS_NR);
  value->tokens[value->n++] = token;
}

// KeyValue
struct KeyValue *
keyvalue_create(struct Token *token, struct Value *value,
                struct ParseContext *ctx)
{
  struct KeyValue *kv = (struct KeyValue *)nc_palloc(ctx->pool, sizeof(*kv));
  memcpy(kv->key, token->i_value.s, token->len);
  kv->key[token->len] = '\0';
  kv->value = value;

  return kv;
}

// LogConfig
struct LogConfig *
log_config_create()
{
  struct LogConfig *cf = (struct LogConfig *)nc_zalloc(sizeof(*cf));
  cf->level = CONF_UNSET_SDS;
  cf->file = CONF_UNSET_SDS;

  return cf;
}

void
log_config_set(struct LogConfig *cf, struct KeyValue *kv,
               struct ParseContext *ctx)
{
  struct Token **tokens = kv->value->tokens;

  if (strcmp(kv->key, "level") == 0) {
    CONF_SET_SDS_SLOT(cf->level, tokens[0]);
  } else if (strcmp(kv->key, "file") == 0) {
    CONF_SET_SDS_SLOT(cf->file, tokens[0]);
  }
}

void
log_config_destroy(struct LogConfig *cf)
{
  sdsfree(cf->level);
  sdsfree(cf->file);
  nc_free(cf);
}

// ListenConfig
struct ListenConfig *
listen_config_create()
{
  struct ListenConfig *cf = (struct ListenConfig *)nc_alloc(sizeof(*cf));
  cf->socket = CONF_UNSET_SDS;
  cf->host = CONF_UNSET_SDS;
  cf->port = CONF_UNSET_NUM;

  return cf;
}

void
listen_config_set(struct ListenConfig *cf, struct KeyValue *kv,
                  struct ParseContext *ctx)
{
  struct Token **tokens = kv->value->tokens;

  if (strcmp(kv->key, "socket") == 0) {
    CONF_SET_SDS_SLOT(cf->socket, tokens[0]);
  } else if (strcmp(kv->key, "host") == 0) {
    CONF_SET_SDS_SLOT(cf->host, tokens[0]);
  } else if (strcmp(kv->key, "port") == 0) {
    CONF_SET_NUM_SLOT(cf->port, tokens[0]);
  }
}

// StoreConfig
struct StoreConfig *
store_config_create()
{
  struct StoreConfig *cf = (struct StoreConfig *)nc_zalloc(sizeof(*cf));
  cf->type = CONF_UNSET_SDS;
  cf->port = CONF_UNSET_NUM;
  cf->buffer_type = CONF_UNSET_SDS;
  cf->socket = CONF_UNSET_SDS;
  cf->host = CONF_UNSET_SDS;
  cf->path = CONF_UNSET_SDS;
  cf->name = CONF_UNSET_SDS;
  cf->rotate = CONF_UNSET_SDS;
  cf->flush = CONF_UNSET_SDS;
  cf->success = CONF_UNSET_SDS;
  cf->topic = CONF_UNSET_SDS;
  nc_array_init(&cf->stores, MAX_SUB_STORES_NR, sizeof(struct StoreConfig *));

  return cf;
}

void
store_config_set(struct StoreConfig *cf, struct KeyValue *kv,
                 struct ParseContext *ctx)
{
  struct Token **tokens = kv->value->tokens;

  if (strcmp(kv->key, "socket") == 0) {
    CONF_SET_SDS_SLOT(cf->socket, tokens[0]);
  } else if (strcmp(kv->key, "host") == 0) {
    CONF_SET_SDS_SLOT(cf->host, tokens[0]);
  } else if (strcmp(kv->key, "port") == 0) {
    CONF_SET_NUM_SLOT(cf->port, tokens[0]);
  }
}

void
store_config_append_store(struct StoreConfig *cf, struct StoreConfig *store)
{
  struct StoreConfig **p_store = nc_array_push(&cf->stores);
  *p_store = store;
}

void
store_config_destroy(struct StoreConfig *cf)
{
}

// NcConfig
struct NcConfig *
nc_config_create()
{
  struct NcConfig *cf = nc_zalloc(sizeof(*cf));
  cf->log_level = CONF_UNSET_SDS;
  cf->log_file = CONF_UNSET_SDS;

  return cf;
}

void
nc_config_set(struct NcConfig *cf, struct KeyValue *kv,
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

void
print_store(struct StoreConfig *store, int depth)
{
  char spaces[64] = {0};

  memset(spaces, ' ', 63);
  spaces[depth * 2] = '\0';

  printf("%sstore %s:\n", spaces, store->type);

  if (store->stores.nelem > 0) {
    struct StoreConfig **stores = (struct StoreConfig **)store->stores.elems;
    for (int i = 0; i < store->stores.nelem; i++) {
      print_store(stores[i], depth + 1);
    }
  }
}

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
               "}\n"
               "store buffer {\n"
               "  store network primary {\n"
               "    host www.myserver.com;\n"
               "    port 3388;\n"
               "  }\n"
               "  store file secondary {\n"
               "    path /data/data/kidsbuf;\n"
               "    rotate 5min;\n"
               "  }\n"
               "}\n";

  struct ParseContext *ctx = ParseConfig(cfg);

  if (!ctx->success) {
    printf("error: %s\n", ctx->error);
  } else {
    struct NcConfig *conf = ctx->conf;
    if (conf->log_file) {
      printf("log:\n");
      printf("  level: %s\n", conf->log_level);
      printf("  file: %s\n", conf->log_file);
    }
    if (conf->listen) {
      printf("listen:\n");
      printf("  socket: %s\n", conf->listen->socket);
      printf("  port: %d\n", conf->listen->port);
    }
    print_store(conf->store, 0);
  }

  return 0;
}
