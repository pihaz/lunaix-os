#include <hal/ahci/hba.h>
#include <klibc/stdio.h>
#include <klibc/string.h>
#include <lib/crc.h>
#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

#include <lunaix/spike.h>

#define BLOCK_EREAD 1
#define BLOCK_ESIG 2
#define BLOCK_ECRC 3
#define BLOCK_EFULL 3

LOG_MODULE("BLOCK")

#define MAX_DEV 32

static struct cake_pile* lbd_pile;
static struct block_dev** dev_registry;
static struct twifs_node* blk_sysroot;

int free_slot = 0;

int
__block_mount_partitions(struct hba_device* hd_dev);

int
__block_register(struct block_dev* dev);

void
block_init()
{
    blkio_init();
    lbd_pile = cake_new_pile("block_dev", sizeof(struct block_dev), 1, 0);
    dev_registry = vcalloc(sizeof(struct block_dev*), MAX_DEV);
    free_slot = 0;
    blk_sysroot = twifs_dir_node(NULL, "block");
}

int
__block_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    int errno;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t bsize = bdev->blk_size, rd_block = offset / bsize,
           r = offset % bsize, rd_size = 0;

    if (!(len = MIN(len, ((size_t)bdev->end_lba - rd_block + 1) * bsize))) {
        return 0;
    }

    struct vecbuf* vbuf = vbuf_alloc(NULL, buf, len);
    struct blkio_req* req;
    void* tmp_buf = NULL;

    if (r) {
        tmp_buf = vzalloc(bsize);
        rd_size = MIN(len, bsize - r);
        vbuf->buf.size = rd_size;
        vbuf->buf.buffer = tmp_buf;

        vbuf_alloc(vbuf, buf + rd_size, len - rd_size);
    }

    req = blkio_vrd(vbuf, rd_block, NULL, NULL, 0);
    blkio_commit(bdev->blkio, req);

    pwait(&req->wait);

    if (!(errno = req->errcode)) {
        memcpy(buf, tmp_buf + r, rd_size);
        errno = len;
    } else {
        errno = -errno;
    }

    if (tmp_buf) {
        vfree(tmp_buf);
    }

    blkio_free_req(req);
    vbuf_free(vbuf);
    return errno;
}

int
__block_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t bsize = bdev->blk_size, rd_block = offset / bsize,
           r = offset % bsize;

    if (!(len = MIN(len, ((size_t)bdev->end_lba - rd_block + 1) * bsize))) {
        return 0;
    }

    struct vecbuf* vbuf = vbuf_alloc(NULL, buf, len);
    struct blkio_req* req;
    void* tmp_buf = NULL;

    if (r) {
        size_t rd_size = MIN(len, bsize - r);
        tmp_buf = vzalloc(bsize);
        vbuf->buf.size = bsize;
        vbuf->buf.buffer = tmp_buf;

        memcpy(tmp_buf + r, buf, rd_size);
        vbuf_alloc(vbuf, buf + rd_size, len - rd_size);
    }

    req = blkio_vwr(vbuf, rd_block, NULL, NULL, 0);
    blkio_commit(bdev->blkio, req);

    pwait(&req->wait);

    int errno = req->errcode;
    if (!errno) {
        errno = len;
    } else {
        errno = -errno;
    }

    if (tmp_buf) {
        vfree(tmp_buf);
    }

    blkio_free_req(req);
    vbuf_free(vbuf);
    return errno;
}

struct block_dev*
block_alloc_dev(const char* blk_id, void* driver, req_handler ioreq_handler)
{
    struct block_dev* bdev = cake_grab(lbd_pile);
    memset(bdev, 0, sizeof(struct block_dev));

    strncpy(bdev->name, blk_id, PARTITION_NAME_SIZE);

    bdev->blkio = blkio_newctx(ioreq_handler);
    bdev->driver = driver;
    bdev->blkio->driver = driver;

    return bdev;
}

int
block_mount(struct block_dev* bdev, devfs_exporter fs_export)
{
    int errno = 0;

    if (!__block_register(bdev)) {
        errno = BLOCK_EFULL;
        goto error;
    }

    struct twifs_node* dev_root = twifs_dir_node(blk_sysroot, bdev->bdev_id);
    blk_set_blkmapping(bdev, dev_root);
    fs_export(bdev, dev_root);

    return errno;

error:
    kprintf(KERROR "Fail to mount block device: %s (%x)\n", bdev->name, -errno);
    return errno;
}

int
__block_register(struct block_dev* bdev)
{
    if (free_slot >= MAX_DEV) {
        return 0;
    }

    struct device* dev = device_addvol(NULL, bdev, "sd%c", 'a' + free_slot);
    dev->write = __block_write;
    dev->read = __block_read;

    bdev->dev = dev;
    strcpy(bdev->bdev_id, dev->name_val);
    dev_registry[free_slot++] = bdev;
    return 1;
}