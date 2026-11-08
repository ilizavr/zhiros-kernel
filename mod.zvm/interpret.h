#ifndef INTERPRET_H
#define INTERPRET_H

#include "parse.h"
#include "vector.h"

typedef enum ValKind {
    VAL_NONE,
    VAL_U32,     // also covers u16/u8/bool (stored in u)
    VAL_FLOAT,
    VAL_STRING,
} ValKind;

typedef struct Value {
    ValKind kind;
    union {
        u32 u;            // integer   AND  bool too
        float f;
        char* s;          
    };
} Value;


typedef enum Control {
    CTRL_NONE,    // fell off the end / plain statement
    CTRL_RETURN,  
    CTRL_BREAK,  
    CTRL_CONTINUE,
} Control;


void interpret(Vector* program);

#endif
