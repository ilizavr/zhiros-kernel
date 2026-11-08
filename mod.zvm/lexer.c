#include "zhirvm_types.h"
#include "../lib/string.h"
#include "../lib/zhirtypes.h"
#include "../lib/zhirapi.h"
#include "parse.h"
#include "lexer.h"
#include "interpret.h"

void* (*kalloc)(u32 size) = 0;

int read_count = 0;
char* buf = 0;

int is_digit(char digit) {
    return digit >= '0' && digit <= '9';
}

int is_alpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int is_sspace(char c) {
    return c == ' '  || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int is_number_buf(char* buf) {
    for(int i = 0;i < strlen(buf);i++) {
        if(!is_digit(buf[i])) return 0;
    }
    return 1;
}

void init_string(struct file* file) {
    int size = file->getsize(file);
    buf = (char*)kalloc(size+1);
    file->read(file,buf,size,0);
    buf[size] = '\0';
}

Token* make_token_number(int val) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = number;
token->number.val = val;

return token;
}

Token* make_token_float_number(float val) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = float_number;
token->float_number.val = val;

return token;
}

Token* make_token_identifier(char* name) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = identifier;
token->identifier.name = name;

return token;
}



Token* make_token_string(char* val) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = string;
token->string.val = val;

return token;
}


Token* make_token_keyword(int kind) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = kind;
token->keyword.kind = kind;

return token;
}

Token* make_token_operation(int op) {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = op;
token->operation.op = op;

return token;
}


Token* make_token_null() {
Token* token = (Token*)kalloc(sizeof(Token) * 1);
token->kind = token_eof;
token->null.not_actual = NOT_ACTUAL;

return token;
}





char* read_string(void) {
    while (*buf && is_sspace(*buf)) buf++;
    if (!*buf) return 0;

    char* token = buf;

    if (*buf == '"') {
        buf++;
        while (*buf && *buf != '"') buf++;
        if (*buf == '"') buf++;
        if (*buf) { *buf = '\0'; buf++; }
        return token;
    }

    while (*buf && !is_sspace(*buf)) buf++;
    if (*buf) { *buf = '\0'; buf++; }
    return token;
}

int is_identifier_buf(char* buf) {
    for(int i = 0;i < strlen(buf);i++) {
        if(is_sspace(buf[i])) return 0;
        if(i == 0 && is_number_buf(buf)) return 0;
        if(!is_alpha((int)buf[i])) return 0;
    }
    return 1;
}

int sti(char* buf) {
    int r = 0;
    for(int i = 0;i < strlen(buf);i++) {
        if(!is_digit(buf[i])) return NOT_ACTUAL;
        if(r > (INT_MAX - (buf[i] - '0') / 10)) return NOT_ACTUAL;
        r = r * 10 + (buf[i] - '0');
    }

   return r;
}

static int is_base_digit(char c, int base) {
    int d = -1;

    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;

    return d >= 0 && d < base;
}

static int st_base(char* buf, int base, u32* out) {
    u32 value = 0;
    int digits = 0;

    for (; *buf; buf++) {
        int d;
        if (*buf >= '0' && *buf <= '9') d = *buf - '0';
        else if (*buf >= 'a' && *buf <= 'f') d = *buf - 'a' + 10;
        else if (*buf >= 'A' && *buf <= 'F') d = *buf - 'A' + 10;
        else return 0;

        if (d >= base) return 0;
        if (value > (0xFFFFFFFFu - (u32)d) / (u32)base) return 0;

        value = value * (u32)base + (u32)d;
        digits++;
    }

    if (!digits) return 0;

    *out = value;
    return 1;
}
float stf(char * buf) {
    float value = 0.0f;
    float scale = 0.1f;
    int exponent = 0;
    int exp_sign = 1;

    for (; is_digit(*buf); buf++)
        value = value * 10.0f + (float)(*buf - '0');

    if (*buf == '.') {
        buf++;
        for (; is_digit(*buf); buf++) {
            value += (float)(*buf - '0') * scale;
            scale *= 0.1f;
        }
    }

    if (*buf == 'e' || *buf == 'E') {
        buf++;
        if (*buf == '+') {
            buf++;
        } else if (*buf == '-') {
            exp_sign = -1;
            buf++;
        }
        for (; is_digit(*buf); buf++)
            exponent = exponent * 10 + (*buf - '0');
    }

    while (exponent-- > 0)
        value = exp_sign > 0 ? value * 10.0f : value / 10.0f;

    return value;
    }


char* get_string(char* buf) {
    char* res = (char*)kalloc(strlen(buf) + 1);
    int i = 0;
    for(;i <strlen(buf);i++) {
        if(buf[i] == '"') break;
        res[i] = buf[i];
    }
    res[i] = '\0';

    return res;
}

