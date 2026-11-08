#include "parse.h"
#include "lexer.h"
#include "vector.h"
#include "builtins.h"

Vector* nodes = {0};


AN* make_ast() {
    AN* node = (AN*)kalloc(sizeof(AN));
    return node;
}

AN* make_ast_assign(char* name, AN* val) {
    AN* n = make_ast();
    n->type = AST_ASSIGN;
    n->assign.name = name;
    n->assign.value = val;
    
    return n;
}

AN* make_ast_nassign(AN* name, AN* val) {
    AN* n = make_ast();
    n->type = AST_EXPR_ASSIGN;
    n->ast_assign.name = name;
    n->ast_assign.value = val;
    return n;
}

AN* make_ast_if(AN* cond, AN* then, AN* else_) {
    AN* n = make_ast();
    n->type = AST_IF;
    n->if_node.cond = cond;
    n->if_node.then = then;
    n->if_node.else_ = else_;
    return n;
}

AN* make_ast_while(AN* cond, AN* body) {
    AN* n = make_ast();
    n->type = AST_WHILE;
    n->while_node.cond = cond;
    n->while_node.body = body;
    return n;
}

AN* make_ast_return(AN* value) {
    AN* n = make_ast();
    n->type = AST_RETURN;
    n->return_node.value = value;
    return n;
}

AN* make_ast_block(Vector* stmts) {
    AN* n = make_ast();
    n->type = AST_BLOCK;
    n->block_node.stmts = stmts;
    return n;
}

AN* make_ast_number(int value) {
    AN* n = make_ast();
    n->type = AST_NUMBER;
    n->number.value = value;
    return n;
}

AN* make_ast_float_number(float float_num) {
    AN* n = make_ast();
    n->type = AST_FL_NUMBER;
    n->float_number_node.float_num = float_num;
    return n;
}

AN* make_ast_string(char* value) {
    AN* n = make_ast();
    n->type = AST_STRING;
    n->string.value = value;
    return n;
}

AN* make_ast_binop(char* op, AN* left, AN* right) {
    AN* n = make_ast();
    n->type = AST_BINOP;
    n->binop.op = op;
    n->binop.left = left;
    n->binop.right = right;
    return n;
}

AN* make_ast_paren(AN* expr) {
    AN* n = make_ast();
    n->type = AST_PAREN_OP;
    n->paren_expr.expr = expr;
    return n;
}

AN* make_ast_unop(char* op, AN* operand) {
    AN* n = make_ast();
    n->type = AST_UNOP;
    n->unop.op = op;
    n->unop.operand = operand;
    return n;
}

AN* make_ast_func(char* ident, Vector* args, AN* body, int is_proc) {
    AN* n = make_ast();
    n->type = AST_FUNC;
    n->func_node.ident = ident;
    n->func_node.args = args;
    n->func_node.body = body;
    n->func_node.is_proc = is_proc;
    return n;
}

AN* make_ast_call(AN* name, Vector* args) {
    AN* n = make_ast();
    n->type = AST_CALL;
    n->call_node.name = name;
    n->call_node.args = args;
    return n;
}

AN* make_ast_ident(char* name) {
    AN* n = make_ast();
    n->type = AST_IDENT;
    n->ident.name = name;
    return n;
}

AN* make_ast_index(AN* base, AN* index) {
    AN* n = make_ast();
    n->type = AST_INDEX;
    n->index_node.base = base;
    n->index_node.index = index;
    return n;
}

AN* make_ast_varar(char* name) {
    AN* n = make_ast();
    n->type = AST_VARAR;
    n->varar.name = name;
    return n;
}


static Token* _peek_token = 0;

Token* peek(struct file* file) {
    if (!_peek_token) _peek_token = lex(file);
    return _peek_token;
}

Token* advance(struct file* file) {
    Token* t = peek(file);
    _peek_token = 0;
    return t;
}

int is_eof(Token* t) {
    return !t || t->kind == token_eof;
}

int is_type_token(Token* t) {
    if (!t) return 0;
    switch (t->kind) {
        case u32_type:
        case u16_type:
        case u8_type:
        case bool_type:
        case float_type:
        case string_type:
            return 1;
        default:
            return 0;
    }
}


