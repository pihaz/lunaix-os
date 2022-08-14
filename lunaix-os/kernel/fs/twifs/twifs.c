/**
 * @file twifs.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief TwiFS - A pseudo file system for kernel state exposure.
 * @version 0.1
 * @date 2022-07-21
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

static struct twifs_node* fs_root;

static struct cake_pile* twi_pile;

int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode);

int
__twifs_openfile(struct v_inode* inode, struct v_file* file);

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name);

struct v_inode*
__twifs_create_inode(struct twifs_node* twi_node);

int
__twifs_iterate_dir(struct v_inode* inode, struct dir_context* dctx);

int
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point);

int
__twifs_mkdir(struct v_inode* inode, struct v_dnode* dnode);

int
__twifs_rmstuff(struct v_inode* inode);

int
__twifs_fwrite(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

int
__twifs_fread(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

void
twifs_init()
{
    twi_pile = cake_new_pile("twifs_node", sizeof(struct twifs_node), 1, 0);

    struct filesystem* twifs = vzalloc(sizeof(struct filesystem));
    twifs->fs_name = HSTR("twifs", 5);
    twifs->mount = __twifs_mount;
    twifs->types = FSTYPE_ROFS;

    fsm_register(twifs);

    fs_root = twifs_dir_node(NULL, NULL, 0, 0);
}

struct twifs_node*
__twifs_new_node(struct twifs_node* parent,
                 const char* name,
                 int name_len,
                 uint32_t itype)
{
    struct twifs_node* node = cake_grab(twi_pile);
    memset(node, 0, sizeof(*node));

    node->name = HSTR(name, name_len);
    node->itype = itype;
    hstr_rehash(&node->name, HSTR_FULL_HASH);
    llist_init_head(&node->children);

    if (parent) {
        llist_append(&parent->children, &node->siblings);
    }

    return node;
}

int
twifs_rm_node(struct twifs_node* node)
{
    if ((node->itype & VFS_IFDIR) && !llist_empty(&node->children)) {
        return ENOTEMPTY;
    }
    llist_delete(&node->siblings);
    vfs_i_free(node->inode);
    cake_release(twi_pile, node);
    return 0;
}

struct twifs_node*
twifs_file_node(struct twifs_node* parent,
                const char* name,
                int name_len,
                uint32_t itype)
{
    struct twifs_node* twi_node =
      __twifs_new_node(parent, name, name_len, VFS_IFFILE | itype);

    struct v_inode* twi_inode = __twifs_create_inode(twi_node);
    twi_node->inode = twi_inode;

    return twi_node;
}

struct twifs_node*
twifs_dir_node(struct twifs_node* parent,
               const char* name,
               int name_len,
               uint32_t itype)
{
    struct hstr hname = HSTR(name, name_len);
    hstr_rehash(&hname, HSTR_FULL_HASH);
    struct twifs_node* node = __twifs_get_node(parent, &hname);
    if (node) {
        return node;
    }

    struct twifs_node* twi_node =
      __twifs_new_node(parent, name, name_len, VFS_IFDIR | itype);

    struct v_inode* twi_inode = __twifs_create_inode(twi_node);
    twi_node->inode = twi_inode;

    return twi_node;
}

struct twifs_node*
twifs_toplevel_node(const char* name, int name_len, uint32_t itype)
{
    return twifs_dir_node(fs_root, name, name_len, itype);
}

int
__twifs_mkdir(struct v_inode* inode, struct v_dnode* dnode)
{
    struct twifs_node* parent_node = (struct twifs_node*)inode->data;
    if (!(parent_node->itype & VFS_IFDIR)) {
        return ENOTDIR;
    }
    struct twifs_node* new_node =
      twifs_dir_node(parent_node, dnode->name.value, dnode->name.len, 0);
    dnode->inode = new_node->inode;

    return 0;
}

int
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    mount_point->inode = fs_root->inode;
    return 0;
}

struct v_inode*
__twifs_create_inode(struct twifs_node* twi_node)
{
    struct v_inode* inode = vfs_i_alloc();
    inode->itype = twi_node->itype;
    inode->data = twi_node;

    inode->ctime = clock_unixtime();
    inode->atime = inode->ctime;
    inode->mtime = inode->ctime;

    inode->ops.dir_lookup = __twifs_dirlookup;
    inode->ops.mkdir = __twifs_mkdir;
    inode->ops.unlink = __twifs_rmstuff;
    inode->ops.rmdir = __twifs_rmstuff;
    inode->ops.open = __twifs_openfile;

    inode->default_fops = (struct v_file_ops){ .read = __twifs_fread,
                                               .write = __twifs_fwrite,
                                               .readdir = __twifs_iterate_dir };

    return inode;
}

int
__twifs_fwrite(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.write) {
        return ENOTSUP;
    }
    return twi_node->ops.write(inode, buffer, len, fpos);
}

int
__twifs_fread(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.read) {
        return ENOTSUP;
    }
    return twi_node->ops.read(inode, buffer, len, fpos);
}

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name)
{
    if (!parent)
        return NULL;

    struct twifs_node *pos, *n;
    llist_for_each(pos, n, &parent->children, siblings)
    {
        if (HSTR_EQ(&pos->name, name)) {
            return pos;
        }
    }
    return NULL;
}

int
__twifs_rmstuff(struct v_inode* inode)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    return twifs_rm_node(twi_node);
}

int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;

    if (!(twi_node->itype & VFS_IFDIR)) {
        return ENOTDIR;
    }

    struct twifs_node* child_node = __twifs_get_node(twi_node, &dnode->name);
    if (child_node) {
        dnode->inode = child_node->inode;
        return 0;
    }
    return ENOENT;
}

int
__twifs_iterate_dir(struct v_inode* inode, struct dir_context* dctx)
{
    struct twifs_node* twi_node = (struct twifs_node*)(inode->data);
    int counter = 0;
    struct twifs_node *pos, *n;

    llist_for_each(pos, n, &twi_node->children, siblings)
    {
        if (counter++ >= dctx->index) {
            dctx->index = counter;
            dctx->read_complete_callback(
              dctx, pos->name.value, pos->name.len, pos->itype);
            return 0;
        }
    }

    return 1;
}

int
__twifs_openfile(struct v_inode* inode, struct v_file* file)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (twi_node) {
        return 0;
    }
    return ENOTSUP;
}
