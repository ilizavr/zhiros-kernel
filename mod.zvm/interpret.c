#include "parse.h"
#include "lexer.h"
#include "vector.h"
#include "map.h"
#include "../kernel32/linker.h"
#include "builtins.h"
#include "../lib/string.h"


static Map* globals;
static Map* functions;
static Value retval;


static Control exec(AN* node);
static Value   eval(AN* node);
static Value   call_user(char* name, Vector* args);


static Value make_none(void) {
    Value v; v.kind = VAL_NONE; v.u = 0; return v;
}

static Value make_u32(u32 x) {
    Value v; v.kind = VAL_U32; v.u = x; return v;
}

static Value make_float(float x) {
    Value v; v.kind = VAL_FLOAT; v.f = x; return v;
}

static Value make_string(char* s) {
    Value v; v.kind = VAL_STRING; v.s = s; return v;
}

static int truthy(Value v) {
    switch (v.kind) {
        case VAL_U32:    return v.u != 0;
        case VAL_FLOAT:  return v.f != 0.0f;
        case VAL_STRING: return v.s && v.s[0] != '\0';
        default:         return 0;
    }
}

static int is_number(Value v) {
    return v.kind == VAL_U32 || v.kind == VAL_FLOAT;
}

static float as_float(Value v) {
    return (v.kind == VAL_FLOAT) ? v.f : (float)v.u;
}

static Value* env_get(Map* env, char* name) {
    void* val = map_get(env,name);
    return (Value*)val;
}

static void env_set(Map* env, char* name, Value* v) {
    map_put(env,name,v);
}

static void env_bind(Map* env, char* name, Value v) {
    if (!env || !name || !kalloc) return;
    Value* slot = (Value*)kalloc(sizeof(Value));
    if (!slot) return;
    *slot = v;
    env_set(env, name, slot);
}

static int builtin_id(char* name) {
    if (!name) return 0;

    if (!strcmp(name, "print"))   return 1;
    if (!strcmp(name, "println")) return 2;
    if (!strcmp(name, "len"))     return 3;
    if (!strcmp(name, "type"))    return 4;
    if (!strcmp(name, "str"))     return 5;
    if (!strcmp(name, "int"))     return 6;
    if (!strcmp(name, "float"))   return 7;
    if (!strcmp(name, "fp"))      return 8;
    if (!strcmp(name, "abs"))     return 9;
    if (!strcmp(name, "min"))     return 10;
    if (!strcmp(name, "max"))     return 11;
    if (!strcmp(name, "chr"))     return 12;
    if (!strcmp(name, "ord"))     return 13;
    return 0;
}

static Value call_builtin(int id, Vector* args) {
    switch (id) {
        case 1:  return printb(args);
        case 2:  return printlnb(args);
        case 3:  return lenb(args);
        case 4:  return typeb(args);
        case 5:  return strb(args);
        case 6:  return intb(args);
        case 7:  return floatb(args);
        case 8:  return floatb(args);
        case 9:  return absb(args);
        case 10: return minb(args);
        case 11: return maxb(args);
        case 12: return chrb(args);
        case 13: return ordb(args);
        default: return make_none();
    }
}

