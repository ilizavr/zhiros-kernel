#ifndef BUILTINS_H
#define BUILTINS_H

#include "vector.h"
#include "interpret.h"

Value printb(Vector* args);
Value printlnb(Vector* args);
Value lenb(Vector* args);
Value typeb(Vector* args);
Value strb(Vector* args);
Value intb(Vector* args);
Value floatb(Vector* args);
Value absb(Vector* args);
Value minb(Vector* args);
Value maxb(Vector* args);
Value chrb(Vector* args);
Value ordb(Vector* args);
void format_float(char* out, float value);

#endif
