#include "kernel.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    char buffer[16];
    assert(strlen("") == 0);
    assert(strlen("sable") == 5);
    assert(strcmp("a", "a") == 0);
    assert(strcmp("a", "b") < 0);
    assert(strncmp("kernel", "kern", 4) == 0);
    assert(strcmp(strcpy(buffer, "thread"), "thread") == 0);

    ramfs_init();
    assert(ramfs_find("welcome.txt") != NULL);
    assert(ramfs_create("notes") == 0);
    assert(ramfs_create("notes") != 0);
    assert(ramfs_write("notes", "hello") == 0);
    assert(strcmp(ramfs_find("notes")->data, "hello") == 0);
    assert(ramfs_create("bad name") != 0);
    assert(ramfs_remove("notes") == 0);
    assert(ramfs_find("notes") == NULL);
    puts("All unit tests passed");
    return 0;
}
