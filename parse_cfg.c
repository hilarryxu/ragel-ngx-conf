#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "conf.h"

#include "nc_palloc.h"

#define CONF_ASSIGN_SDS(to, from) to = sdscpylen(to, from, sdslen(from))
#define CONF_ASSIGN_SDS_FROM_TOKEN(to, token)                                  \
  to = sdscpylen(to, token->i_value.s, token->len)

char *conf_set_num(void *cf, struct conf_command *cmd, struct KeyValue *kv,
                   struct ParseContext *ctx);
char *conf_set_bool(void *cf, struct conf_command *cmd, struct KeyValue *kv,
                    struct ParseContext *ctx);
char *conf_set_sds(void *cf, struct conf_command *cmd, struct KeyValue *kv,
                   struct ParseContext *ctx);

static struct conf_command conf_commands[] = {
    {"id", conf_set_sds, offsetof(struct NcConfig, id)},
    {"max_clients", conf_set_num, offsetof(struct NcConfig, max_clients)},
    {"worker_threads", conf_set_num, offsetof(struct NcConfig, worker_threads)},
    {"ignore_case", conf_set_bool, offsetof(struct NcConfig, ignore_case)},

    null_command};

static struct conf_command log_conf_commands[] = {
    {"level", conf_set_sds, offsetof(struct LogConfig, level)},
    {"file", conf_set_sds, offsetof(struct LogConfig, file)},

    null_command};

static struct conf_command listen_conf_commands[] = {
    {"socket", conf_set_sds, offsetof(struct ListenConfig, socket)},
    {"host", conf_set_sds, offsetof(struct ListenConfig, host)},
    {"port", conf_set_num, offsetof(struct ListenConfig, port)},

    null_command};

static struct conf_command store_conf_commands[] = {
    {"socket", conf_set_sds, offsetof(struct StoreConfig, socket)},
    {"host", conf_set_sds, offsetof(struct StoreConfig, host)},
    {"port", conf_set_num, offsetof(struct StoreConfig, port)},
    {"path", conf_set_sds, offsetof(struct StoreConfig, path)},
    {"name", conf_set_sds, offsetof(struct StoreConfig, name)},
    {"rotate", conf_set_sds, offsetof(struct StoreConfig, rotate)},
    {"flush", conf_set_sds, offsetof(struct StoreConfig, flush)},
    {"success", conf_set_sds, offsetof(struct StoreConfig, success)},
    {"topic", conf_set_sds, offsetof(struct StoreConfig, topic)},

    null_command};

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
  assert(token->len <= MAX_KEY_LEN);
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
  struct conf_command *cmd;

  for (cmd = log_conf_commands; cmd->name != NULL; cmd++) {
    char *rv;

    if (strcmp(kv->key, cmd->name) != 0) {
      continue;
    }

    rv = cmd->set(cf, cmd, kv, ctx);
    if (rv != CONF_OK) {
      snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
               "line: %d, log: directive \"%s\" %s", ctx->line, kv->key, rv);
      ctx->success = 0;
    }

    return;
  }

  snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
           "line: %d, log: directive \"%s\" is unknown", ctx->line, kv->key);
  ctx->success = 0;
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
  struct conf_command *cmd;

  for (cmd = listen_conf_commands; cmd->name != NULL; cmd++) {
    char *rv;

    if (strcmp(kv->key, cmd->name) != 0) {
      continue;
    }

    rv = cmd->set(cf, cmd, kv, ctx);
    if (rv != CONF_OK) {
      snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
               "line: %d, listen: directive \"%s\" %s", ctx->line, kv->key, rv);
      ctx->success = 0;
    }

    return;
  }

  snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
           "line: %d, listen: directive \"%s\" is unknown", ctx->line, kv->key);
  ctx->success = 0;
}

void
listen_config_destroy(struct ListenConfig *cf)
{
  sdsfree(cf->socket);
  sdsfree(cf->host);
  nc_free(cf);
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
  struct conf_command *cmd;

  for (cmd = store_conf_commands; cmd->name != NULL; cmd++) {
    char *rv;

    if (strcmp(kv->key, cmd->name) != 0) {
      continue;
    }

    rv = cmd->set(cf, cmd, kv, ctx);
    if (rv != CONF_OK) {
      snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
               "line: %d, store: directive \"%s\" %s", ctx->line, kv->key, rv);
      ctx->success = 0;
    }

    return;
  }

  snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
           "line: %d, store: directive \"%s\" is unknown", ctx->line, kv->key);
  ctx->success = 0;
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
  sdsfree(cf->type);
  sdsfree(cf->buffer_type);
  sdsfree(cf->socket);
  sdsfree(cf->host);
  sdsfree(cf->path);
  sdsfree(cf->name);
  sdsfree(cf->rotate);
  sdsfree(cf->flush);
  sdsfree(cf->success);
  sdsfree(cf->topic);
  nc_free(cf);
}

// NcConfig
struct NcConfig *
nc_config_create()
{
  struct NcConfig *cf = nc_zalloc(sizeof(*cf));
  cf->id = CONF_UNSET_SDS;
  cf->log_level = CONF_UNSET_SDS;
  cf->log_file = CONF_UNSET_SDS;
  cf->max_clients = CONF_UNSET_NUM;
  cf->worker_threads = CONF_UNSET_NUM;
  cf->ignore_case = CONF_UNSET_NUM;

  return cf;
}