char* get_identifier(char* buf) {
    char* res = (char*)kalloc(strlen(buf) + 1);
    int i = 0;
    for(;i <strlen(buf);i++) {
        if(is_sspace(buf[i])) break;
        res[i] = buf[i];
    }
    res[i] = '\0';

    return res;
}

static Token* classify_word(char* w) {
    if(!strcmp(w,"u32")) return make_token_keyword(u32_type);
    else if(!strcmp(w,"u16")) return make_token_keyword(u16_type);
    else if(!strcmp(w,"u8")) return make_token_keyword(u8_type);
    else if(!strcmp(w,"str")) return make_token_keyword(string_type);
    else if(!strcmp(w,"fp")) return make_token_keyword(float_type);
    else if(!strcmp(w,"if")) return make_token_keyword(if_keyword);
    else if(!strcmp(w,"udf")) return make_token_keyword(udf_keyword);
    else if(!strcmp(w,"udp")) return make_token_keyword(udp_keyword);
    else if(!strcmp(w,"whl")) return make_token_keyword(while_keyword);
    else if(!strcmp(w,"else")) return make_token_keyword(else_keyword);
    else if(!strcmp(w,"ret") || !strcmp(w,"return")) return make_token_keyword(return_keyword);
    else if(!strcmp(w,"var")) return make_token_keyword(var_keyword);
    else if(!strcmp(w,"varar")) return make_token_keyword(varar_keyword);
    else if(!strcmp(w,"call")) return make_token_keyword(call_keyword);
    else if(!strcmp(w,"class")) return make_token_keyword(class_keyword);
    else if(!strcmp(w,"reg")) return make_token_keyword(register_keyword);
    else if(!strcmp(w,"goto")) return make_token_keyword(goto_keyword);

    if(is_number_buf(w)) {
        int num = sti(w);
        if(num != NOT_ACTUAL) return make_token_number(num);
        return make_token_null();
    }

    if(is_identifier_buf(w)) {
        Token* token = (Token*)kalloc(sizeof(Token) * 1);
        token->kind = identifier;
        token->identifier.name = w;
        return token;
    }

    return make_token_null();
}

