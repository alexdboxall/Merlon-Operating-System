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
#include <process.h>
#include <tree.h>
#include <fcntl.h>
#include <linkedlist.h>
#include <thread.h>
#include <stackadt.h>

/*
* Try not to have non-static functions that return in any way a struct vnode*, as it
* probably means you need to use the reference/dereference functions.
*/

#define MAX_COMPONENT_LENGTH	128
#define MAX_PATH_LENGTH			2000
#define MAX_LOOP				5

/*
* A structure for mounted devices and filesystems.
*/
struct mounted_file {
	/* The vnode representing the device / root directory of a filesystem */
	struct file* node;

	/* What the device / filesystem mount is called */
	char* name;
};

static struct spinlock vfs_lock;
static struct linked_list* mount_points = NULL;

struct mounted_file* GetMountPointFromName(const char* name) {
	if (mount_points == NULL) {
		return NULL;
	}
	struct linked_list_node* iter = ListGetFirstNode(mount_points);
	while (iter != NULL) {
		struct mounted_file* data = ListGetDataFromNode(iter);
		if (!strcmp(name, data->name)) {
			return data;
		}
		iter = ListGetNextNode(iter);
	}
	return NULL;
}

int RootsRead(struct vnode*, struct transfer* io) {
	LogWriteSerial("RootsRead. offset = %d\n", io->offset);
	LogWriteSerial("io->addr 0x%X. dir 0x%X. len 0x%X. type 0x%X.\n", io->address, io->direction, io->length_remaining, io->type);
	if (io->offset % sizeof(struct dirent) != 0) {
		return EINVAL;
	}

	int index = io->offset / sizeof(struct dirent);
	if (index == 0 || index == 1) {
		struct dirent dir;
		dir.d_ino = 0;
		strcpy(dir.d_name, index == 0 ? "." : "..");
		dir.d_namlen = strlen(dir.d_name);
		dir.d_type = DT_DIR;
		return PerformTransfer(&dir, io, sizeof(struct dirent));
	}

	struct mounted_file* root = ListGetDataAtIndex(mount_points, index - 2);
	if (root == NULL) {
		return ENOENT;
	}

	struct dirent dir;
	dir.d_type = IFTODT(root->node->node->stat.st_mode);
	dir.d_ino = root->node->node->stat.st_ino;
	dir.d_namlen = strlen(root->name);
	strncpy(dir.d_name, root->name, sizeof(dir.d_name));
	dir.d_name[sizeof(dir.d_name) - 1] = 0;
	return PerformTransfer(&dir, io, sizeof(struct dirent));
}

int RootsFollow(struct vnode* node, struct vnode** output, const char* name) {
	if (!strcmp(name, "..")) {
		*output = node;
		return 0;
	}
	struct mounted_file* root = GetMountPointFromName(name);
	if (root == NULL) {
		return ENOENT;
	}
	*output = root->node->node;
    return 0;
}

void InitRootsFilesystem(void) {
	struct vnode_operations dev_ops = {
		.read           = RootsRead,
		.follow         = RootsFollow,
	};

	AddVfsMount(CreateVnode(dev_ops, (struct stat) {
        .st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO,
        .st_nlink = 1,
    }), "*");
}

void InitVfs(void) {
    InitSpinlock(&vfs_lock, "vfs", IRQL_SCHEDULER);
    mount_points = ListCreate();
}

