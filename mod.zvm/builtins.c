//fuck it
#include "vector.h"
#include "lexer.h"
#include "parse.h"
#include "builtins.h"

void format_float(char* out, float value) {
    char* p = out;
    char digits[16];
    int n = 0;

    if (value < 0.0f) {
        *p++ = '-';
        value = -value;
    }

    u32 whole = (u32)value;
    float frac = value - (float)whole;
    frac += 0.0000005f;

    if (frac >= 1.0f) {
        frac -= 1.0f;
        whole += 1;
    }

    if (whole == 0) digits[n++] = '0';
    while (whole > 0) {
        digits[n++] = (char)('0' + (whole % 10));
        whole /= 10;
    }
    while (n > 0) *p++ = digits[--n];

    *p++ = '.';
    for (int i = 0; i < 6; i++) {
        frac *= 10.0f;
        int d = (int)frac;
        if (d < 0) d = 0;
        if (d > 9) d = 9;
        *p++ = (char)('0' + d);
        frac -= (float)d;
    }
    *p = '\0';
}

static Value builtin_none(void) {
    Value v; v.kind = VAL_NONE; v.u = 0; return v;
}

static Value builtin_u32(u32 x) {
    Value v; v.kind = VAL_U32; v.u = x; return v;
}

static Value builtin_float(float x) {
    Value v; v.kind = VAL_FLOAT; v.f = x; return v;
}

static Value builtin_string(char* s) {
    Value v; v.kind = VAL_STRING; v.s = s; return v;
}

static int builtin_numeric(Value* v) {
    return v->kind == VAL_U32 || v->kind == VAL_FLOAT;
}

static float builtin_as_float(Value* v) {
    return v->kind == VAL_FLOAT ? v->f : (float)v->u;
}

static int builtin_less(Value* a, Value* b) {
    if (a->kind == VAL_FLOAT || b->kind == VAL_FLOAT)
        return builtin_as_float(a) < builtin_as_float(b);
    return a->u < b->u;
}

static char* builtin_copy_string(char* s) {
    size_t len = strlen(s);
    char* out = (char*)kalloc(len + 1);
    if (!out) return 0;
    memcpy(out, s, len + 1);
    return out;
}

static char* builtin_u32_string(u32 value) {
    char tmp[16];
    int n = 0;

    if (value == 0) tmp[n++] = '0';
    while (value > 0) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    char* out = (char*)kalloc(n + 1);
    if (!out) return 0;
    for (int i = 0; i < n; i++) out[i] = tmp[n - i - 1];
    out[n] = '\0';
    return out;
}

static int builtin_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int builtin_parse_int(char* str, u32* out) {
    if (!str) return 0;

    char* s = str;
    int negative = 0;
    int base = 10;

    if (*s == '-' || *s == '+') {
        negative = *s == '-';
        s++;
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X' ||
                        s[1] == 'c' || s[1] == 'C' ||
                        s[1] == 'b' || s[1] == 'B')) {
        if (s[1] == 'x' || s[1] == 'X') base = 16;
        else if (s[1] == 'c' || s[1] == 'C') base = 8;
        else base = 2;
        s += 2;
    }

    u32 value = 0;
    int digits = 0;

    for (; *s; s++) {
        int d = builtin_digit(*s);
        if (d < 0 || d >= base) return 0;
        if (value > (0xFFFFFFFFu - (u32)d) / (u32)base) return 0;
        value = value * (u32)base + (u32)d;
        digits++;
    }

    if (!digits) return 0;
    if (negative) value = 0u - value;

    *out = value;
    return 1;
}

static void builtin_print_value(Value* v) {
    if (!v) return;

    switch (v->kind) {
        case VAL_STRING:
            printf("%s", v->s ? v->s : "");
            break;
        case VAL_U32:
            printf("%u", v->u);
            break;
        case VAL_FLOAT: {
            char buf[64];
            format_float(buf, v->f);
            printf("%s", buf);
            break;
        }
        case VAL_NONE:
            printf("%s", "none");
            break;
        default:
            break;
    }
}