AN* parse_expr(struct file* file);

char* op_str(int kind) {
    switch (kind) {
        case plus:         return "+";
        case minus:        return "-";
        case multiply:     return "*";
        case divide:       return "/";
        case modulo:       return "%";
        case plus_assign:        return "+";
        case minus_assign:       return "-";
        case multiply_assign:    return "*";
        case divide_assign:      return "/";
        case modulo_assign:      return "%";
        case bit_and_assign:     return "&";
        case bit_or_assign:      return "|";
        case bit_xor_assign:     return "^";
        case shift_left_assign:  return "<<";
        case shift_right_assign: return ">>";
        case equals:             return "==";
        case dont_equal:         return "!=";
        case greater_than:       return ">";
        case lesser_than:        return "<";
        case greater_or_equal:   return ">=";
        case lesser_or_equal:    return "<=";
        case shift_left:         return "<<";
        case shift_right:        return ">>";
        case bit_xor:            return "^";
        case bit_and:            return "&";
        case bit_or:             return "|";
        case logical_and:        return "&&";
        case logical_or:         return "||";
        case logical_not:        return "!";
        case bit_not:            return "~";
        default:                 return "?";
    }
}

AN* parse_primary(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    switch (t->kind) {
        case number:
            advance(file);
            return make_ast_number(t->number.val);

        case float_number:
            advance(file);
            return make_ast_float_number(t->float_number.val);

        case string:
            advance(file);
            return make_ast_string(t->string.val);

        case identifier:
            advance(file);
            return make_ast_ident(t->identifier.name);

        case string_type:
            advance(file);
            return make_ast_ident("str");

        case float_type:
            advance(file);
            return make_ast_ident("fp");

        case left_paren: {
            advance(file);
            AN* e = parse_expr(file);
            Token* rp = advance(file);
            if (!rp || rp->kind != right_paren) return 0;
            return make_ast_paren(e);
        }
    }
    return 0;
}

AN* parse_postfix(struct file* file) {
    AN* node = parse_primary(file);
    if (!node) return 0;

    for (;;) {
        Token* t = peek(file);
        if (is_eof(t)) break;

        if (t->kind == left_quad_brace) {
            advance(file);
            AN* idx = parse_expr(file);
            Token* rb = advance(file);
            if (!rb || rb->kind != right_quad_brace) return 0;
            node = make_ast_index(node, idx);
            continue;
        }

        if (t->kind == left_paren) {
            advance(file);
            Vector* args = make_vector();
            Token* rp = peek(file);
            if (!is_eof(rp) && rp->kind != right_paren) {
                for (;;) {
                    AN* arg = parse_expr(file);
                    if (!arg) return 0;
                    vec_push(args, arg);
                    Token* sep = peek(file);
                    if (!is_eof(sep) && sep->kind == comma) {
                        advance(file);
                        continue;
                    }
                    break;
                }
            }
            Token* end = advance(file);
            if (!end || end->kind != right_paren) return 0;
            node = make_ast_call(node, args);
            continue;
        }

        break;
    }
    return node;
}

static AN* parse_explicit_call(struct file* file) {
    AN* call = parse_assign_target(file);
    if (!call || call->type != AST_CALL) return 0;
    return call;
}

AN* parse_unary(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    if (t->kind == call_keyword) {
        advance(file);
        return parse_explicit_call(file);
    }

    if (t->kind == minus || t->kind == plus ||
        t->kind == logical_not || t->kind == bit_not) {
        int k = t->kind;
        advance(file);
        AN* operand = parse_unary(file);
        return make_ast_unop(op_str(k), operand);
    }
    return parse_postfix(file);
}

#define BIN_LEVEL(name, next, op_a, op_b)             \
    AN* name(struct file* file) {              \
        AN* left = next(file);                        \
        if (!left) return 0;                          \
        for (;;) {                                    \
            Token* t = peek(file);                    \
            if (is_eof(t)) break;                     \
            if (t->kind != op_a && t->kind != op_b)   \
                break;                                \
            int k = t->kind;                          \
            advance(file);                            \
            AN* right = next(file);                   \
            left = make_ast_binop(op_str(k), left, right); \
        }                                             \
        return left;                                  \
    }

