#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// unsafe shit
// beware

typedef struct _StringBuilder {
    size_t sLength, sCap;
    char *str;
} StringBuilder;

int sb_init(StringBuilder *sb) {
    char *str = (char *)calloc(2, 1);

    if(!str) {
        puts("Not enough memory for token(sb init failed).");
        return -1;
    }

    *sb = (StringBuilder) {
        .sCap = 2,
        .sLength = 0,
        .str = str,
    };

    return 0;
}

int sb_add(StringBuilder *sb, char ch) {
    if((sb->sLength + 2) >= sb->sCap) { // yeah we need generics, and I hate preprocessor
        size_t new_cap = sb->sCap + (sb->sCap >> 1);
        char *new_str = (char *)realloc(sb->str, new_cap);
        if(!new_str) {
            puts("Not enough memory for token(sb failed).");
            free(sb->str);
            return -1;
        }
        sb->sCap = new_cap;
        sb->str = new_str;
    }

    sb->str[sb->sLength++] = ch;
    sb->str[sb->sLength] = 0;

    return 0;
}

int sb_finalize(StringBuilder *sb, char **output) {
    if(sb->sCap != sb->sLength) { // omg it's shrinking
        char *new_str = (char *)realloc(sb->str, sb->sLength + 1);
        if(!new_str) {
            puts("Strangely unable to shrink sb's cstr...");
            return -1;
        }
        *output = new_str;
        return 0;
    }

    *output = sb->str; // lol really

    return 0;
}

typedef enum _TokenType {
    TokLiteralCharacter = -1,
    TokIdentifier = -2,
    TokMacro = -3,
    TokComment = -4,
    TokLiteralString = -5,
    TokLiteralInteger = -6,
    TokLiteralFloat = -7,
} TokenType;

typedef struct _Token {
    enum _TokenType eType;
    char *strData;
} Token;

typedef struct _TokenArray {
    size_t sLength, sCap;
    struct _Token *tTokens;
} TokenArray;

void print_tokens(TokenArray *tarr) {
    printf("Token Array [%zu/%zu]:\n", tarr->sLength, tarr->sCap);
    for(size_t i = 0; i < tarr->sLength; i++) {
        Token tok = tarr->tTokens[i];

        printf("[%d %p]\n", tok.eType, tok.strData);

        if(tok.eType > 0) {
            printf("Casual '%c'\n", (char)tok.eType);
        } else if(tok.eType == TokLiteralCharacter) {
            printf("Character '%c'\n", (char)tok.strData);
        } else {
            switch(tok.eType) {
                case TokIdentifier:
                    fputs("Identifier ", stdout);
                    break;
                case TokMacro:
                    fputs("Macro ", stdout);
                    break;
                case TokComment:
                    fputs("Comment ", stdout);
                    break;
                case TokLiteralString:
                    fputs("String ", stdout);
                    break;
                case TokLiteralInteger:
                    fputs("Integer ", stdout);
                    break;
                case TokLiteralFloat:
                    fputs("Float ", stdout);
                    break;
            }
            fputs("--------------------\n", stdout);
            fputs(tok.strData, stdout);
            fputs("\n-------------------------------\n", stdout);
        }
    }
}

// imagine world without functions
int tokarr_add(TokenArray *tarrTokens, Token tToken) {
    if(tarrTokens->sLength == tarrTokens->sCap) {
        size_t new_cap = tarrTokens->sCap + (tarrTokens->sCap >> 1);
        Token *new_tokens = (Token *)realloc(tarrTokens->tTokens, new_cap * sizeof(Token));
        if(!new_tokens) {
            puts("Not enough memory for tokens array.");
            return -1;
        }
        tarrTokens->tTokens = new_tokens;
        tarrTokens->sCap = new_cap;
    }

    tarrTokens->tTokens[tarrTokens->sLength++] = tToken;

    return 0;
}

void tokarr_free_insides(TokenArray *tarr) {
    for(size_t i = 0; i < tarr->sLength; i++)
        if(tarr->tTokens[i].eType < -1)
            free(tarr->tTokens[i].strData);
}

