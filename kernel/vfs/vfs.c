#include <vfs.h>
#include <spinlock.h>
#include <irql.h>
#include <log.h>
#include <assert.h>
#include <virtual.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <heap.h>
#include <avl.h>
#include <fcntl.h>
#include <stackadt.h>

/*
* Try not to have non-static functions that return in any way a struct vnode*, as it
* probably means you need to use the reference/dereference functions.
*/

/*
* Maximum length of a component of a filepath (e.g. an file/directory's individual name).
*/
#define MAX_COMPONENT_LENGTH	128

/*
* Maximum total length of a path.
*/
#define MAX_PATH_LENGTH			2000

/*
* Maximum number of symbolic links to derefrence in a path before returning ELOOP.
*/
#define MAX_LOOP				5


/*
* A structure for mounted devices and filesystems.
*/
struct mounted_file {
	/* The vnode representing the device / root directory of a filesystem */
	struct open_file* node;

	/* What the device / filesystem mount is called */
	char* name;
};

static struct spinlock vfs_lock;
static struct avl_tree* mount_points = NULL;

int MountedDeviceComparator(void* a_, void* b_) {
    struct mounted_file* a = a_;
    struct mounted_file* b = b_;
    return strcmp(a->name, b->name);
}

void InitVfs(void) {
    InitSpinlock(&vfs_lock, "vfs", IRQL_SCHEDULER);
    mount_points = AvlTreeCreate();
    AvlTreeSetComparator(mount_points, MountedDeviceComparator);
}

static int CheckValidComponentName(const char* name)
{
	assert(name != NULL);
	
	if (name[0] == 0) {
		return EINVAL;
	}

	for (int i = 0; name[i]; ++i) {
		char c = name[i];

		if (c == '/' || c == '\\' || c == ':') {
			return EINVAL;
		}
	}

	return 0;
}

static int DoesMountPointExist(const char* name) {
    assert(name != NULL);
    assert(IsSpinlockHeld(&vfs_lock));

    struct mounted_file target;
    target.name = (char*) name;
    if (AvlTreeContains(mount_points, &target)) {
        return EEXIST;
    }

    return 0;
}

/*
* Given a filepath, and a pointer to an index within that filepath (representing where
* start searching), copies the next component into an output buffer of a given length.
* The index is updated to point to the start of the next component, ready for the next call.
*
* This also handles duplicated and trailing forward slashes.
*/
static int GetPathComponent(const char* path, int* ptr, char* output_buffer, int max_output_length, char delimiter) {
	int i = 0;

	output_buffer[0] = 0;

	while (path[*ptr] && path[*ptr] != delimiter) {
		if (i >= max_output_length - 1) {
			return ENAMETOOLONG;
		}

		output_buffer[i++] = path[*ptr];
		output_buffer[i] = 0;
		(*ptr)++;
	}

	/*
	* Skip past the delimiter (unless we are at the end of the string),
	* as well as any trailing slashes (which could be after a slash delimiter, 
	* or after a colon). 
	*/
	if (path[*ptr]) {
		do {
			(*ptr)++;
		} while (path[*ptr] == '/');
	}

	/*
	* Ensure that there are no colons or backslashes in the filename itself.
	*/
	return CheckValidComponentName(output_buffer);
}

static int GetFinalPathComponent(const char* path, char* output_buffer, int max_output_length) {
	int path_ptr = 0;

	int status = GetPathComponent(path, &path_ptr, output_buffer, max_output_length, ':');
	if (status) {
		return status;
	}

	while (path_ptr < (int) strlen(path)) {
		status = GetPathComponent(path, &path_ptr, output_buffer, max_output_length, '/');
		if (status) {
			return status;
		}
	}

	return 0;
}

/*
* Takes in a device name, without the colon, and returns its vnode.
* If no such device is mounted, it returns NULL.
*/
static struct open_file* GetMountFromName(const char* name) {
	assert(name != NULL);

	if (mount_points == NULL) {
		return NULL;
	}

	struct mounted_file target;
    target.name = (char*) name;
    struct mounted_file* mount = AvlTreeGet(mount_points, (void*) &target);
    return mount->node;
}

int AddVfsMount(struct vnode* node, const char* name) {
    EXACT_IRQL(IRQL_STANDARD);   

    if (name == NULL || node == NULL) {
		return EINVAL;
	}

	if (strlen(name) >= MAX_COMPONENT_LENGTH) {
		return ENAMETOOLONG;
	}

    int status = CheckValidComponentName(name);
	if (status != 0) {
		return status;
	}

    AcquireSpinlockIrql(&vfs_lock);

    if (DoesMountPointExist(name) == EEXIST) {
        ReleaseSpinlockIrql(&vfs_lock);
        return EEXIST;
    }

    struct mounted_file* mount = AllocHeap(sizeof(struct mounted_file));
	mount->name = strdup(name);
    mount->node = CreateOpenFile(node, 0, 0, true, true);

    AvlTreeInsert(mount_points, (void*) mount);

	LogWriteSerial("MOUNTED TO THE VFS: %s\n", name);

    ReleaseSpinlockIrql(&vfs_lock);
    return 0;
}

