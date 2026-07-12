#include "kernel.h"

static ramfs_file_t files[RAMFS_MAX_FILES];

static bool valid_name(const char *name) {
    size_t n = strlen(name);
    if (!n || n > RAMFS_NAME_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if (name[i] == '/' || name[i] == ' ') return false;
    return true;
}

void ramfs_init(void) {
    memset(files, 0, sizeof(files));
    ramfs_create("welcome.txt");
    ramfs_write("welcome.txt", "Welcome to SableOS. Type 'help' for commands.");
}

const ramfs_file_t *ramfs_find(const char *name) {
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && strcmp(files[i].name, name) == 0) return &files[i];
    return NULL;
}

int ramfs_create(const char *name) {
    if (!valid_name(name) || ramfs_find(name)) return -1;
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = true;
            files[i].size = 0;
            strcpy(files[i].name, name);
            files[i].data[0] = 0;
            return 0;
        }
    }
    return -2;
}

int ramfs_write(const char *name, const char *data) {
    ramfs_file_t *file = (ramfs_file_t *)ramfs_find(name);
    if (!file) return -1;
    size_t n = strlen(data);
    if (n > RAMFS_DATA_MAX) return -2;
    memcpy(file->data, data, n + 1);
    file->size = n;
    return 0;
}

int ramfs_remove(const char *name) {
    ramfs_file_t *file = (ramfs_file_t *)ramfs_find(name);
    if (!file) return -1;
    memset(file, 0, sizeof(*file));
    return 0;
}

const ramfs_file_t *ramfs_at(size_t index) {
    return index < RAMFS_MAX_FILES && files[index].used ? &files[index] : NULL;
}
