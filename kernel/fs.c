#include "fs.h"
#include "string.h"

static fs_node_t  nodes[FS_MAX_NODES];
static uint8_t    data_pool[FS_DATA_POOL_SIZE];
static uint32_t   pool_next = 0;   /* next free byte in data pool */
static fs_node_t *root_node = NULL;

/* ------------------------------------------------------------------ */
static fs_node_t *node_alloc(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i] = (fs_node_t){0};
            nodes[i].used = true;
            return &nodes[i];
        }
    }
    return NULL;
}

static void node_free(fs_node_t *n) {
    n->used = false;
}

/* Allocate bytes from pool (no free, simple bump allocator) */
static uint32_t pool_alloc(uint32_t len) {
    if (pool_next + len > FS_DATA_POOL_SIZE) return (uint32_t)-1;
    uint32_t off = pool_next;
    pool_next += len;
    return off;
}

/* ------------------------------------------------------------------ */
void fs_init(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) nodes[i].used = false;
    pool_next = 0;

    root_node = node_alloc();
    strcpy(root_node->name, "/");
    root_node->type   = FS_DIR;
    root_node->parent = root_node;  /* root's parent is itself */
    root_node->child  = NULL;
    root_node->next   = NULL;

    /* Create /dev and /etc */
    fs_node_t *dev = fs_mkdir(root_node, "dev");
    fs_node_t *etc = fs_mkdir(root_node, "etc");
    (void)dev;

    /* /etc/motd */
    fs_node_t *motd = fs_create(etc, "motd");
    fs_write(motd,
        "Welcome to MicrOS v0.1\n"
        "Type 'help' for a list of commands.\n", 0);
}

fs_node_t *fs_root(void) { return root_node; }

/* ------------------------------------------------------------------ */
/* Look up a child by name in a directory */
static fs_node_t *dir_find(fs_node_t *dir, const char *name) {
    for (fs_node_t *c = dir->child; c; c = c->next)
        if (strcmp(c->name, name) == 0)
            return c;
    return NULL;
}

/* Resolve a path starting from base (NULL -> root) */
fs_node_t *fs_lookup(fs_node_t *base, const char *path) {
    if (!path || !*path) return base ? base : root_node;

    fs_node_t *cur = (path[0] == '/') ? root_node : (base ? base : root_node);
    if (path[0] == '/') path++;

    char seg[FS_NAME_LEN];
    while (*path) {
        /* Extract next segment */
        int i = 0;
        while (*path && *path != '/') seg[i++] = *path++;
        seg[i] = '\0';
        if (*path == '/') path++;

        if (!i || strcmp(seg, ".") == 0) continue;
        if (strcmp(seg, "..") == 0) { cur = cur->parent; continue; }
        if (cur->type != FS_DIR) return NULL;
        cur = dir_find(cur, seg);
        if (!cur) return NULL;
    }
    return cur;
}

/* ------------------------------------------------------------------ */
fs_node_t *fs_mkdir(fs_node_t *parent, const char *name) {
    if (!parent || parent->type != FS_DIR) return NULL;
    if (dir_find(parent, name)) return NULL;   /* already exists */

    fs_node_t *n = node_alloc();
    if (!n) return NULL;
    strncpy(n->name, name, FS_NAME_LEN - 1);
    n->type   = FS_DIR;
    n->parent = parent;
    n->next   = parent->child;
    parent->child = n;
    return n;
}

fs_node_t *fs_create(fs_node_t *parent, const char *name) {
    if (!parent || parent->type != FS_DIR) return NULL;
    if (dir_find(parent, name)) return NULL;

    fs_node_t *n = node_alloc();
    if (!n) return NULL;
    strncpy(n->name, name, FS_NAME_LEN - 1);
    n->type   = FS_FILE;
    n->size   = 0;
    n->data_off = pool_alloc(FS_MAX_FILE_DATA);
    n->parent = parent;
    n->next   = parent->child;
    parent->child = n;
    return n;
}

int fs_write(fs_node_t *node, const char *data, uint32_t len) {
    if (!node || node->type != FS_FILE) return -1;
    if (len == 0) len = (uint32_t)strlen(data);
    if (len > FS_MAX_FILE_DATA) len = FS_MAX_FILE_DATA;
    if (node->data_off == (uint32_t)-1) return -1;
    memcpy(data_pool + node->data_off, data, len);
    node->size = len;
    return (int)len;
}

int fs_read(fs_node_t *node, char *buf, uint32_t maxlen) {
    if (!node || node->type != FS_FILE) return -1;
    uint32_t n = (node->size < maxlen) ? node->size : maxlen;
    memcpy(buf, data_pool + node->data_off, n);
    return (int)n;
}

int fs_unlink(fs_node_t *node) {
    if (!node || node == root_node) return -1;
    if (node->type == FS_DIR && node->child) return -1; /* dir not empty */

    fs_node_t *par = node->parent;
    /* Remove from parent's child list */
    if (par->child == node) {
        par->child = node->next;
    } else {
        for (fs_node_t *c = par->child; c; c = c->next) {
            if (c->next == node) { c->next = node->next; break; }
        }
    }
    node_free(node);
    return 0;
}
