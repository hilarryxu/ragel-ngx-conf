#ifndef T_CONF_H_
#define T_CONF_H_

#include "nc_sds.h"
#include "nc_array.h"

#define ERR_SYNTAX "Syntax error"
#define ERR_BAD_CONF_FILE "Bad or empty config file"
#define ERR_MULTI_LOG "Multi log conf"
#define ERR_MULTI_LISTEN "Multi listen conf"
#define ERR_MULTI_STORE "Multi store conf"

#define TOKEN_VALUE_TYPE_NONE 0
#define TOKEN_VALUE_TYPE_INTEGER 1
#define TOKEN_VALUE_TYPE_FLOAT 2
#define TOKEN_VALUE_TYPE_STRING 3

#define MAX_PARSE_CONTEXT_ERROR_LEN 1024
#define MAX_KEY_LEN 63
#define MAX_VALUE_TOKENS_NR 10
#define MAX_SUB_STORES_NR 5

#define CONF_OK (void *)NULL
#define CONF_ERROR (void *)"has an invalid value"

#define CONF_UNSET -1
#define CONF_UNSET_NUM -1
#define CONF_UNSET_SDS sdsempty()
#define CONF_UNSET_PTR NULL

struct Token {
  int id;
  int len;
  int value_type;
  union {
    int i;
    double d;
    char *s;
  } i_value;
};

struct Value {
  int n;
  struct Token *tokens[MAX_VALUE_TOKENS_NR];
};

struct KeyValue {
  char key[MAX_KEY_LEN + 1];
  struct Value *value;
};

struct LogConfig {
  sds level;
  sds file;
};

struct ListenConfig {
  sds socket;
  sds host;
  int port;
};

struct StoreConfig {
  sds type;
  sds buffer_type;
  sds socket;
  sds host;
  int port;
  sds path;
  sds name;
  sds rotate;
  sds flush;
  sds success;
  sds topic;
  struct nc_array stores;
};

struct NcConfig {
  sds id;
  sds log_level;
  sds log_file;
  int max_clients;
  int worker_threads;
  int ignore_case;
  struct ListenConfig *listen;
  struct StoreConfig *store;
};

struct ParseContext {
  int line;
  int success;
  struct NcConfig *conf;
  struct nc_pool *pool;
  char error[MAX_PARSE_CONTEXT_ERROR_LEN + 1];
};

struct conf_command {
  char *name;
  char *(*set)(void *cf, struct conf_command *cmd, struct KeyValue *kv,
               struct ParseContext *ctx);
  int offset;
};

#define null_command                                                           \
  {                                                                            \
    NULL, NULL, 0                                                              \
  }

#endif // T_CONF_H_