AN* parse_mul(struct file* file) {
    AN* left = parse_unary(file);
    if (!left) return 0;
    for (;;) {
        Token* t = peek(file);
        if (is_eof(t)) break;
        if (t->kind != multiply && t->kind != divide && t->kind != modulo)
            break;
        int k = t->kind;
        advance(file);
        AN* right = parse_unary(file);
        left = make_ast_binop(op_str(k), left, right);
    }
    return left;
}

BIN_LEVEL(parse_add,  parse_mul,   plus,         minus)
BIN_LEVEL(parse_shift, parse_add,  shift_left,   shift_right)

AN* parse_rel(struct file* file) {
    AN* left = parse_shift(file);
    if (!left) return 0;
    for (;;) {
        Token* t = peek(file);
        if (is_eof(t)) break;
        if (t->kind != lesser_than && t->kind != greater_than &&
            t->kind != lesser_or_equal && t->kind != greater_or_equal)
            break;
        int k = t->kind;
        advance(file);
        AN* right = parse_shift(file);
        left = make_ast_binop(op_str(k), left, right);
    }
    return left;
}

BIN_LEVEL(parse_eq,   parse_rel,   equals,       dont_equal)
BIN_LEVEL(parse_band, parse_eq,    bit_and,      bit_and)
BIN_LEVEL(parse_bxor, parse_band,  bit_xor,      bit_xor)
BIN_LEVEL(parse_bor,  parse_bxor,  bit_or,       bit_or)
BIN_LEVEL(parse_land, parse_bor,   logical_and,  logical_and)
BIN_LEVEL(parse_lor,  parse_land,  logical_or,   logical_or)

AN* parse_expr(struct file* file) {
    return parse_lor(file);
}


AN* parse_assign_target(struct file* file) {
    Token* id = peek(file);
    if (is_eof(id) || id->kind != identifier) return 0;
    return parse_postfix(file);
}

static int is_compound_assign(int kind) {
    switch (kind) {
        case plus_assign:
        case minus_assign:
        case multiply_assign:
        case divide_assign:
        case modulo_assign:
        case bit_and_assign:
        case bit_or_assign:
        case bit_xor_assign:
        case shift_left_assign:
        case shift_right_assign:
            return 1;
        default:
            return 0;
    }
}

AN* parse_gen_assign(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    if (t->kind == var_keyword) {
        advance(file);
        if (!is_type_token(peek(file))) return 0;
        advance(file);

        Token* id = advance(file);
        if (!id || id->kind != identifier) return 0;

        Token* eq = advance(file);
        if (!eq || eq->kind != assign) return 0;

        AN* val = parse_expr(file);
        return make_ast_assign(id->identifier.name, val);
    }

    if (t->kind == varar_keyword) {
        advance(file);
        if (!is_type_token(peek(file))) return 0;
        advance(file);

        Token* id = advance(file);
        if (!id || id->kind != identifier) return 0;

        return make_ast_varar(id->identifier.name);
    }

    AN* lhs = parse_assign_target(file);
    if (!lhs) return 0;

    Token* eq = peek(file);
    if (!is_eof(eq) && is_compound_assign(eq->kind)) {
        int k = eq->kind;
        advance(file);
        AN* rhs = parse_expr(file);
        return make_ast_nassign(lhs, make_ast_binop(op_str(k), lhs, rhs));
    }
    if (is_eof(eq) || eq->kind != assign) {
        if (lhs->type == AST_CALL) return lhs;
        return 0;
    }
    advance(file);

    AN* rhs = parse_expr(file);
    return make_ast_nassign(lhs, rhs);
}


AN* parse_statement(struct file* file);

static Vector* parse_params(struct file* file) {
    Vector* args = make_vector();
    Token* t = peek(file);
    if (is_eof(t) || t->kind == right_paren) return args;

    for (;;) {
        Token* id = advance(file);
        if (!id || id->kind != identifier) return 0;
        vec_push(args, make_ast_ident(id->identifier.name));

        Token* sep = peek(file);
        if (!is_eof(sep) && sep->kind == comma) {
            advance(file);
            continue;
        }
        break;
    }
    return args;
}

