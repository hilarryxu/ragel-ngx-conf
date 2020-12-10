%%{
  machine conf_lexer;

  line_sane = ( '\n' ) >{ ctx->line++; };
  line_weird = ( '\r' ) >{ ctx->line++; };
  line_insane = ( '\r' ( '\n' >{ ctx->line--; } ) );
  line = ( line_sane | line_weird | line_insane );

  ws = ( '\t' | ' ' );
  comment = ( '#' (any - line)* line );
  noise = ( ws | line | comment );

  dliteralChar = [^'"\\] | ( '\\' any );

  main := |*
    ('-'|'+')? [1-9][0-9]* { INTEGER() };
    ('-'|'+')? [0-9]* '.' [0-9]+{ FLOAT() };

    /listen/i { KEYWORD(TK_LISTEN) };
    /store/i { KEYWORD(TK_STORE) };
    /log/i { KEYWORD(TK_LOG) };
    /primary/i { KEYWORD(TK_BUFFER_TYPE) };
    /secondary/i { KEYWORD(TK_BUFFER_TYPE) };

    '{' { SYMBOL(TK_LP) };
    '}' { SYMBOL(TK_RP) };
    ';' { SYMBOL(TK_SEMICOLON) };

    '"' (dliteralChar | "'")* '"' { STRING() };
    "'" (dliteralChar | '"')* "'" { STRING() };

    (print - noise - [,<>=;{}])+ { ID() };

    comment;
    noise;
  *|;
}%%

%% write data;

struct ParseContext *
ParseConfig(char *cfg_str)
{
  char *p = cfg_str;
  char *pe = p + strlen(cfg_str);
  char *eof = pe;
  int cs, act;
  char *ts, *te;

  struct ParseContext *ctx = parse_context_create();
  if (cfg_str == NULL) {
    ctx->success = 0;
    snprintf(ctx->error, 1024, ERR_BAD_CONF_FILE);
    return ctx;
  }

  struct Token *tk = NULL;

  void *parser = ParseAlloc(malloc);

  %% write init;
  %% write exec;

  if (cs < %%{ write first_final; }%%) {
    snprintf(ctx->error, 1024, "%s(line: %d)", ERR_SYNTAX, ctx->line);
    ctx->success = 0;
  } else {
    Parse(parser, 0, 0, ctx);
  }

  ParseFree(parser, free);

  return ctx;
}
