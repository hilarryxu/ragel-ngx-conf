#include <stdio.h>

#include <string>

#define ERR_SYNTAX "Syntax error"
#define ERR_BAD_CONF_FILE "Bad or empty config file"

#define TK_ID 1
#define TK_SEMICOLON 2
#define TK_LOG 3
#define TK_LP 4
#define TK_RP 5
#define TK_LISTEN 6
#define TK_STORE 7
#define TK_BUFFER_TYPE 8

#define SUB_TYPE_NONE 0

#define ID_INTEGER 1
#define ID_FLOAT 2
#define ID_STRING 3

void emit(int token_type, int sub_type, const char* ts, const char* te) {
  std::string token_value;
  if (ts != NULL)
    token_value.assign(ts, te - ts);
  printf("TOKEN_TYPE: %d, SUB_TYPE: %d, TOKEN_VALUE: %s\n", token_type,
         sub_type, token_value.c_str());
}

#define KEYWORD(ID) \
  { emit(ID, SUB_TYPE_NONE, ts, te); }

#define SYMBOL(ID) \
  { emit(ID, SUB_TYPE_NONE, ts, te); }

#define ID() \
  { emit(TK_ID, SUB_TYPE_NONE, ts, te); }

#define INTEGER() \
  { emit(TK_ID, ID_INTEGER, ts, te); }

#define FLOAT() \
  { emit(TK_ID, ID_FLOAT, ts, te); }

#define STRING() \
  { emit(TK_ID, ID_STRING, ts + 1, te - 1); }

struct ParseContext {
  ParseContext() : line(1), success(true) {}

  int line;
  bool success;
  char error[1025];
};

#include "lexer.c"

int main(int argc, char* argv[]) {
  std::string cfg =
      "\n"
      "listen {\n"
      "  port 80;\n"
      "  host 'www.google.cn';\n"
      "}";
  ParseContext* ctx = ParseConfig(cfg);

  return 0;
}

// ragel lexer.rl && g++ -o token_scanner token_scanner.cc && ./token_scanner.exe