AN* parse_block(struct file* file) {
    Token* lb = advance(file);
    if (!lb || lb->kind != left_brace) return 0;

    Vector* stmts = make_vector();
    for (;;) {
        Token* t = peek(file);
        if (is_eof(t)) return 0;
        if (t->kind == right_brace) { advance(file); break; }

        AN* s = parse_statement(file);
        if (!s) return 0;
        vec_push(stmts, s);
    }

    return make_ast_block(stmts);
}

static AN* parse_func_def(struct file* file, int is_proc) {
    Token* id = advance(file);
    if (!id || id->kind != identifier) return 0;

    Token* lp = advance(file);
    if (!lp || lp->kind != left_paren) return 0;

    Vector* params = parse_params(file);
    if (!params) return 0;

    Token* rp = advance(file);
    if (!rp || rp->kind != right_paren) return 0;

    AN* body = parse_block(file);
    if (!body) return 0;

    return make_ast_func(id->identifier.name, params, body, is_proc);
}

static AN* parse_if(struct file* file, int is_elif);

static AN* parse_else_tail(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    if (t->kind == if_keyword) {
        advance(file);
        AN* n = parse_if(file, 1);
        return n;
    }

    return parse_block(file);
}

static AN* parse_if(struct file* file, int is_elif) {
    Token* lp = advance(file);
    if (!lp || lp->kind != left_paren) return 0;

    AN* cond = parse_expr(file);
    if (!cond) return 0;

    Token* rp = advance(file);
    if (!rp || rp->kind != right_paren) return 0;

    AN* then = parse_block(file);
    if (!then) return 0;

    AN* else_ = 0;
    Token* t = peek(file);
    if (!is_eof(t) && t->kind == else_keyword) {
        advance(file);
        else_ = parse_else_tail(file);
        if (!else_) return 0;
    }

    AN* n = make_ast_if(cond, then, else_);
    n->if_node.is_elif = is_elif;
    return n;
}

static AN* parse_while(struct file* file) {
    Token* lp = advance(file);
    if (!lp || lp->kind != left_paren) return 0;

    AN* cond = parse_expr(file);
    if (!cond) return 0;

    Token* rp = advance(file);
    if (!rp || rp->kind != right_paren) return 0;

    AN* body = parse_block(file);
    if (!body) return 0;

    return make_ast_while(cond, body);
}

AN* parse_statement(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    switch (t->kind) {
        case udf_keyword: {
            advance(file);
            return parse_func_def(file, 0);
        }
        case udp_keyword: {
            advance(file);
            return parse_func_def(file, 1);
        }
        case return_keyword: {
            advance(file);
            LOGI("stmt: return");
            AN* val = parse_expr(file);
            return make_ast_return(val);
        }
        case if_keyword: {
            advance(file);
            return parse_if(file, 0);
        }
        case while_keyword: {
            advance(file);
            return parse_while(file);
        }
        case call_keyword: {
            advance(file);
            return parse_explicit_call(file);
        }
        default:
            return parse_gen_assign(file);
    }
}

static AN* parse_statement_verbose(struct file* file) {
    Token* t = peek(file);
    if (is_eof(t)) return 0;

    switch (t->kind) {
        case udf_keyword: {
            advance(file);
            LOGI("stmt: udf definition");
            return parse_func_def(file, 0);
        }
        case udp_keyword: {
            advance(file);
            LOGI("stmt: udp definition");
            return parse_func_def(file, 1);
        }
        case return_keyword: {
            advance(file);
            LOGI("stmt: return");
            AN* val = parse_expr(file);
            return make_ast_return(val);
        }
        case if_keyword: {
            advance(file);
            LOGI("stmt: if");
            return parse_if(file, 0);
        }
        case while_keyword: {
            advance(file);
            LOGI("stmt: whl");
            return parse_while(file);
        }
        case call_keyword: {
            advance(file);
            LOGI("stmt: call");
            return parse_explicit_call(file);
        }
        default:
            return parse_gen_assign_verbose(file);
    }
}


