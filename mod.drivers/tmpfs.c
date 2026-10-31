/*
 * Copyright (c) 2026 TINERKOTL & ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirapi.h"
#include "../lib/string.h"

struct tmpfs_node {
    char *name;
    bool type; //0 - файл, 1 - папка
    struct tmpfs_node *parent;
    struct tmpfs_node *children;
    struct tmpfs_node *next_sibling;

    u32 size;
    u8 *data;
    u32 capacity;
};

struct tmpfs {
    char mount_letter;
    struct disk *vfs_disk;
    struct tmpfs_node *root;
};

static u32 tmpfs_getsize(struct file *file) {
    if (!file || !file->other_info)
        return 0;

    struct tmpfs_node *node = file->other_info;
    return node->size;
}

static u32 tmpfs_write(struct file *file, void* buffer, u32 size, u32 offset) {
    if (!file || !buffer || !file->other_info || !size)
        return 0;
    
    struct tmpfs_node *node = file->other_info;
    if (node->type == 0) {
        if (size > 0xFFFFFFFF - offset)
            return 0;

        u32 last_position = offset + size;
        
        if (last_position > node->capacity){
            u8 *new_data = alloc(last_position);
            if (!new_data)
                return 0;
            
            memcpy(new_data, node->data, node->size);
            free(node->data);
            node->capacity = last_position;
            node->data = new_data;
        }

        if (offset > node->size) {
            for (u32 i = node->size; i < offset; i++)
                node->data[i] = '\0';
            
            node->size = offset;
        }

        if (last_position > node->size)
            node->size = last_position;

        memcpy(node->data + offset, buffer, size);

        return size;
    } else {
        return 0; // нельзя сделать запись в папку
    }
}

static struct tmpfs_node *tmpfs_find_child(struct tmpfs_node *parent, const char *name, u32 name_len){
    if (!parent || !name || !name_len)
        return 0;
    
    for (struct tmpfs_node *node = parent->children; node; node = node->next_sibling){
        if (strlen(node->name) == name_len && strncmp(node->name, name, name_len) == 0)
            return node;
    }

    return 0;
}

static struct tmpfs_node *tmpfs_find_path(struct tmpfs *fs, const char *path) {
    if (!fs || !path || !fs->root)
        return 0;
    
    struct tmpfs_node *node = fs->root;

    while (*path == '/')
        path++;

    while (*path) {
        const char *component = path;
        while (*path && *path != '/')
            path++;
        
        u32 component_len = path - component;

        if (!node->type)
            return 0;
        
        node = tmpfs_find_child(node, component, component_len);
        if(!node)
            return 0;
        
        while (*path == '/')
            path++;
    }
    
    return node;
}

static bool tmpfs_close(struct file *file) {
    if (!file)
        return false;
    
    return free(file);
}

static u32 tmpfs_read(struct file *file, void* buffer, u32 size, u32 offset) {
    if (!file || !buffer)
        return 0;

    struct tmpfs_node *node = file->other_info;
    if (!node)
        return 0;

    u32 skip = offset / sizeof(struct file);
    u32 capacity = size / sizeof(struct file);
    u32 written = 0;

    if (node->type){
        struct file *entries = buffer;

        struct tmpfs_node *child = node->children;
        while (child && skip) {
            child = child->next_sibling;
            skip--;
        }

        while (child && written < capacity) {
            entries[written].path = child->name;
            entries[written].dsk = file->dsk;
            entries[written].is_dir = child->type;
            entries[written].other_info = child;
            entries[written].read = child->type ? tmpfs_read : 0;
            entries[written].write = 0;
            entries[written].getsize = tmpfs_getsize;
            entries[written].close = 0;

            written++;
            child = child->next_sibling;
        }

        return written * sizeof(struct file);
    } else {
        if (!node->data || offset >= node->size)
            return 0;
        
        u32 availble = node->size - offset;
        u32 to_read = size < availble ? size : availble;

        memcpy(buffer, node->data, to_read);

        return to_read;
    }
}

static struct file *tmpfs_mkdir(struct disk *disk, char *path){
    if (!disk || !path || !disk->fs_info)
        return 0;

    struct tmpfs *fs = disk->fs_info;
    if (!fs->root)
        return 0;
    
    u32 path_len = strlen(path);
    char *path_copy = alloc(path_len + 1);
    if (!path_copy)
        return 0;
    
    memcpy(path_copy, path, path_len + 1);

    char *p = path_copy;
    while (*p == '/')
        p++;
    
    char *last_slash = 0; //ищет последний компонент (папку)
    for (char *c = p; *c; c++)
        if(*c=='/')
            last_slash = c;
    
    char *parent_path;
    char *dir_name;
    u32 dir_name_len;

    if (last_slash){
        *last_slash = 0;
        parent_path = p;
        dir_name = last_slash + 1; // /(...)/(...)/
    } else {
        parent_path = "";
        dir_name = p;
    }

    dir_name_len = strlen(dir_name);

    if (dir_name_len == 0){
        free(path_copy);
        return 0;
    }

    struct tmpfs_node *parent = tmpfs_find_path(fs, parent_path);
    if (!parent || !parent->type)
    {
        free(path_copy);
        return 0;
    } //родитель или каталог не найден
    
    if (tmpfs_find_child(parent, dir_name, dir_name_len))
    {
        free(path_copy);
        return 0;
    } //уже существет папка
    
    struct tmpfs_node *node = alloc(sizeof(*node));
    struct file *file = alloc(sizeof(*file));

    if (!node || !file){
        free(node);
        free(file);
        free(path_copy);
        return 0;
    }

    node->name = alloc(dir_name_len + 1);
    if (!node->name){
        free(node);
        free(file);
        free(path_copy);
        return 0;
    }

    memcpy(node->name, dir_name, dir_name_len);
    node->name[dir_name_len] = 0;
    free(path_copy);
    node->type = 1; //каталог
    node->parent = parent;
    node->children = 0;
    node->next_sibling = parent->children;
    node->size = 0;
    node->data = 0;
    node->capacity = 0;

    parent->children = node;

    file->dsk = disk;
    file->path = node->name;
    file->is_dir = node->type;
    file->read = tmpfs_read;
    file->write = 0;
    file->getsize = tmpfs_getsize;
    file->close = tmpfs_close;
    file->other_info = node;

    return file;
}

static struct file *tmpfs_open(struct disk *disk, char *path) {
    if (!disk || !path || !disk->fs_info)
        return 0;

    struct tmpfs *fs = disk->fs_info;

    if (!fs->root)
        return 0;

    struct tmpfs_node *node = tmpfs_find_path(fs, path);

    if (!node)
        return 0;

    struct file *file = alloc(sizeof(*file));

    if (!file)
        return 0;

    file->dsk = disk;
    file->path = node->name;
    file->is_dir = node->type;
    file->read = tmpfs_read;
    file->write = tmpfs_write;
    file->getsize = tmpfs_getsize;
    file->close = tmpfs_close;
    file->other_info = node;

    return file;
}

static struct file *tmpfs_create_file(struct disk *disk, char *path){
    if (!disk || !path || !disk->fs_info)
        return 0;

    struct tmpfs *fs = disk->fs_info;
    if (!fs->root)
        return 0;
    
    u32 path_len = strlen(path);
    char *path_copy = alloc(path_len + 1);
    if (!path_copy)
        return 0;
    
    memcpy(path_copy, path, path_len + 1);

    char *p = path_copy;
    while (*p == '/')
        p++;
    
    char *last_slash = 0; //ищет последний компонент (папку)
    for (char *c = p; *c; c++)
        if(*c=='/')
            last_slash = c;
    
    char *parent_path;
    char *file_name;
    u32 file_name_len;

    if (last_slash){
        *last_slash = 0;
        parent_path = p;
        file_name = last_slash + 1; // /(...)/(...)/
    } else {
        parent_path = "";
        file_name = p;
    }

    file_name_len = strlen(file_name);

    if (file_name_len == 0){
        free(path_copy);
        return 0;
    }

    struct tmpfs_node *parent = tmpfs_find_path(fs, parent_path);
    if (!parent || !parent->type)
    {
        free(path_copy);
        return 0;
    } //родитель или каталог не найден
    
    if (tmpfs_find_child(parent, file_name, file_name_len))
    {
        free(path_copy);
        return 0;
    } //уже существет файл
    
    struct tmpfs_node *node = alloc(sizeof(*node));
    struct file *file = alloc(sizeof(*file));

    if (!node || !file){
        free(node);
        free(file);
        free(path_copy);
        return 0;
    }

    node->name = alloc(file_name_len + 1);
    if (!node->name){
        free(node);
        free(file);
        free(path_copy);
        return 0;
    }

    memcpy(node->name, file_name, file_name_len);
    node->name[file_name_len] = 0;
    free(path_copy);
    node->type = 0; //файл
    node->parent = parent;
    node->children = 0;
    node->next_sibling = parent->children;
    node->size = 0;
    node->data = 0;
    node->capacity = 0;

    parent->children = node;

    file->dsk = disk;
    file->path = node->name;
    file->is_dir = node->type;
    file->read = tmpfs_read;
    file->write = tmpfs_write;
    file->getsize = tmpfs_getsize;
    file->close = tmpfs_close;
    file->other_info = node;

    return file;
}

static char mount_tmpfs(struct tmpfs *fs) {
    if (!fs)
        return 0;

    fs->root = alloc(sizeof(*fs->root));
    fs->vfs_disk = alloc(sizeof(*fs->vfs_disk));

    if (!fs->root || !fs->vfs_disk)
        return 0;

    fs->root->name = "/";
    fs->root->type = true;
    fs->root->parent = 0;
    fs->root->children = 0;
    fs->root->next_sibling = 0;
    fs->root->size = 0;
    fs->root->data = 0;
    fs->root->capacity = 0;

    fs->vfs_disk->name = "tmpfs";
    fs->vfs_disk->size = 0;
    fs->vfs_disk->mountletter = 0;
    fs->vfs_disk->lba_read = 0;
    fs->vfs_disk->lba_write = 0;
    fs->vfs_disk->other_info = 0;
    fs->vfs_disk->fs_info = fs;

    for (u32 i = 0; i < 8; i++)
        fs->vfs_disk->partitions[i] = 0;

    fs->mount_letter = mount(fs->vfs_disk, (void *)tmpfs_open, (void *)tmpfs_mkdir, (void *)tmpfs_create_file);

    return fs->mount_letter;
}

static struct tmpfs tmpfs_instance;

INIT void init(void* _resolve_function(char* name))
{
    api_init(_resolve_function);

    if (!mount_tmpfs(&tmpfs_instance))
        return;
    LOGI("tmpfs mounted %c", tmpfs_instance.mount_letter);
    diskadd(tmpfs_instance.vfs_disk);

    //create_file(tmpfs_instance.mount_letter, "hi");
}
