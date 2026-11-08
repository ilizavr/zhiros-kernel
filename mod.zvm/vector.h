#ifndef VECTOR_H
#define VECTOR_H
#include "../lib/zhirtypes.h"
#include "../lib/string.h"

#define MIN_SIZE 8

typedef struct  Vector{
    void **body;
    int len;
    int nalloc;
} Vector;

Vector *make_vector(void);
Vector *make_vector1(void *e);
Vector *vec_copy(Vector *src);
void vec_push(Vector *vec, void *elem);
void vec_append(Vector *a, Vector *b);
void *vec_pop(Vector *vec);
void *vec_get(Vector *vec, int index);
void vec_set(Vector *vec, int index, void *val);
void *vec_head(Vector *vec);
void *vec_tail(Vector *vec);
Vector *vec_reverse(Vector *vec);
void *vec_body(Vector *vec);
int vec_len(Vector *vec);
#endif