void parse(struct file* file) {
    _peek_token = 0;
    if (!nodes) nodes = make_vector();
    for (;;) {
        Token* t = peek(file);
        if (is_eof(t)) break;

        AN* node = parse_statement(file);
        if (!node) break;

        vec_push(nodes, node);
    }
}

void see() {
     for (int i = 0; i < vec_len(nodes); i++) {
        LOGI("%u", vec_get(nodes, i));
    }
}


void dump_ast(AN* node, int indent);

char* ast_type_name(int type) {
    switch (type) {
        case AST_ASSIGN:      return "AST_ASSIGN";
        case AST_EXPR_ASSIGN: return "AST_EXPR_ASSIGN";
        case AST_IF:          return "AST_IF";
        case AST_WHILE:       return "AST_WHILE";
        case AST_RETURN:      return "AST_RETURN";
        case AST_BLOCK:       return "AST_BLOCK";
        case AST_NUMBER:      return "AST_NUMBER";
        case AST_FL_NUMBER:   return "AST_FL_NUMBER";
        case AST_STRING:      return "AST_STRING";
        case AST_BINOP:       return "AST_BINOP";
        case AST_PAREN_OP:    return "AST_PAREN_OP";
        case AST_UNOP:        return "AST_UNOP";
        case AST_FUNC:        return "AST_FUNC";
        case AST_CALL:        return "AST_CALL";
        case AST_IDENT:       return "AST_IDENT";
        case AST_INDEX:       return "AST_INDEX";
        case AST_VARAR:       return "AST_VARAR";
        default:              return "AST_???";
    }
}

static void dump_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void dump_ast_children(AN* node, int indent) {
    if (!node) {
        dump_indent(indent);
        printf("<null>\n");
        return;
    }
    dump_ast(node, indent);
}