void
nc_config_set(struct NcConfig *cf, struct KeyValue *kv,
              struct ParseContext *ctx)
{
  struct conf_command *cmd;

  for (cmd = conf_commands; cmd->name != NULL; cmd++) {
    char *rv;

    if (strcmp(kv->key, cmd->name) != 0) {
      continue;
    }

    rv = cmd->set(cf, cmd, kv, ctx);
    if (rv != CONF_OK) {
      snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
               "line: %d, conf: directive \"%s\" %s", ctx->line, kv->key, rv);
      ctx->success = 0;
    }

    return;
  }

  snprintf(ctx->error, MAX_PARSE_CONTEXT_ERROR_LEN,
           "line: %d, conf: directive \"%s\" is unknown", ctx->line, kv->key);
  ctx->success = 0;
}

// ParseContext
struct ParseContext *
parse_context_create()
{
  struct ParseContext *ctx = nc_zalloc(sizeof(*ctx));
  ctx->success = 1;
  ctx->line = 0;
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

  if (sdslen(store->buffer_type) > 0) {
    printf("%sstore %s %s:\n", spaces, store->type, store->buffer_type);
  } else {
    printf("%sstore %s:\n", spaces, store->type);
  }

#define PRINT_STORE_ITEM(_name)                                                \
  if (sdslen(store->_name) > 0) {                                              \
    printf("  %s" #_name ": %s\n", spaces, store->_name);                      \
  }

  PRINT_STORE_ITEM(socket)
  PRINT_STORE_ITEM(host)
  if (store->port != CONF_UNSET_NUM) {
    printf("  %sport: %d\n", spaces, store->port);
  }
  PRINT_STORE_ITEM(path)
  PRINT_STORE_ITEM(name)
  PRINT_STORE_ITEM(rotate)
  PRINT_STORE_ITEM(flush)
  PRINT_STORE_ITEM(success)
  PRINT_STORE_ITEM(topic)

  if (store->stores.nelem > 0) {
    struct StoreConfig **stores = (struct StoreConfig **)store->stores.elems;
    for (int i = 0; i < store->stores.nelem; i++) {
      print_store(stores[i], depth + 1);
    }
  }
}

char *
conf_set_num(void *cf, struct conf_command *cmd, struct KeyValue *kv,
             struct ParseContext *ctx)
{
  struct Token *value = kv->value->tokens[0];
  uint8_t *p;
  int *np;

  p = cf;
  np = (int *)(p + cmd->offset);

  if (*np != CONF_UNSET_NUM) {
    return "is a duplicate";
  }

  if (value->value_type != TOKEN_VALUE_TYPE_INTEGER) {
    return "is not a number";
  }

  *np = value->i_value.i;

  return CONF_OK;
}

char *
conf_set_bool(void *cf, struct conf_command *cmd, struct KeyValue *kv,
              struct ParseContext *ctx)
{
  struct Token *value = kv->value->tokens[0];
  uint8_t *p;
  int *bp;

  p = cf;
  bp = (int *)(p + cmd->offset);

  if (*bp != CONF_UNSET_NUM) {
    return "is a duplicate";
  }

  if (value->value_type == TOKEN_VALUE_TYPE_INTEGER) {
    if (value->i_value.i == 1) {
      *bp = 1;
    } else if (value->i_value.i == 0) {
      *bp = 0;
    } else {
      return "is not 1 or 0";
    }
  } else if ((value->value_type == TOKEN_VALUE_TYPE_NONE) ||
             (value->value_type == TOKEN_VALUE_TYPE_STRING)) {
    if ((strcmp(value->i_value.s, "true") == 0) ||
        (strcmp(value->i_value.s, "on") == 0)) {
      *bp = 0;
    } else if ((strcmp(value->i_value.s, "false") == 0) ||
               (strcmp(value->i_value.s, "off") == 0)) {
      *bp = 1;
    } else {
      return "is not true,false,on,off";
    }
  } else {
    return CONF_ERROR;
  }

  return CONF_OK;
}

char *
conf_set_sds(void *cf, struct conf_command *cmd, struct KeyValue *kv,
             struct ParseContext *ctx)
{
  struct Token *value = kv->value->tokens[0];
  uint8_t *p;
  sds *sp;

  p = cf;
  sp = (sds *)(p + cmd->offset);

  if (sdslen(*sp) > 0) {
    return "is a duplicate";
  }

  if ((value->value_type == TOKEN_VALUE_TYPE_NONE) ||
      (value->value_type == TOKEN_VALUE_TYPE_STRING)) {
    *sp = sdscpylen(*sp, value->i_value.s, value->len);
  } else {
    return "is not a string";
  }

  return CONF_OK;
}

// FIXME(xcc): nc_config_check

int
main(int argc, char *argv[])
{
  char cfg[] = "ignore_case on;\n"
               "worker_threads 4;\n"
               "id 'cfg-01';\n"
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
    printf("ERR: %s\n", ctx->error);
  } else {
    struct NcConfig *conf = ctx->conf;
    printf("id: %s\n", conf->id);
    printf("max_clients: %d\n", conf->max_clients);
    printf("worker_threads: %d\n", conf->worker_threads);
    printf("ignore_case: %d\n", conf->ignore_case);
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
