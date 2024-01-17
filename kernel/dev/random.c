
/*
 * dev/random.c - Null Device
 *
 * A device which generates an indefinite number of random bytes.
 */

#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>

static int Read(struct vnode*, struct transfer* io) {    
    while (io->length_remaining > 0) {
        uint8_t random_byte = rand() & 0xFF;
        int err = PerformTransfer(&random_byte, io, 1);
        if (err) {
            return err;
        }
    }

    return 0;
}

static const struct vnode_operations dev_ops = {
    .read = Read,
};

void InitRandomDevice(void)
{
    AddVfsMount(CreateVnode(dev_ops, (struct stat) {
        .st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO, 
        .st_nlink = 1
    }), "rand");
}
