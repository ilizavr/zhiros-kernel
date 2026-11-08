#ifndef MAP_H
#define MAP_H

#include "../lib/zhirtypes.h"
#include "../lib/string.h"


typedef struct Map {
    struct Map *parent;
    char **key;
    void **val;
    int size;
    int nelem;
    int nused;
} Map;

#define EMPTY_MAP ((Map){})
Map *make_map(void);
Map *make_map_parent(Map *parent);
void *map_get(Map *m, char *key);
void map_put(Map *m, char *key, void *val);
void map_remove(Map *m, char *key);
size_t map_len(Map *m);

#endif