Value printb(Vector* args){
    Value none; none.kind = VAL_NONE; none.u = 0;

    if (!args || vec_len(args) < 1) return none;

    Value* fmtv = (Value*)vec_get(args, 0);
    if (fmtv->kind != VAL_STRING || !fmtv->s) {
        for (int i = 0; i < vec_len(args); i++) {
            if (i) printf("%c", ' ');
            builtin_print_value((Value*)vec_get(args, i));
        }
        return none;
    }

    char* fmt = fmtv->s;
    int argi = 1;

    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] == '\\' && fmt[i + 1]) {
            i++;
            if (fmt[i] == 'n') {
                printf("%c", '\n');
            } else if (fmt[i] == '\\') {
                printf("%c", '\\');
            } else {
                printf("%c", '\\');
                printf("%c", fmt[i]);
            }
            continue;
        }

        if (fmt[i] != '%') {
            printf("%c", fmt[i]);
            continue;
        }

        i++;
        if (!fmt[i]) break;

        switch (fmt[i]){
            case 's':{
                if (argi < vec_len(args)) {
                    Value* v = (Value*)vec_get(args, argi);
                    if (v->kind == VAL_STRING && v->s)
                        printf("%s", v->s);
                    else if (v->kind == VAL_U32)
                        printf("%u", v->u);
                }
                argi++;
                break;
            }
            case 'c':{
                if (argi < vec_len(args)) {
                    Value* v = (Value*)vec_get(args, argi);
                    u32 chr = (v->kind == VAL_U32) ? v->u : 0;
                    printf("%c", chr & 0xFF);
                }
                argi++;
                break;
            }
            case 'u':{
                if (argi < vec_len(args)) {
                    Value* v = (Value*)vec_get(args, argi);
                    u32 num = (v->kind == VAL_U32) ? v->u : 0;
                    printf("%u", num);
                }
                argi++;
                break;
            }
            case 'x':{
                if (argi < vec_len(args)) {
                    Value* v = (Value*)vec_get(args, argi);
                    u32 num = (v->kind == VAL_U32) ? v->u : 0;
                    printf("%x", num);
                }
                argi++;
                break;
            }
            case 'f':{
                if (argi < vec_len(args)) {
                    Value* v = (Value*)vec_get(args, argi);
                    float num = (v->kind == VAL_FLOAT) ? v->f : 0.0f;
                    if (v->kind == VAL_U32) num = (float)v->u;
                    char fbuf[64];
                    format_float(fbuf, num);
                    printf("%s", fbuf);
                }
                argi++;
                break;
            }
            case '%':{
                printf("%c", '%');
                break;
            }

            default:{
                printf("%c", '%');
                printf("%c", fmt[i]);
                break;
            }
        }
    }

    return none;
}

Value printlnb(Vector* args) {
    Value result = printb(args);
    printf("%c", '\n');
    return result;
}

Value lenb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    if (v->kind != VAL_STRING) return builtin_none();
    return builtin_u32((u32)strlen(v->s ? v->s : ""));
}

Value typeb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    switch (v->kind) {
        case VAL_U32:    return builtin_string("u32");
        case VAL_FLOAT:  return builtin_string("fp");
        case VAL_STRING: return builtin_string("str");
        default:         return builtin_string("none");
    }
}

Value strb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    char* s = 0;

    switch (v->kind) {
        case VAL_STRING:
            return builtin_string(v->s ? v->s : "");
        case VAL_U32:
            s = builtin_u32_string(v->u);
            break;
        case VAL_FLOAT: {
            char buf[64];
            format_float(buf, v->f);
            s = builtin_copy_string(buf);
            break;
        }
        case VAL_NONE:
            return builtin_string("none");
        default:
            return builtin_none();
    }

    if (!s) return builtin_none();
    return builtin_string(s);
}

Value intb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    switch (v->kind) {
        case VAL_U32:
            return *v;
        case VAL_FLOAT: {
            s32 value = (s32)v->f;
            return builtin_u32((u32)value);
        }
        case VAL_STRING: {
            u32 value;
            if (builtin_parse_int(v->s ? v->s : "", &value))
                return builtin_u32(value);
            return builtin_none();
        }
        default:
            return builtin_none();
    }
}

Value floatb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    switch (v->kind) {
        case VAL_U32:
            return builtin_float((float)v->u);
        case VAL_FLOAT:
            return *v;
        case VAL_STRING:
            return builtin_float(stf(v->s ? v->s : ""));
        default:
            return builtin_none();
    }
}

Value absb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    if (v->kind == VAL_U32) return *v;
    if (v->kind == VAL_FLOAT)
        return builtin_float(v->f < 0.0f ? -v->f : v->f);
    return builtin_none();
}

static Value builtin_minmax(Vector* args, int want_max) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* best = (Value*)vec_get(args, 0);
    if (!builtin_numeric(best)) return builtin_none();

    for (int i = 1; i < vec_len(args); i++) {
        Value* v = (Value*)vec_get(args, i);
        if (!builtin_numeric(v)) return builtin_none();

        if ((want_max && builtin_less(best, v)) ||
            (!want_max && builtin_less(v, best)))
            best = v;
    }

    return *best;
}

Value minb(Vector* args) {
    return builtin_minmax(args, 0);
}

Value maxb(Vector* args) {
    return builtin_minmax(args, 1);
}

Value chrb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    if (v->kind != VAL_U32) return builtin_none();

    char* s = (char*)kalloc(2);
    if (!s) return builtin_none();
    s[0] = (char)(v->u & 0xFF);
    s[1] = '\0';
    return builtin_string(s);
}

Value ordb(Vector* args) {
    if (!args || vec_len(args) < 1) return builtin_none();

    Value* v = (Value*)vec_get(args, 0);
    if (v->kind != VAL_STRING || !v->s || !v->s[0])
        return builtin_none();

    return builtin_u32((u32)(u8)v->s[0]);
}
