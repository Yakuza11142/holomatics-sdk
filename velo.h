#ifndef VELO_H
#define VELO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_LET,
    TOKEN_VAR,
    TOKEN_FN,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COLON
} TokenType;

typedef struct {
    TokenType type;
    char text[64];
    int line;
} Token;

typedef struct {
    const char* source;
    size_t pos;
    int line;
} Lexer;

void lexer_init(Lexer* lexer, const char* source);
Token lexer_next_token(Lexer* lexer);

#endif