int RemoveVfsMount(const char* name) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    if (name == NULL) {
		return EINVAL;
	}

	if (CheckValidComponentName(name) != 0) {
		return EINVAL;
	}

	AcquireSpinlockIrql(&vfs_lock);

	/*
	* Scan through the mount table for the device
	*/ 
	struct mounted_file target;
    target.name = (char*) name;

    struct mounted_file* actual = AvlTreeGet(mount_points, &target);
    if (actual == NULL) {
        ReleaseSpinlockIrql(&vfs_lock);
        return ENODEV;
    }

    assert(!strcmp(actual->name, name));

    /*
     * Decrement the reference that was initially created way back in
     * vfs_add_device in the call to dev_create_vnode (the vnode dereference),
     * and then the open file that was created alongside it.
     */
    DereferenceVnode(actual->node->node);
    DereferenceOpenFile(actual->node);

    AvlTreeDelete(mount_points, actual);
    FreeHeap(actual->name);

    ReleaseSpinlockIrql(&vfs_lock);
    return 0;
}

static void CleanupVnodeStack(struct stack_adt* stack) {
	while (StackAdtSize(stack) > 0) {
		struct vnode* node = StackAdtPop(stack);
		DereferenceVnode(node);
	}

	StackAdtDestroy(stack);
}

/*
* Given an absolute filepath, returns the vnode representing
* the file, directory or device. 
*
* Should be used carefully, as the reference count is incremented.
*/
static int GetVnodeFromPath(const char* path, struct vnode** out, bool want_parent) {
	assert(path != NULL);
	assert(out != NULL);

	if (strlen(path) == 0) {
		return EINVAL;
	}
	if (strlen(path) >= MAX_PATH_LENGTH) {
		return ENAMETOOLONG;
	}

	int path_ptr = 0;
	char component_buffer[MAX_COMPONENT_LENGTH];

	int err = GetPathComponent(path, &path_ptr, component_buffer, MAX_COMPONENT_LENGTH, ':');
	if (err != 0) {
		return err;
	}

	struct open_file* current_file = GetMountFromName(component_buffer);

	/*
	* No root device found, so we can't continue.
	*/
	if (current_file == NULL || current_file->node == NULL) {
		return ENODEV;
	}

	struct vnode* current_vnode = current_file->node;

	/*
	* This will be dereferenced either as we go through the loop, or
	* after a call to vfs_close (this function should only be called 
	* by vfs_open).
	*/
	ReferenceVnode(current_vnode);

	char component[MAX_COMPONENT_LENGTH + 1];

	/*
	* To go back to a parent directory, we need to keep track of the previous component.
	* As we can go back through many parents, we must keep track of all of them, hence we
	* use a stack to store each vnode we encounter. We will not dereference the vnodes
	* on the stack until the end using cleanup_vnode_stack.
	*/
	struct stack_adt* previous_components = StackAdtCreate();
	
	/*
	* Iterate over the rest of the path.
	*/
	while (path_ptr < (int) strlen(path)) {
		int status = GetPathComponent(path, &path_ptr, component, MAX_COMPONENT_LENGTH, '/');
		if (status != 0) {
			DereferenceVnode(current_vnode);
			CleanupVnodeStack(previous_components);
			return status;
		}

		if (!strcmp(component, ".")) {
			/*
			* This doesn't change where we point to.
			*/
			continue;

		} else if (!strcmp(component, "..")) {
			if (StackAdtSize(previous_components) == 0) {
				/*
				* We have reached the root. Going 'further back' than the root 
				* on Linux just keeps us at the root, so don't do anything here.
				*/
			
			} else {
				/*
				* Pop the previous component and use it.
				*/
				current_vnode = StackAdtPop(previous_components);
			}

			continue;
		}

		/*
		* Use a seperate pointer so that both inputs don't point to the same
		* location. vnode_follow either increments the reference count or creates
		* a new vnode with a count of one.
		*/
		struct vnode* next_vnode = NULL;
		status = VnodeOpFollow(current_vnode, &next_vnode, component);
		if (status != 0) {
			DereferenceVnode(current_vnode);
			CleanupVnodeStack(previous_components);
			return status;
		}	
		
		/*
		* We have a component that can be backtracked to, so add it to the stack.
		* 
		* Also note that vnode_follow adds a reference count, so current_vnode
		* needs to be dereferenced. Conveniently, all components that need to be
		* put on the stack also need dereferencing, and vice versa. 
		*
		* The final vnode we find will not be added to the stack and dereferenced
		* as we won't get here.
		*/
		StackAdtPush(previous_components, current_vnode);
		current_vnode = next_vnode;
	}

	int status = 0;

	if (want_parent) {
		/*
		* Operations that require us to get the parent don't work if we are already
		* at the root.
		*/
		if (StackAdtSize(previous_components) == 0) {
			status = EINVAL;

		} else {
			*out = StackAdtPop(previous_components);
		}
	} else {
		*out = current_vnode;
	}

	CleanupVnodeStack(previous_components);
	return status;
}