static bool IsRelative(const char* path) {
	for (int i = 0; path[i]; ++i) {
		if (path[i] == ':') {
			return false;
		}
	}
	return true;
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

    if (GetMountPointFromName(name) != NULL) {
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
static int GetPathComponent(const char* path, int* ptr, char* out, int max_len, char delimiter) {
	int i = 0;
	LogWriteSerial(" GetPathComponent --> %s --> ", path);

	out[0] = 0;

	while (path[*ptr] && path[*ptr] != delimiter) {
		if (i >= max_len - 1) {
			return ENAMETOOLONG;
		}

		out[i++] = path[*ptr];
		out[i] = 0;
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

	LogWriteSerial("%s\n ", out);

	/*
	* Ensure that there are no colons or backslashes in the filename itself.
	*/
	return CheckValidComponentName(out);
}

static int GetFinalPathComponent(const char* path, char* out, int max_len) {
	int path_ptr = 0;
	int status;

	// @@@ TODO: 
	if (!IsRelative(path)) {
		status = GetPathComponent(path, &path_ptr, out, max_len, ':');
		if (status) {
			LogWriteSerial("BAD C: %d\n", status);
			return status;
		}
	}

	while (path_ptr < (int) strlen(path)) {
		status = GetPathComponent(path, &path_ptr, out, max_len, '/');
		if (status) {
			LogWriteSerial("BAD D: %d\n", status);
			return status;
		}
	}

	return 0;
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

    AcquireSpinlock(&vfs_lock);

    if (DoesMountPointExist(name) == EEXIST) {
        ReleaseSpinlock(&vfs_lock);
        return EEXIST;
    }

    struct mounted_file* mount = AllocHeap(sizeof(struct mounted_file));
	mount->name = strdup(name);
    mount->node = CreateFile(node, 0, 0, true, true);

	ListInsertEnd(mount_points, (void*) mount);

	LogWriteSerial("MOUNTED TO THE VFS: %s\n", name);

    ReleaseSpinlock(&vfs_lock);
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

	AcquireSpinlock(&vfs_lock);

	/*
	* Scan through the mount table for the device
	*/ 
    struct mounted_file* actual = GetMountPointFromName(name);
    if (actual == NULL) {
        ReleaseSpinlock(&vfs_lock);
        return ENODEV;
    }

    assert(!strcmp(actual->name, name));

    /*
     * Decrement the reference that was initially created way back in
     * vfs_add_device in the call to dev_create_vnode (the vnode dereference),
     * and then the open file that was created alongside it.
     */
    DereferenceVnode(actual->node->node);
    DereferenceFile(actual->node);

	ListDeleteData(mount_points, actual);
    FreeHeap(actual->name);

    ReleaseSpinlock(&vfs_lock);
    return 0;
}

/*
* Given an absolute or relative filepath, returns the vnode representing
* the file, directory or device. 
*
* Should be used carefully, as the reference count is incremented.
*/
static int GetVnodeFromPath(const char* path, struct vnode** out, bool want_parent) {
	assert(path != NULL);
	assert(out != NULL);

	LogWriteSerial("GetVnodeFromPath: %s\n", path);

	if (strlen(path) == 0) {
		return EINVAL;
	}
	if (strlen(path) >= MAX_PATH_LENGTH) {
		return ENAMETOOLONG;
	}

	int path_ptr = 0;
	char component_buffer[MAX_COMPONENT_LENGTH];

	bool relative = IsRelative(path);

	int err;
	struct vnode* current_vnode = NULL;
	if (relative) {
		struct process* prcss = GetProcess();
		if (prcss == NULL) {
			return ENODEV;
		}
		current_vnode = prcss->cwd;
		if (current_vnode == NULL) {
			LogWriteSerial("*** *!* *!* *!* *** No cwd...\n");
		}

	} else {
		err = GetPathComponent(path, &path_ptr, component_buffer, MAX_COMPONENT_LENGTH, ':');
		if (err != 0) {
			return err;
		}

		struct mounted_file* mount = GetMountPointFromName(component_buffer);
		struct file* current_file = mount == NULL ? NULL : mount->node;
		if (current_file == NULL) {
			return ENODEV;
		}
		current_vnode = current_file->node;
		LogWriteSerial("Got mount point: %s, 0x%X, 0x%X, 0x%X\n", component_buffer, mount, current_file, current_vnode);
	}
	
	if (current_vnode == NULL) {
		return ENODEV;
	}

	/*
	* This will be dereferenced either as we go through the loop, or
	* after a call to vfs_close (this function should only be called 
	* by vfs_open).
	*/
	ReferenceVnode(current_vnode);

	char component[MAX_COMPONENT_LENGTH + 1];

	/*
	* Iterate over the rest of the path.
	*/
	while (path_ptr < (int) strlen(path)) {
		LogWriteSerial(" ==> path %s\n", path);
		int status = GetPathComponent(path, &path_ptr, component, MAX_COMPONENT_LENGTH, '/');
		if (status != 0) {
			DereferenceVnode(current_vnode);
			return status;
		}

		LogWriteSerial("PATH ITER: %s [of %s]\n", component, path);

		if (!strcmp(component, ".")) {
			/*
			* This doesn't change where we point to.
			*/
			continue;
		} 

		/* 
		 * No need for ".." here, the filesystem itself handles that. This is
		 * needed for relative directories to work - as only the filesytem
		 * itself can get to the parent from merely a vnode.
		 */

		/*
		* Use a seperate pointer so that both inputs don't point to the same
		* location. vnode_follow either increments the reference count or creates
		* a new vnode with a count of one.
		*/
		struct vnode* next_vnode = NULL;
		status = VnodeOpFollow(current_vnode, &next_vnode, component);
		if (status != 0) {
			DereferenceVnode(current_vnode);
			return status;
		}	
		current_vnode = next_vnode;
	}

	if (want_parent) {
		struct vnode* parent;
		int status = VnodeOpFollow(current_vnode, &parent, "..");
		DereferenceVnode(current_vnode);
		if (status != 0) {
			return status;
		}
		*out = parent;

	} else {
		*out = current_vnode;
	}

	return 0;
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

int OpenFile(const char* path, int flags, mode_t mode, struct file** out) {
    EXACT_IRQL(IRQL_STANDARD);  

	LogWriteSerial("OPEN FILE: %s\n", path); 

 	if (path == NULL || out == NULL || strlen(path) <= 0) {
		LogWriteSerial("BAD A %d\n", 0);
		return EINVAL;
	}

    int status;
	struct vnode* node;
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
			
			char name[MAX_COMPONENT_LENGTH + 1];
			LogDeveloperWarning("GetFinalPathComponent probably needs to be fixed");
			status = GetFinalPathComponent(path, name, MAX_COMPONENT_LENGTH);
			LogWriteSerial("--> CREATING A FILE, WITH NAME %s\n", name);
			if (status) {
				LogWriteSerial("BAD B %d\n", status);
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
			 * The file already exists (as we didn't get ENOENT), but we were 
			 * passed O_EXCL so we must give an error. If O_EXCL isn't passed, 
			 * then O_CREAT will just open the existing file.
			 */
			return EEXIST;
		}

    } else if (status != 0) {
		return status;
    }

	status = VnodeOpCheckOpen(node, flags & (O_ACCMODE | O_NONBLOCK));
    if (status) {
		DereferenceVnode(node);
		return status;
	}

	bool can_read = (flags & O_ACCMODE) != O_WRONLY;
	bool can_write = (flags & O_ACCMODE) != O_RDONLY;

	if (IFTODT(node->stat.st_mode) == DT_DIR && can_write) {
		/*
		* You cannot write to a directory - this also prevents truncation.
		*/
		DereferenceVnode(node);
		return EISDIR;
	}

	if ((flags & O_TRUNC) && IFTODT(node->stat.st_mode) == DT_REG) {
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

	// TODO: may need to actually have a VnodeOpOpen, for things like FatFS.

	node->flags = flags & (O_NONBLOCK | O_APPEND);
	*out = CreateFile(node, mode, flags & O_CLOEXEC, can_read, can_write);
    return 0;
}

static int FileAccess(struct file* file, struct transfer* io, bool write) {
	EXACT_IRQL(IRQL_STANDARD);

    if (io == NULL || io->address == NULL || file == NULL || file->node == NULL) {
		return EINVAL;
	}
    if ((!write && !file->can_read) || (write && !file->can_write)) {
        return EBADF;
    }
	
	io->blockable = !(file->node->flags & O_NONBLOCK);
	return (write ? VnodeOpWrite : VnodeOpRead)(file->node, io);
}

int ReadFile(struct file* file, struct transfer* io) {
	return FileAccess(file, io, false);
}

int WriteFile(struct file* file, struct transfer* io) {
	return FileAccess(file, io, true);
}

int CloseFile(struct file* file) {
	EXACT_IRQL(IRQL_STANDARD);

    if (file == NULL || file->node == NULL) {
		return EINVAL;
	}

    DereferenceVnode(file->node);
	DereferenceFile(file);
	return 0;
}

/*
 * Sets the current working directory of the current process. The node should 
 * be open, and may be safely closed after a call to this function, as the 
 * kernel maintains a references to the working directory.
 */
int SetWorkingDirectory(struct vnode* node) {
	struct process* prcss = GetProcess();
	if (prcss == NULL || node == NULL) {
		return EINVAL;
	}
	if (!S_ISDIR(node->stat.st_mode)) {
		return ENOTDIR;
	}
	LockScheduler();
	struct vnode* deref = prcss->cwd;
	prcss->cwd = node;
	ReferenceVnode(node);
	UnlockScheduler();
	if (deref != NULL) {
		DereferenceVnode(deref);
	}
	return 0;
}
