TARGET = parse_cfg.exe

ifndef verbose
  SILENT = @
endif

$(TARGET): parse_cfg.c lexer.c parser.c
	$(SILENT) $(CC) parse_cfg.c -o $@ -I "../libnc/src" -L "../libnc/builddir" -lnc

lexer.c: lexer.rl
	$(SILENT) ragel $<

parser.c: parser.y
	$(SILENT) lemon $<