static Value eval_binop(Value l, Value r, const char* op) {
    if (!op) return make_none();

    if (!strcmp(op, "&&")) return make_u32(truthy(l) && truthy(r));
    if (!strcmp(op, "||")) return make_u32(truthy(l) || truthy(r));

    if (!strcmp(op, "==") || !strcmp(op, "!=")) {
        int eq;
        if (l.kind == VAL_STRING || r.kind == VAL_STRING) {
            eq = (l.kind == VAL_STRING && r.kind == VAL_STRING)
                     ? (strcmp(l.s ? l.s : "", r.s ? r.s : "") == 0)
                     : 0;
        } else if (l.kind == VAL_FLOAT || r.kind == VAL_FLOAT) {
            eq = is_number(l) && is_number(r) && (as_float(l) == as_float(r));
        } else {
            eq = (l.u == r.u);
        }
        return make_u32((!strcmp(op, "==")) ? eq : !eq);
    }

    if (!strcmp(op, "+") && l.kind == VAL_STRING && r.kind == VAL_STRING) {
        char* a = l.s ? l.s : "";
        char* b = r.s ? r.s : "";
        size_t n1 = strlen(a);
        size_t n2 = strlen(b);
        char* out = kalloc ? (char*)kalloc(n1 + n2 + 1) : 0;
        if (!out) return make_none();
        memcpy(out, a, n1);
        memcpy(out + n1, b, n2 + 1);
        return make_string(out);
    }

    if (l.kind == VAL_FLOAT || r.kind == VAL_FLOAT) {
        if (!is_number(l) || !is_number(r)) return make_none();
        float a = as_float(l);
        float b = as_float(r);
        if (!strcmp(op, "+")) return make_float(a + b);
        if (!strcmp(op, "-")) return make_float(a - b);
        if (!strcmp(op, "*")) return make_float(a * b);
        if (!strcmp(op, "/")) {
            if (b == 0.0f) return make_none();
            return make_float(a / b);
        }
        if (!strcmp(op, "<")) return make_u32(a < b);
        if (!strcmp(op, ">")) return make_u32(a > b);
        if (!strcmp(op, "<=")) return make_u32(a <= b);
        if (!strcmp(op, ">=")) return make_u32(a >= b);
        return make_none();
    }

    if (l.kind == VAL_U32 && r.kind == VAL_U32) {
        u32 a = l.u;
        u32 b = r.u;
        if (!strcmp(op, "+")) return make_u32(a + b);
        if (!strcmp(op, "-")) return make_u32(a - b);
        if (!strcmp(op, "*")) return make_u32(a * b);
        if (!strcmp(op, "/")) {
            if (!b) return make_none();
            return make_u32(a / b);
        }
        if (!strcmp(op, "%")) {
            if (!b) return make_none();
            return make_u32(a % b);
        }
        if (!strcmp(op, "<")) return make_u32(a < b);
        if (!strcmp(op, ">")) return make_u32(a > b);
        if (!strcmp(op, "<=")) return make_u32(a <= b);
        if (!strcmp(op, ">=")) return make_u32(a >= b);
        if (!strcmp(op, "<<")) return make_u32(b >= 32 ? 0 : a << b);
        if (!strcmp(op, ">>")) return make_u32(b >= 32 ? 0 : a >> b);
        if (!strcmp(op, "^")) return make_u32(a ^ b);
        if (!strcmp(op, "&")) return make_u32(a & b);
        if (!strcmp(op, "|")) return make_u32(a | b);
        return make_none();
    }

    return make_none();
}

static Value call_user(char* name, Vector* args) {
    if (!functions || !name) return make_none();

    AN* fn = (AN*)map_get(functions, name);
    if (!fn || fn->type != AST_FUNC) return make_none();

    Map* saved_globals = globals;
    Value saved_retval = retval;

    globals = make_map_parent(saved_globals);
    retval = make_none();

    Vector* params = fn->func_node.args;
    int nparams = params ? vec_len(params) : 0;
    int nargs = args ? vec_len(args) : 0;

    for (int i = 0; i < nparams; i++) {
        AN* p = (AN*)vec_get(params, i);
        if (!p || p->type != AST_IDENT) continue;
        Value v = (i < nargs) ? *(Value*)vec_get(args, i) : make_none();
        env_bind(globals, p->ident.name, v);
    }

    exec(fn->func_node.body);

    Value result = fn->func_node.is_proc ? make_none() : retval;

    globals = saved_globals;
    retval = saved_retval;

    return result;
}


