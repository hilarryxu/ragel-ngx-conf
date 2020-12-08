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
    snprintf(ctx->error, 1024, "%s(line: %d): %s", ERR_SYNTAX, ctx->line, (const char *)(TOKEN->value));
  } else {
    snprintf(ctx->error, 1024, "%s(line: %d)", ERR_SYNTAX, ctx->line);
  }
  ctx->success = 0;
}

%stack_overflow {
  snprintf(ctx->error, 1024, "parse config file stack overflow(%d)", ctx->line);
  ctx->success = 0;
}

%extra_argument {ParseContext *ctx}

%token_type {Token*}
%token_prefix TK_

%type conf {NcConfig*}
%type log {NcConfig*}
%type listen {NcConfig*}
%type keyvalue {KeyValue*}
%type value {Value*}
%type log_conf {NcConfig*}
%type listen_conf {NcConfig*}
%type buffer_type {Token*}

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
  keyvalue_destroy(B);
}
conf(A) ::= conf(B) keyvalue(C). {
  A = B;
  nc_config_set(A, C, ctx);
  keyvalue_destroy(C);
}
