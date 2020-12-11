%include {
  #include <assert.h>
}

%token_destructor { ((void)ctx); }

%parse_failure {
  ctx->success = 0;
  snprintf(ctx->error, 1024, "%s(line: %d)", ERR_SYNTAX, ctx->line);
}

%syntax_error {
  if (TOKEN) {
    snprintf(ctx->error, 1024, "%s(line: %d): %d", ERR_SYNTAX, ctx->line, TOKEN->value_type);
  } else {
    snprintf(ctx->error, 1024, "%s(line: %d)", ERR_SYNTAX, ctx->line);
  }
  ctx->success = 0;
}

%stack_overflow {
  snprintf(ctx->error, 1024, "parse config file stack overflow(%d)", ctx->line);
  ctx->success = 0;
}

%extra_argument {struct ParseContext *ctx}

%token_type {struct Token*}
%token_prefix TK_

%type conf {struct NcConfig*}
%type log {struct LogConfig*}
%type listen {struct ListenConfig*}
%type store {struct StoreConfig*}
%type keyvalue {struct KeyValue*}
%type value {struct Value*}
%type log_conf {struct LogConfig*}
%type listen_conf {struct ListenConfig*}
%type store_conf {struct StoreConfig*}
%type buffer_type {struct Token*}

main ::= conf(A). {
  ctx->conf = A;
}

value(A) ::= ID(V). {
  A = value_create(ctx);
  value_append_token(A, V);
}
value(A) ::= value(B) ID(V). {
  A = B;
  value_append_token(A, V);
}

keyvalue(A) ::= ID(K) value(V) SEMICOLON. {
  A = keyvalue_create(K, V, ctx);
}

conf(A) ::= keyvalue(B). {
  A = nc_config_create();
  nc_config_set(A, B, ctx);
}
conf(A) ::= conf(B) keyvalue(C). {
  A = B;
  nc_config_set(A, C, ctx);
}

conf(A) ::= log(B). {
  A = nc_config_create();
  CONF_COPY_SDS(A->log_level, B->level);
  CONF_COPY_SDS(A->log_file, B->file);
}
conf(A) ::= conf(B) log(C). {
  A = B;
  if (sdslen(A->log_level) > 0) {
    snprintf(ctx->error, 1024, ERR_MULTI_LOG);
    ctx->success = 0;
    log_config_destroy(C);
  } else {
    CONF_COPY_SDS(A->log_level, C->level);
    CONF_COPY_SDS(A->log_file, C->file);
    log_config_destroy(C);
  }
}

conf(A) ::= listen(B). {
  A = nc_config_create();
  A->listen = B;
}
conf(A) ::= conf(B) listen(C). {
  A = B;
  if (A->listen != NULL) {
    snprintf(ctx->error, 1024, ERR_MULTI_LISTEN);
    ctx->success = 0;
  } else {
    A->listen = C;
  }
}

conf(A) ::= store(B). {
  A = nc_config_create();
  A->store = B;
}
conf(A) ::= conf(B) store(C). {
  A = B;
  if (A->store != NULL) {
    snprintf(ctx->error, 1024, ERR_MULTI_STORE);
    ctx->success = 0;
    store_config_destroy(C);
  } else {
    A->store = C;
  }
}

log(A) ::= LOG LP log_conf(B) RP. {
  A = B;
}
log_conf(A) ::= keyvalue(B). {
  A = log_config_create();
  log_config_set(A, B, ctx);
}
log_conf(A) ::= log_conf(B) keyvalue(C). {
  A = B;
  log_config_set(A, C, ctx);
}

listen(A) ::= LISTEN LP listen_conf(B) RP. {
  A = B;
}
listen_conf(A) ::= keyvalue(B). {
  A = listen_config_create();
  listen_config_set(A, B, ctx);
}
listen_conf(A) ::= listen_conf(B) keyvalue(C). {
  A = B;
  listen_config_set(A, C, ctx);
}

store(A) ::= STORE ID(B) buffer_type(C) LP store_conf(D) RP. {
  A = D;
  A->type = sdscpylen(A->type, B->i_value.s, B->len);
  if (C != NULL) {
    A->buffer_type = sdscpylen(A->buffer_type, C->i_value.s, C->len);
  }
}
store(A) ::= STORE ID(B) buffer_type(C) LP RP. {
  A = store_config_create();
  A->type = sdscpylen(A->type, B->i_value.s, B->len);
  if (C != NULL) {
    A->buffer_type = sdscpylen(A->buffer_type, C->i_value.s, C->len);
  }
}

buffer_type(A) ::= BUFFER_TYPE(B). {
  A = B;
}
buffer_type(A) ::= . {
  A = NULL;
}

store_conf(A) ::= keyvalue(B). {
  A = store_config_create();
  store_config_set(A, B, ctx);
}
store_conf(A) ::= store_conf(B) keyvalue(C). {
  A = B;
  store_config_set(A, C, ctx);
}
store_conf(A) ::= store(B). {
  A = store_config_create();
  store_config_append_store(A, B);
}
store_conf(A) ::= store_conf(B) store(C). {
  A = B;
  store_config_append_store(A, C);
}