static Control exec(AN* node) {
    if (!node) return CTRL_NONE;

    switch (node->type) {
        case AST_VARAR:
            //XXX: NOW IT MUST NOT BE IMPLEMENTED.
            return CTRL_NONE;

        case AST_ASSIGN: {
            Value v = eval(node->assign.value);
            env_bind(globals, node->assign.name, v);
            return CTRL_NONE;
        }

        case AST_EXPR_ASSIGN: {
            AN* target = node->ast_assign.name;
            Value v = eval(node->ast_assign.value);

            if (target && target->type == AST_IDENT) {
                env_bind(globals, target->ident.name, v);
            } else if (target && target->type == AST_INDEX) {
                //XXX: arrays/indexing are not implemented yet
            }
            return CTRL_NONE;
        }

        case AST_RETURN:
            retval = eval(node->return_node.value);
            return CTRL_RETURN;

        case AST_BLOCK: {
            Vector* stmts = node->block_node.stmts;
            if (!stmts) return CTRL_NONE;
            for (int i = 0; i < vec_len(stmts); i++) {
                Control c = exec((AN*)vec_get(stmts, i));
                if (c != CTRL_NONE) return c;
            }
            return CTRL_NONE;
        }

        case AST_IF: {
            Value cond = eval(node->if_node.cond);
            if (truthy(cond)) return exec(node->if_node.then);
            if (node->if_node.else_) return exec(node->if_node.else_);
            return CTRL_NONE;
        }

        case AST_WHILE: {
            for (;;) {
                if (!truthy(eval(node->while_node.cond))) break;
                Control c = exec(node->while_node.body);
                if (c == CTRL_RETURN) return c;
                if (c == CTRL_BREAK) break;
                if (c == CTRL_CONTINUE) continue;
            }
            return CTRL_NONE;
        }

        case AST_FUNC:
            if (!functions) functions = make_map();
            map_put(functions, node->func_node.ident, node);
            return CTRL_NONE;

        case AST_CALL:
            (void)eval(node);
            return CTRL_NONE;

        default:
            (void)eval(node);
            return CTRL_NONE;
    }
}


static Value eval(AN* node) {
    if (!node) return make_none();

    switch (node->type) {
        case AST_NUMBER: {
            Value v; v.kind = VAL_U32; v.u = (u32)node->number.value; return v;
        }

        case AST_FL_NUMBER:{
            Value v; v.kind = VAL_FLOAT;v.f = (float)node->float_number_node.float_num;return v;
       }

        case AST_STRING: {
            Value v; v.kind = VAL_STRING; v.s = node->string.value; return v;
        }

        case AST_IDENT: {
            Value* v = env_get(globals, node->ident.name);
            return v ? *v : make_none();
        }

        case AST_PAREN_OP:
            return eval(node->paren_expr.expr);

        case AST_UNOP: {
            Value a = eval(node->unop.operand);
            const char* op = node->unop.op;
            if (!op) return make_none();

            if (!strcmp(op, "+")) return a;

            if (!strcmp(op, "-")) {
                if (a.kind == VAL_U32)   return make_u32(0u - a.u);
                if (a.kind == VAL_FLOAT) return make_float(-a.f);
                return make_none();
            }

            if (!strcmp(op, "!")) return make_u32(!truthy(a));

            if (!strcmp(op, "~")) {
                if (a.kind == VAL_U32) return make_u32(~a.u);
                return make_none();
            }

            return make_none();
        }

        case AST_BINOP: {
            Value l = eval(node->binop.left);
            Value r = eval(node->binop.right);
            return eval_binop(l, r, node->binop.op);
        }

        case AST_INDEX: {
	    //XXX: NOW IT SHOULD'NT BE IMPLEMENTED
            return make_none();
        }

        case AST_CALL: {
            AN* callee = node->call_node.name;

            if (!callee || callee->type != AST_IDENT) return make_none();

            char* name = callee->ident.name;
            Vector* argan = node->call_node.args;
            Vector* argv = make_vector();

            if (!kalloc) return make_none();
            if (argan) {
                for (int i = 0; i < vec_len(argan); i++) {
                    AN* arg = (AN*)vec_get(argan, i);
                    Value* slot = (Value*)kalloc(sizeof(Value));
                    if (!slot) return make_none();
                    *slot = eval(arg);
                    vec_push(argv, slot);
                }
            }

            int id = builtin_id(name);
            if (id)
                return call_builtin(id, argv);

            return call_user(name, argv);
        }

        default:
            return make_none();
    }
}


void interpret(Vector* program) {
    if (!program) return;

    globals = make_map();
    functions = make_map();
    retval = make_none();

    for (int i = 0; i < vec_len(program); i++) {
        Control c = exec((AN*)vec_get(program, i));
        if (c == CTRL_RETURN) break;
    }
}