int parse_tokid(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    
    while((**strInputText) != 0) {
        char current_char = **strInputText;
        if((current_char == '_') || isalpha(current_char) || isdigit(current_char)) {
            if(sb_add(&sb, current_char)) {
                return -1;
            }
        } else {
            *strInputText -= 1;
            goto ptk_done;
        }
        
        *strInputText += 1;
    }

    ptk_done:
    Token tok;
    tok.eType = TokIdentifier;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

int parse_toknum(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    int is_dot = 0;
    
    while((**strInputText) != 0) {
        char current_char = **strInputText;
        if(isdigit(current_char)) {
            if(sb_add(&sb, current_char)) {
                return -1;
            }
        } else if (current_char == '.') {
            if(is_dot) {
                puts("More than one dot in number format.");
                free(sb.str);
                return -1;
            } else {
                is_dot = 1;
                if(sb_add(&sb, current_char)) {
                    return -1;
                }
            }
        } else {
            *strInputText -= 1;
            goto ptk_done;
        }
        
        *strInputText += 1;
    }

    ptk_done:
    Token tok;
    tok.eType = (is_dot) ? TokLiteralFloat : TokLiteralInteger;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

int parse_tokmacro(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    (*strInputText)++;
    
    while((**strInputText) != 0) {
        char current_char = **strInputText;
        if(current_char != '$') {
            if(sb_add(&sb, current_char)) {
                return -1;
            }
        } else {
            goto ptm_done;
        }
        
        *strInputText += 1;
    }

    ptm_done:
    Token tok;
    tok.eType = TokMacro;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

int parse_tokstr(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    char escape_flag = 0;

    (*strInputText)++;
    
    while((**strInputText) != 0) {
        char current_char = **strInputText;

        if(current_char != '"') {
            if(current_char == '\\') {
                escape_flag = 1;
            } else {
                if(escape_flag)
                    escape_flag = 0;
                if(sb_add(&sb, current_char)) {
                    return -1;
                }
            }
        } else {
            if(escape_flag) {
                if(sb_add(&sb, current_char)) {
                    return -1;
                }
                escape_flag = 0;
            } else
                goto ptm_done;
        }
        
        *strInputText += 1;
    }

    ptm_done:
    Token tok;
    tok.eType = TokLiteralString;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

int parse_tokchar(char **strInputText, TokenArray *tarrOutput) {
    (*strInputText)++;

    char curr_ch = **strInputText;
    char ch;

    if(curr_ch == '\\') {
        ch = *(++*strInputText);
        curr_ch = *(++*strInputText);
    } else {
        ch = curr_ch;
    }

    curr_ch = *(++*strInputText);

    if(curr_ch == '\'') {
        goto ptch_done;
    } else {
        puts("Bad char format.");
        return -1;
    }

    ptch_done:
    Token tok;
    tok.eType = TokLiteralCharacter;
    tok.strData = (char *)ch;

    return tokarr_add(tarrOutput, tok);
}

int parse_tokcomm1(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    (*strInputText)++;
    
    while((**strInputText) != 0) {
        char current_char = **strInputText;
        if(current_char != '\n') {
            if(sb_add(&sb, current_char)) {
                return -1;
            }
        } else {
            goto ptc1_done;
        }
        
        *strInputText += 1;
    }

    ptc1_done:
    Token tok;
    tok.eType = TokComment;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

int parse_tokcomm2(char **strInputText, TokenArray *tarrOutput) {
    StringBuilder sb;
    if(sb_init(&sb))
        return -1;

    char last_check = 0;

    (*strInputText)++;
    
    while((**strInputText) != 0) {
        char current_char = **strInputText;
        if(last_check) {
            if(current_char == '/') {
                goto ptc2_done;
            } else {
                if(sb_add(&sb, '*')) {
                    return -1;
                }
                last_check = 0;
            }
        }

        if(current_char != '*') {
            if(sb_add(&sb, current_char)) {
                return -1;
            }
        } else {
            last_check = 1;
        }
        
        *strInputText += 1;
    }

    ptc2_done:
    Token tok;
    tok.eType = TokComment;

    int sbf = sb_finalize(&sb, &tok.strData);
    if(sbf) {
        return -1;
    }

    return tokarr_add(tarrOutput, tok);
}

inline static int parse_casual(char **strInputText, TokenArray *tarrOutput) {
    return tokarr_add(tarrOutput, (Token) {
        .eType = **strInputText,
        .strData = NULL,
    });
}

int parse_tokens(char *strInputText, TokenArray *tarrOutput) {
    Token *toks = (Token *)malloc(sizeof(*toks) * 2);

    if(!toks) {
        puts("Not enough memory for tokens array init.");
        return -1;
    }

    TokenArray tarr = (TokenArray) {
        .sCap = 2,
        .sLength = 0,
        .tTokens = toks,
    };

    char is_failure = 0;
    char comm_check = 0;
    char current_char = *strInputText;

    while(current_char != 0) {
        if(!isspace(current_char)) {
            int result = 0;

            if(comm_check == 1) {
                if(current_char == '/') {
                    result = parse_tokcomm1(&strInputText, &tarr);
                    comm_check = 0;
                } else if(current_char == '*') {
                    result = parse_tokcomm2(&strInputText, &tarr);
                    comm_check = 0;
                } else {
                    comm_check = 2;
                    strInputText -= 2;
                }
            } else if(isdigit(current_char)) {
                result = parse_toknum(&strInputText, &tarr);
            } else if((current_char == '_') || isalpha(current_char)) {
                result = parse_tokid(&strInputText, &tarr);
            } else if(current_char == '\'') {
                result = parse_tokchar(&strInputText, &tarr);
            } else if(current_char == '"') {
                result = parse_tokstr(&strInputText, &tarr);
            } else if(current_char == '$') {
                result = parse_tokmacro(&strInputText, &tarr);
            } else if(current_char == '/') {
                if(!comm_check) {
                    comm_check = 1;
                } else {
                    result = parse_casual(&strInputText, &tarr);
                }
            } else {
                result = parse_casual(&strInputText, &tarr);
            }

            if(result == 1) {
                break;
            } else if(result < 0) {
                is_failure = 1;
                break;
            }
        }
        current_char = *(++strInputText);
    }

    // if parsing failed
    if(is_failure) {
        tokarr_free_insides(&tarr);
        return -1;
    } else {
        *tarrOutput = tarr;
    }

    return 0;
}

void translate(FILE *in, FILE *out) {
    long infsize;

    // get input file size(does not work if the file is too big)
    fseek(in, 0L, SEEK_END);
    infsize = ftell(in);
    fseek(in, 0L, SEEK_SET);

    // reading the input file
    char *input_text = (char *)malloc(infsize + 1);

    if(!input_text) {
        puts("Error. Not enough memory(input_text).");
        return;
    }

    input_text[infsize] = 0;

    fread(input_text, 1, infsize, in);

    // parsing the tokens
    TokenArray tokens;

    if(parse_tokens(input_text, &tokens)) {
        free(input_text); // that's why **deref** is needed
        puts("Error. Token parse failure.");
        return;
    }

    print_tokens(&tokens);

    // freeing zone:

    free(input_text);
    
    // freeing the tokens
    tokarr_free_insides(&tokens);
    free(tokens.tTokens);

    puts("Successufully translated.");
}

int main(int argc, char **argv) {
    if(argc != 3) {
        puts("Usage: bred <input> <output>");
        return 0;
    }

    char *input_path = argv[1];
    char *output_path = argv[2];

    FILE *in = fopen(input_path, "rb");

    if(!in) {
        puts("Couldn't open input file.");
        return 0;
    }

    FILE *out = fopen(output_path, "wb");

    if(!out) {
        fclose(in);
        puts("Couldn't open output file.");
        return 0;
    }

    translate(in, out);

    fclose(in);
    fclose(out);

    return 0;
}
