#ifndef LEXER_H
#define LEXER_H

#include "zhirvm_types.h"

#define INT_MAX 2147483647
#define NOT_ACTUAL -1

extern void* (*kalloc)(u32 size);
extern void (*printf)(...);
extern void (*print_color)(char* string, u32 color);
extern struct file* (*open)(char disk_letter, char* path);

extern int read_count;
extern char* buf;


int is_digit(char digit);
int is_alpha(int c);
int is_sspace(char c);
int is_number_buf(char* buf);
int is_identifier_buf(char* buf);
int sti(char* buf);
float stf(char* buf);


void init_string(struct file* file);
char* read_string(void);
char* get_string(char* buf);
char* get_identifier(char* buf);


Token* make_token_number(int val);
Token* make_token_float_number(float val);
Token* make_token_identifier(char* name);
Token* make_token_string(char* val);
Token* make_token_keyword(int kind);
Token* make_token_operation(int op);
Token* make_token_null(void);


Token* do_lex(void);
Token* lex(struct file* file);

void init(void* _resolve_function(char* name));

#endif