void dump_ast(AN* node, int indent) {
    if (!node) {
        dump_indent(indent);
        printf("<null>\n");
        return;
    }

    dump_indent(indent);
    printf("%s @0x%x\n", ast_type_name(node->type), node);

    switch (node->type) {
        case AST_ASSIGN:
            dump_indent(indent + 1);
            printf("name: \"%s\"\n", node->assign.name);
            dump_indent(indent + 1);
            printf("value:\n");
            dump_ast_children(node->assign.value, indent + 2);
            break;

        case AST_EXPR_ASSIGN:
            dump_indent(indent + 1);
            printf("name:\n");
            dump_ast_children(node->ast_assign.name, indent + 2);
            dump_indent(indent + 1);
            printf("value:\n");
            dump_ast_children(node->ast_assign.value, indent + 2);
            break;

        case AST_IF:
            dump_indent(indent + 1);
            printf("cond:\n");
            dump_ast_children(node->if_node.cond, indent + 2);
            dump_indent(indent + 1);
            printf("then:\n");
            dump_ast_children(node->if_node.then, indent + 2);
            dump_indent(indent + 1);
            printf("else:\n");
            dump_ast_children(node->if_node.else_, indent + 2);
            dump_indent(indent + 1);
            printf("is_elif: %u\n", node->if_node.is_elif);
            break;

        case AST_WHILE:
            dump_indent(indent + 1);
            printf("cond:\n");
            dump_ast_children(node->while_node.cond, indent + 2);
            dump_indent(indent + 1);
            printf("body:\n");
            dump_ast_children(node->while_node.body, indent + 2);
            break;

        case AST_RETURN:
            dump_indent(indent + 1);
            printf("value:\n");
            dump_ast_children(node->return_node.value, indent + 2);
            break;

        case AST_BLOCK:
            dump_indent(indent + 1);
            printf("stmts: len=%u\n",
                   node->block_node.stmts ? vec_len(node->block_node.stmts) : 0);
            if (node->block_node.stmts) {
                for (int i = 0; i < vec_len(node->block_node.stmts); i++) {
                    dump_indent(indent + 2);
                    printf("[%u]:\n", i);
                    dump_ast_children((AN*)vec_get(node->block_node.stmts, i), indent + 3);
                }
            }
            break;

        case AST_NUMBER:
            dump_indent(indent + 1);
            printf("value: %u\n", node->number.value);
            break;

        case AST_FL_NUMBER: {
            char fbuf[64];
            format_float(fbuf, node->float_number_node.float_num);
            dump_indent(indent + 1);
            printf("float_num: \"%s\"\n", fbuf);
            break;
        }

        case AST_STRING:
            dump_indent(indent + 1);
            printf("value: \"%s\"\n", node->string.value);
            break;

        case AST_BINOP:
            dump_indent(indent + 1);
            printf("op: \"%s\"\n", node->binop.op);
            dump_indent(indent + 1);
            printf("left:\n");
            dump_ast_children(node->binop.left, indent + 2);
            dump_indent(indent + 1);
            printf("right:\n");
            dump_ast_children(node->binop.right, indent + 2);
            break;

        case AST_PAREN_OP:
            dump_indent(indent + 1);
            printf("expr:\n");
            dump_ast_children(node->paren_expr.expr, indent + 2);
            break;

        case AST_UNOP:
            dump_indent(indent + 1);
            printf("op: \"%s\"\n", node->unop.op);
            dump_indent(indent + 1);
            printf("operand:\n");
            dump_ast_children(node->unop.operand, indent + 2);
            break;

        case AST_FUNC:
            dump_indent(indent + 1);
            printf("ident: \"%s\"\n", node->func_node.ident);
            dump_indent(indent + 1);
            printf("kind: %s\n", node->func_node.is_proc ? "udp" : "udf");
            dump_indent(indent + 1);
            printf("args: len=%u\n", node->func_node.args ? vec_len(node->func_node.args) : 0);
            if (node->func_node.args) {
                for (int i = 0; i < vec_len(node->func_node.args); i++) {
                    dump_indent(indent + 2);
                    printf("[%u]:\n", i);
                    dump_ast_children((AN*)vec_get(node->func_node.args, i), indent + 3);
                }
            }
            dump_indent(indent + 1);
            printf("body:\n");
            dump_ast_children(node->func_node.body, indent + 2);
            break;

        case AST_CALL:
            dump_indent(indent + 1);
            printf("name:\n");
            dump_ast_children(node->call_node.name, indent + 2);
            dump_indent(indent + 1);
            printf("args: len=%u\n", node->call_node.args ? vec_len(node->call_node.args) : 0);
            if (node->call_node.args) {
                for (int i = 0; i < vec_len(node->call_node.args); i++) {
                    dump_indent(indent + 2);
                    printf("[%u]:\n", i);
                    dump_ast_children((AN*)vec_get(node->call_node.args, i), indent + 3);
                }
            }
            break;

        case AST_IDENT:
            dump_indent(indent + 1);
            printf("name: \"%s\"\n", node->ident.name);
            break;

        case AST_INDEX:
            dump_indent(indent + 1);
            printf("base:\n");
            dump_ast_children(node->index_node.base, indent + 2);
            dump_indent(indent + 1);
            printf("index:\n");
            dump_ast_children(node->index_node.index, indent + 2);
            break;

        case AST_VARAR:
            dump_indent(indent + 1);
            printf("name: \"%s\"\n", node->varar.name);
            break;

        default:
            dump_indent(indent + 1);
            printf("<unknown type %u>\n", node->type);
            break;
    }
}

void dump_nodes(void) {
    LOGI("nodes vector: len=%u nalloc=%u body=0x%x",
         nodes ? vec_len(nodes) : 0,
         nodes ? nodes->nalloc : 0,
         nodes ? nodes->body : 0);

    if (!nodes) return;

    for (int i = 0; i < vec_len(nodes); i++) {
        AN* node = (AN*)vec_get(nodes, i);
        LOGI("nodes[%u] = 0x%x type=%s", i, node,
             node ? ast_type_name(node->type) : "<null>");
        if (node) dump_ast(node, 1);
    }
}