int RemoveFileOrDirectory(const char* path, bool rmdir) {
	struct vnode* node;
	int res = GetVnodeFromPath(path, &node, false);
	if (res != 0) {
		return res;
	}

	bool is_dir = IFTODT(node->stat.st_mode) == DT_DIR;
	if (rmdir && !is_dir) return ENOTDIR;
	if (!rmdir && is_dir) return EISDIR;

	if (rmdir) {
		res = VnodeOpDelete(node);

	} else {
		res = node->stat.st_nlink > 0 ? VnodeOpUnlink(node) : ENOENT;
	}

	DereferenceVnode(node);
	return res;
}

int OpenFile(const char* path, int flags, mode_t mode, struct open_file** out) {
    EXACT_IRQL(IRQL_STANDARD);   

 	if (path == NULL || out == NULL || strlen(path) <= 0) {
		return EINVAL;
	}

    int status;
    char name[MAX_COMPONENT_LENGTH + 1];
	status = GetFinalPathComponent(path, name, MAX_COMPONENT_LENGTH);
	if (status) {
		return status;
	}

    /*
	* Grab the vnode from the path.
	*/
	struct vnode* node;

    /*
    * Check if there is an existing file here.
    */
    status = GetVnodeFromPath(path, &node, false);

    if (flags & O_CREAT) {
		if (status == ENOENT) {
			/*
			* Get the parent folder.
			*/
			status = GetVnodeFromPath(path, &node, true);
			if (status) {
				return status;
			}
			
			struct vnode* child;
			status = VnodeOpCreate(node, &child, name, flags, mode);
			DereferenceVnode(node);

			if (status) {
				return status;
			}

			node = child;

		} else if (flags & O_EXCL) {
			/*
			 * The file already exists (as we didn't get ENOENT), but we were passed O_EXCL so we
			 * must give an error. If O_EXCL isn't passed, then O_CREAT will just open the existing file.
			 */
			return EEXIST;
		}

    } else if (status != 0) {
        return status;
    }

	status = VnodeOpCheckOpen(node, name, flags & (O_ACCMODE | O_NONBLOCK));
    if (status) {
		DereferenceVnode(node);
		return status;
	}

	bool can_read = (flags & O_ACCMODE) != O_WRONLY;
	bool can_write = (flags & O_ACCMODE) != O_RDONLY;
	uint8_t dirent_type = VnodeOpDirentType(node);

	if (dirent_type == DT_DIR && can_write) {
		/*
		* You cannot write to a directory. This also prevents truncation.
		*/
		DereferenceVnode(node);
		return EISDIR;
	}

	if ((flags & O_TRUNC) && dirent_type == DT_REG) {
		if (can_write) {
			status = VnodeOpTruncate(node, 0);
			if (status) {
				return status;
			}
			return ENOSYS;
		} else {
			return EINVAL;
		}
	}

    /* TODO: clear out the flags that don't normally get saved */

	// TODO: may need to actually have a VnodeOpOpen, for things like FatFS.

	*out = CreateOpenFile(node, mode, flags, can_read, can_write);
    return 0;
}


static int FileAccess(struct open_file* file, struct transfer* io, bool write) {
	EXACT_IRQL(IRQL_STANDARD);

    if (io == NULL || io->address == NULL || file == NULL || file->node == NULL) {
		return EINVAL;
	}
    if ((!write && !file->can_read) || (write && !file->can_write)) {
        return EBADF;
    }
	
	if (file->flags & O_NONBLOCK) {
		int block_status = VnodeOpWait(file->node, (write ? VNODE_WAIT_WRITE : VNODE_WAIT_READ) | VNODE_WAIT_NON_BLOCK, 0);
		if (block_status != 0) {
			return block_status;
		}
	}

	if (write) {
		return VnodeOpWrite(file->node, io);
	} else {
		return VnodeOpRead(file->node, io);
	}
}

int ReadFile(struct open_file* file, struct transfer* io) {
	return FileAccess(file, io, false);
}

int WriteFile(struct open_file* file, struct transfer* io) {
	return FileAccess(file, io, true);
}

int CloseFile(struct open_file* file) {
	EXACT_IRQL(IRQL_STANDARD);

    if (file == NULL || file->node == NULL) {
		return EINVAL;
	}

    DereferenceVnode(file->node);
	DereferenceOpenFile(file);
	return 0;
}