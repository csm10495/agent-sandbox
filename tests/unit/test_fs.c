/* tests/unit/test_fs.c - Tests for in-memory filesystem */
#include "framework.h"
#include "fs.h"
#include "string.h"

void suite_fs(void) {
    fs_init();

    fs_node_t *root = fs_root();
    ASSERT_NONNULL(root);
    ASSERT_EQ(root->type, FS_DIR);

    /* mkdir */
    fs_node_t *d = fs_mkdir(root, "testdir");
    ASSERT_NONNULL(d);
    ASSERT_EQ(d->type, FS_DIR);

    /* duplicate mkdir fails */
    ASSERT_NULL(fs_mkdir(root, "testdir"));

    /* create file */
    fs_node_t *f = fs_create(d, "hello.txt");
    ASSERT_NONNULL(f);
    ASSERT_EQ(f->type, FS_FILE);

    /* write and read */
    const char *msg = "Hello, MicrOS!";
    int w = fs_write(f, msg, 0);
    ASSERT_EQ(w, (int)strlen(msg));

    char rbuf[64] = {0};
    int r = fs_read(f, rbuf, sizeof(rbuf) - 1);
    ASSERT_EQ(r, (int)strlen(msg));
    ASSERT_STREQ(rbuf, msg);

    /* lookup */
    fs_node_t *found = fs_lookup(root, "testdir/hello.txt");
    ASSERT_EQ(found, f);

    fs_node_t *found2 = fs_lookup(root, "testdir");
    ASSERT_EQ(found2, d);

    /* absolute path lookup */
    fs_node_t *found3 = fs_lookup(NULL, "/testdir/hello.txt");
    ASSERT_EQ(found3, f);

    /* unlink file */
    ASSERT_EQ(fs_unlink(f), 0);
    ASSERT_NULL(fs_lookup(root, "testdir/hello.txt"));

    /* unlink non-empty dir fails */
    fs_node_t *f2 = fs_create(d, "f2.txt");
    ASSERT_NONNULL(f2);
    ASSERT_EQ(fs_unlink(d), -1);   /* dir has child */

    /* unlink child, then dir */
    ASSERT_EQ(fs_unlink(f2), 0);
    ASSERT_EQ(fs_unlink(d),  0);
    ASSERT_NULL(fs_lookup(root, "testdir"));

    /* /etc/motd was created in fs_init */
    fs_node_t *etc = fs_lookup(root, "etc");
    ASSERT_NONNULL(etc);
    fs_node_t *motd = fs_lookup(etc, "motd");
    ASSERT_NONNULL(motd);
    ASSERT_EQ(motd->type, FS_FILE);
    ASSERT_TRUE(motd->size > 0u);
}
