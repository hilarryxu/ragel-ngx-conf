TARGET = token_scanner.exe

ifndef verbose
  SILENT = @
endif

$(TARGET): token_scanner.cc lexer.c
	$(SILENT) $(CXX) -o $@ token_scanner.cc

lexer.c: lexer.rl
	$(SILENT) ragel $<