Token* do_lex(void) {
    for (;;) {
        while (*buf && is_sspace(*buf)) buf++;
        if (!*buf) return make_token_null();

        char c = *buf;

        if (c == '/' && buf[1] == '/') {
            while (*buf && *buf != '\n') buf++;
            continue;
        }
        
        if (c == '/' && buf[1] == '*') {
            buf += 2;
            while (*buf && !(*buf == '*' && buf[1] == '/')) buf++;
            if (*buf) buf += 2;
            continue;
        }

        if (c == '"') {
            buf++;
            char* start = buf;
            while (*buf && *buf != '"') buf++;
            int len = (int)(buf - start);
            char* res = (char*)kalloc(len + 1);
            for (int i = 0; i < len; i++) res[i] = start[i];
            res[len] = '\0';
            if (*buf == '"') buf++;
            return make_token_string(res);
        }

        if (c == '&' && buf[1] == '&') { buf += 2; return make_token_operation(logical_and); }
        if (c == '|' && buf[1] == '|') { buf += 2; return make_token_operation(logical_or); }
        if (c == '=' && buf[1] == '=') { buf += 2; return make_token_operation(equals); }
        if (c == '!' && buf[1] == '=') { buf += 2; return make_token_operation(dont_equal); }
        if (c == '<' && buf[1] == '<' && buf[2] == '=') { buf += 3; return make_token_operation(shift_left_assign); }
        if (c == '>' && buf[1] == '>' && buf[2] == '=') { buf += 3; return make_token_operation(shift_right_assign); }
        if (c == '<' && buf[1] == '=') { buf += 2; return make_token_operation(lesser_or_equal); }
        if (c == '>' && buf[1] == '=') { buf += 2; return make_token_operation(greater_or_equal); }
        if (c == '<' && buf[1] == '<') { buf += 2; return make_token_operation(shift_left); }
        if (c == '>' && buf[1] == '>') { buf += 2; return make_token_operation(shift_right); }
        if (c == '+' && buf[1] == '=') { buf += 2; return make_token_operation(plus_assign); }
        if (c == '-' && buf[1] == '=') { buf += 2; return make_token_operation(minus_assign); }
        if (c == '*' && buf[1] == '=') { buf += 2; return make_token_operation(multiply_assign); }
        if (c == '/' && buf[1] == '=') { buf += 2; return make_token_operation(divide_assign); }
        if (c == '%' && buf[1] == '=') { buf += 2; return make_token_operation(modulo_assign); }
        if (c == '&' && buf[1] == '=') { buf += 2; return make_token_operation(bit_and_assign); }
        if (c == '|' && buf[1] == '=') { buf += 2; return make_token_operation(bit_or_assign); }
        if (c == '^' && buf[1] == '=') { buf += 2; return make_token_operation(bit_xor_assign); }

        if (c == '0' && (buf[1] == 'x' || buf[1] == 'X' ||
                         buf[1] == 'c' || buf[1] == 'C' ||
                         buf[1] == 'b' || buf[1] == 'B')) {
            int base = 10;
            if (buf[1] == 'x' || buf[1] == 'X') base = 16;
            else if (buf[1] == 'c' || buf[1] == 'C') base = 8;
            else base = 2;

            buf += 2;
            char* digits_start = buf;
            while (is_base_digit(*buf, base)) buf++;

            if (buf == digits_start || is_digit(*buf) || is_alpha(*buf) || *buf == '_') {
                while (*buf && (is_digit(*buf) || is_alpha(*buf) || *buf == '_')) buf++;
                return make_token_null();
            }

            int len = (int)(buf - digits_start);
            char* word = (char*)kalloc(len + 1);
            for (int i = 0; i < len; i++) word[i] = digits_start[i];
            word[len] = '\0';

            u32 value;
            if (!st_base(word, base, &value)) return make_token_null();
            return make_token_number((int)value);
        }

        if (is_digit(c) || (c == '.' && is_digit(buf[1]))) {
            char* start = buf;
            int is_float = 0;

            if (c == '.') {
                is_float = 1;
                buf++;
            }

            while (is_digit(*buf)) buf++;

            if (!is_float && *buf == '.' && is_digit(buf[1])) {
                is_float = 1;
                buf++;
                while (is_digit(*buf)) buf++;
            }

            if (*buf == 'e' || *buf == 'E') {
                int j = 1;
                if (buf[j] == '+' || buf[j] == '-') j++;
                if (is_digit(buf[j])) {
                    is_float = 1;
                    buf += j;
                    while (is_digit(*buf)) buf++;
                }
            }

            if (is_alpha(*buf) || *buf == '_') {
                while (*buf && (is_digit(*buf) || is_alpha(*buf) || *buf == '_' || *buf == '.')) buf++;
                return make_token_null();
            }

            int len = (int)(buf - start);
            char* word = (char*)kalloc(len + 1);
            for (int i = 0; i < len; i++) word[i] = start[i];
            word[len] = '\0';

            if (is_float) return make_token_float_number(stf(word));

            u32 num;
            if (st_base(word, 10, &num)) return make_token_number((int)num);
            return make_token_null();
        }

        switch (c) {
            case '(': buf++; return make_token_operation(left_paren);
            case ')': buf++; return make_token_operation(right_paren);
            case '{': buf++; return make_token_operation(left_brace);
            case '}': buf++; return make_token_operation(right_brace);
            case '[': buf++; return make_token_operation(left_quad_brace);
            case ']': buf++; return make_token_operation(right_quad_brace);
            case ',': buf++; return make_token_operation(comma);
            case '.': buf++; return make_token_operation(dot);
            case ':': buf++; return make_token_operation(colon);
            case '+': buf++; return make_token_operation(plus);
            case '-': buf++; return make_token_operation(minus);
            case '*': buf++; return make_token_operation(multiply);
            case '/': buf++; return make_token_operation(divide);
            case '%': buf++; return make_token_operation(modulo);
            case '^': buf++; return make_token_operation(bit_xor);
            case '~': buf++; return make_token_operation(bit_not);
            case '&': buf++; return make_token_operation(bit_and);
            case '|': buf++; return make_token_operation(bit_or);
            case '=': buf++; return make_token_operation(assign);
            case '<': buf++; return make_token_operation(lesser_than);
            case '>': buf++; return make_token_operation(greater_than);
            case '!': buf++; return make_token_operation(logical_not);
        }

        if (is_alpha(c) || c == '_') {
            char* start = buf;
            while (*buf && (is_digit(*buf) || is_alpha(*buf) || *buf == '_')) buf++;
            int len = (int)(buf - start);
            char* word = (char*)kalloc(len + 1);
            for (int i = 0; i < len; i++) word[i] = start[i];
            word[len] = '\0';
            return classify_word(word);
        }

        buf++;
    }
}

static int loaded = 0;

Token* lex(struct file* file) {
    if (!loaded) {
        init_string(file);
        loaded = 1;
    }
    return do_lex();
}

static void zvm_exec(char disk_letter, char *name)
{
    loaded = 0;
    read_count = 0;
    buf = 0;
    nodes = 0;

    struct file* foo = open(disk_letter,name);
    if (!foo) return;

    parse(foo);
    interpret(nodes);
}

INIT void init(void* _resolve_function(char* name)) {
    api_init(_resolve_function);
    kalloc = alloc;

    register_function("_zvm",zvm_exec, "_zvm(char disk_letter, char* name) -> None");
}
