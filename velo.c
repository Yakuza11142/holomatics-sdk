#include "velo.h"

void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
}

Token lexer_next_token(Lexer* lexer) {
    while (lexer->source[lexer->pos] != '\0') {
        char c = lexer->source[lexer->pos];

        if (c == ' ' || c == '\t' || c == '\r') {
            lexer->pos++;
            continue;
        }
        if (c == '\n') {
            lexer->line++;
            lexer->pos++;
            continue;
        }

        if (isalpha(c) || c == '_') {
            size_t start = lexer->pos;
            while (isalnum(lexer->source[lexer->pos]) || lexer->source[lexer->pos] == '_') {
                lexer->pos++;
            }
            size_t len = lexer->pos - start;
            Token token;
            token.line = lexer->line;
            strncpy(token.text, lexer->source + start, len);
            token.text[len] = '\0';

            if (strcmp(token.text, "let") == 0) token.type = TOKEN_LET;
            else if (strcmp(token.text, "var") == 0) token.type = TOKEN_VAR;
            else if (strcmp(token.text, "fn") == 0) token.type = TOKEN_FN;
            else token.type = TOKEN_IDENTIFIER;

            return token;
        }

        if (isdigit(c)) {
            size_t start = lexer->pos;
            while (isdigit(lexer->source[lexer->pos])) {
                lexer->pos++;
            }
            size_t len = lexer->pos - start;
            Token token;
            token.type = TOKEN_NUMBER;
            token.line = lexer->line;
            strncpy(token.text, lexer->source + start, len);
            token.text[len] = '\0';
            return token;
        }

        lexer->pos++;
        Token token;
        token.line = lexer->line;
        token.text[0] = c;
        token.text[1] = '\0';

        switch (c) {
            case '=': token.type = TOKEN_ASSIGN; return token;
            case '+': token.type = TOKEN_PLUS; return token;
            case '-': token.type = TOKEN_MINUS; return token;
            case '*': token.type = TOKEN_MUL; return token;
            case '/': token.type = TOKEN_DIV; return token;
            case ';': token.type = TOKEN_SEMICOLON; return token;
            case '(': token.type = TOKEN_LPAREN; return token;
            case ')': token.type = TOKEN_RPAREN; return token;
            case '{': token.type = TOKEN_LBRACE; return token;
            case '}': token.type = TOKEN_RBRACE; return token;
            case ':': token.type = TOKEN_COLON; return token;
        }
    }

    Token eof = {TOKEN_EOF, "", lexer->line};
    return eof;
}

// Low-Level C Code Generator backend directly from Velo Token stream
void compile_velo_to_c(const char* source, FILE* out_file) {
    Lexer lexer;
    lexer_init(&lexer, source);

    fprintf(out_file, "// Generated C Code from Velo Compiler (veloc)\n");
    fprintf(out_file, "#include <stdio.h>\n");
    fprintf(out_file, "#include <stdint.h>\n\n");

    Token tok = lexer_next_token(&lexer);
    while (tok.type != TOKEN_EOF) {
        if (tok.type == TOKEN_FN) {
            Token fn_name = lexer_next_token(&lexer);
            lexer_next_token(&lexer); // (
            lexer_next_token(&lexer); // )
            lexer_next_token(&lexer); // {
            
            fprintf(out_file, "int %s() {\n", fn_name.text);
            
            Token inner = lexer_next_token(&lexer);
            while (inner.type != TOKEN_RBRACE && inner.type != TOKEN_EOF) {
                if (inner.type == TOKEN_LET || inner.type == TOKEN_VAR) {
                    Token var_name = lexer_next_token(&lexer);
                    lexer_next_token(&lexer); // :
                    Token var_type = lexer_next_token(&lexer); // type (e.g. u32)
                    lexer_next_token(&lexer); // =
                    Token val = lexer_next_token(&lexer); // val
                    lexer_next_token(&lexer); // ;

                    char c_type[16] = "int";
                    if (strcmp(var_type.text, "u32") == 0) strcpy(c_type, "uint32_t");
                    if (strcmp(var_type.text, "f32") == 0) strcpy(c_type, "float");

                    fprintf(out_file, "    %s %s = %s;\n", c_type, var_name.text, val.text);
                }
                inner = lexer_next_token(&lexer);
            }
            fprintf(out_file, "    return 0;\n}\n\n");
        }
        tok = lexer_next_token(&lexer);
    }
}