AN* parse_gen_assign_verbose(struct file* file) {
    LOGI("gen_assign: start");
    Token* t = peek(file);
    if (is_eof(t)) {
        LOGI("gen_assign: EOF reached (token kind=%u)", t ? t->kind : -1);
        return 0;
    }
    LOGI("gen_assign: peek token kind=%u", t->kind);

    if (t->kind == var_keyword) {
        LOGI("gen_assign: branch -> var declaration");
        advance(file);
        if (!is_type_token(peek(file))) {
            LOGW("gen_assign: expected type token after var");
            return 0;
        }
        advance(file);

        Token* id = advance(file);
        if (!id || id->kind != identifier) {
            LOGW("gen_assign: expected identifier after type, got kind=%u",
                 id ? id->kind : -1);
            return 0;
        }
        LOGI("gen_assign: var name=\"%s\"", id->identifier.name);

        Token* eq = advance(file);
        if (!eq || eq->kind != assign) {
            LOGW("gen_assign: expected '=' after var name, got kind=%u",
                 eq ? eq->kind : -1);
            return 0;
        }

        AN* val = parse_expr(file);
        LOGI("gen_assign: var value type=%s",
             val ? ast_type_name(val->type) : "<null>");
        LOGI("gen_assign: done, node type=%s IT FALLS DOWN REALLY AT THIS!!!!!", "AST_ASSIGN");
        return make_ast_assign(id->identifier.name, val);
    }

    if (t->kind == varar_keyword) {
        LOGI("gen_assign: branch -> varar declaration");
        advance(file);
        if (!is_type_token(peek(file))) {
            LOGW("gen_assign: expected type token after varar");
            return 0;
        }
        advance(file);

        Token* id = advance(file);
        if (!id || id->kind != identifier) {
            LOGW("gen_assign: expected identifier after type, got kind=%u",
                 id ? id->kind : -1);
            return 0;
        }
        LOGI("gen_assign: varar name=\"%s\"", id->identifier.name);
        LOGI("gen_assign: done, node type=%s", "AST_VARAR");
        return make_ast_varar(id->identifier.name);
    }

    LOGI("gen_assign: branch -> expression assignment");
    AN* lhs = parse_assign_target(file);
    if (!lhs) {
        LOGW("gen_assign: parse_assign_target returned null");
        return 0;
    }

    Token* eq = peek(file);
    if (!is_eof(eq) && is_compound_assign(eq->kind)) {
        int k = eq->kind;
        advance(file);
        LOGI("gen_assign: compound operator \"%s\"", op_str(k));
        AN* rhs = parse_expr(file);
        LOGI("gen_assign: rhs type=%s",
             rhs ? ast_type_name(rhs->type) : "<null>");
        LOGI("gen_assign: done, node type=%s", "AST_EXPR_ASSIGN");
        return make_ast_nassign(lhs, make_ast_binop(op_str(k), lhs, rhs));
    }
    if (is_eof(eq) || eq->kind != assign) {
        if (lhs->type == AST_CALL) {
            LOGI("gen_assign: done, node type=%s", "AST_CALL");
            return lhs;
        }
        LOGW("gen_assign: expected '=' after lhs, got kind=%u",
             eq ? eq->kind : -1);
        return 0;
    }
    advance(file);
    LOGI("gen_assign: lhs type=%s", ast_type_name(lhs->type));

    AN* rhs = parse_expr(file);
    LOGI("gen_assign: rhs type=%s",
         rhs ? ast_type_name(rhs->type) : "<null>");
    LOGI("gen_assign: done, node type=%s", "AST_EXPR_ASSIGN");
    return make_ast_nassign(lhs, rhs);
}

void parse_verbose(struct file* file) {
    if (!nodes) nodes = make_vector();
    LOGI("parse: start");
    for (;;) {

        AN* node = parse_statement_verbose(file);
        if (!node) {
            LOGW("parse: parse_statement returned null, stopping");
            break;
        }

        vec_push(nodes, node);
        LOGI("parse: pushed node[%u] type=%s", vec_len(nodes) - 1, ast_type_name(node->type));
    }
    LOGI("parse: done, %u nodes", vec_len(nodes));
    dump_nodes();
}
