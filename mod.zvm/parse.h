#ifndef PARSE_H
#define PARSE_H

#include "zhirvm_types.h"
#include "vector.h"

typedef ASTNode AN;

extern Vector* nodes;

AN* make_ast(void);
AN* make_ast_assign(char* name, AN* val);
AN* make_ast_nassign(AN* name, AN* val);
AN* make_ast_if(AN* cond, AN* then, AN* else_);
AN* make_ast_while(AN* cond, AN* body);
AN* make_ast_return(AN* value);
AN* make_ast_block(Vector* stmts);
AN* make_ast_number(int value);
AN* make_ast_float_number(float float_num);
AN* make_ast_string(char* value);
AN* make_ast_binop(char* op, AN* left, AN* right);
AN* make_ast_paren(AN* expr);
AN* make_ast_unop(char* op, AN* operand);
AN* make_ast_func(char* ident, Vector* args, AN* body, int is_proc);
AN* make_ast_call(AN* name, Vector* args);
AN* make_ast_ident(char* name);
AN* make_ast_index(AN* base, AN* index);
AN* make_ast_varar(char* name);


Token* peek(struct file* file);
Token* advance(struct file* file);
int is_eof(Token* t);
int is_type_token(Token* t);


AN* parse_expr(struct file* file);
char* op_str(int kind);
AN* parse_primary(struct file* file);
AN* parse_postfix(struct file* file);
AN* parse_unary(struct file* file);
AN* parse_mul(struct file* file);
AN* parse_add(struct file* file);
AN* parse_rel(struct file* file);
AN* parse_eq(struct file* file);
AN* parse_band(struct file* file);
AN* parse_bxor(struct file* file);
AN* parse_bor(struct file* file);
AN* parse_land(struct file* file);
AN* parse_lor(struct file* file);


AN* parse_assign_target(struct file* file);
AN* parse_gen_assign(struct file* file);


AN* parse_statement(struct file* file);
AN* parse_block(struct file* file);


void parse(struct file* file);
void see(void);


char* ast_type_name(int type);
void dump_ast(AN* node, int indent);
void dump_nodes(void);
AN* parse_gen_assign_verbose(struct file* file);
void parse_verbose(struct file* file);

#endif
