#ifndef ZHIRVM_TYPES_H
#define ZHIRVM_TYPES_H

#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "vector.h"

typedef struct ASTNode {
    int type;

    union {
        struct {
            char* name;
            struct ASTNode* value;
        } assign;

        struct {
            struct ASTNode* name;
            struct ASTNode* value;
        } ast_assign;

        struct {
            struct ASTNode *cond, *then, *else_;
            int is_elif;
        } if_node;

        struct {
            struct ASTNode *cond, *body;
        } while_node;

        struct {
            char* ident;
            Vector* args;
            struct ASTNode* body;
            int is_proc;   // 0 = udf (must return), 1 = udp (must not return)
        } func_node;

        struct {
            struct ASTNode* value;
        } return_node;

        struct {
            Vector* stmts;
        } block_node;

        struct {
            int value;
        } number;

        struct {
            char* value;
        } string;

        struct {
            char* op;
            struct ASTNode *left, *right;
        } binop;

        struct {
            struct ASTNode* expr;
        } paren_expr;

        struct {
            char* op;
            struct ASTNode* operand;
        } unop;

        struct {
            struct ASTNode* name;
            Vector* args;
        } call_node;

        struct {
            float float_num;
        } float_number_node;

        struct {
            char* name;
        } ident;

        struct {
            struct ASTNode* base;
            struct ASTNode* index;
        } index_node;

        struct {
            char* name;
        } varar;
    };
} ASTNode;

typedef enum ASTNodeKind {
    AST_ASSIGN,
    AST_EXPR_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_RETURN,
    AST_BLOCK,
    AST_NUMBER,
    AST_FL_NUMBER,
    AST_STRING,
    AST_BINOP,
    AST_PAREN_OP,
    AST_UNOP,
    AST_FUNC,
    AST_CALL,
    AST_IDENT,
    AST_INDEX,
    AST_VARAR
} ASTNodeKind;

typedef enum {
    if_keyword,
    else_keyword,
    identifier,
    return_keyword,
    while_keyword,
    break_keyword,
    continue_keyword,
    var_keyword,
    varar_keyword,
    udf_keyword,
    udp_keyword,
    class_keyword,
    register_keyword,
    goto_keyword,
    const_keyword,
    call_keyword,
    logical_and,
    logical_or,
    logical_not,
    dont_equal,
    u32_type,
    u16_type,
    u8_type,
    bool_type,
    float_type,
    string_type,
    left_paren,
    right_paren,
    func_lparen,
    func_rparen,
    call_lparen,
    call_rparen,
    left_quad_brace,
    right_quad_brace,
    left_brace,
    right_brace,
    assign,
    plus_assign,
    minus_assign,
    multiply_assign,
    divide_assign,
    modulo_assign,
    bit_and_assign,
    bit_or_assign,
    bit_xor_assign,
    shift_left_assign,
    shift_right_assign,
    equals,
    greater_than,
    lesser_than,
    greater_or_equal,
    lesser_or_equal,
    plus,
    minus,
    multiply,
    divide,
    modulo,
    shift_left,
    shift_right,
    bit_xor,
    bit_not,
    bit_and,
    bit_or,
    comma,
    dot,
    colon,
    number,
    float_number,
    string,
    token_eof
} ZhirVMTokens;

typedef struct Token {
    int kind;                

    union {
        struct { int  val; }          number;
        struct { float val; }         float_number;
        struct { char* name; }        identifier;
        struct { char* val;  }        string;
        struct { int  kind; }         keyword;
        struct { int  op;   }         operation;
        struct { int  not_actual; }   null;
    };
} Token;

#endif
