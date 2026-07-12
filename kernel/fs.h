#pragma once
#include "types.h"

/* In-memory filesystem: simple tree of nodes backed by a flat data pool */
#define FS_MAX_NODES     256
#define FS_NAME_LEN       64
#define FS_DATA_POOL_SIZE (256 * 1024)   /* 256 KB data pool */
#define FS_MAX_FILE_DATA   (16 * 1024)   /* max 16 KB per file */

typedef enum { FS_DIR = 0, FS_FILE = 1 } fs_type_t;

typedef struct fs_node {
    char            name[FS_NAME_LEN];
    fs_type_t       type;
    uint32_t        size;         /* bytes of data (files only) */
    uint32_t        data_off;     /* offset in data pool */
    struct fs_node *parent;
    struct fs_node *child;        /* first child (dirs) */
    struct fs_node *next;         /* next sibling */
    bool            used;
} fs_node_t;

void        fs_init(void);

/* Path operations */
fs_node_t  *fs_lookup(fs_node_t *base, const char *path);
fs_node_t  *fs_mkdir(fs_node_t *parent, const char *name);
fs_node_t  *fs_create(fs_node_t *parent, const char *name);
int         fs_write(fs_node_t *node, const char *data, uint32_t len);
int         fs_read(fs_node_t *node, char *buf, uint32_t maxlen);
int         fs_unlink(fs_node_t *node);

fs_node_t  *fs_root(void);
