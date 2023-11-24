#include <stddef.h>
#include <assert.h>
#include <heap.h>
#include <avl.h>
#include <log.h>

struct avl_node {
    struct avl_node* left;
    struct avl_node* right;
    void* data;
};

struct avl_tree {
	int size;
	struct avl_node* root;
    avl_deletion_handler deletion_handler;
    avl_comparator equality_handler;
};

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct avl_node* AvlCreateNode(void* data, struct avl_node* left, struct avl_node* right) {
    struct avl_node* tree = AllocHeap(sizeof(struct avl_node));
    tree->left = left;
    tree->right = right;
    tree->data = data;
    return tree;
}

static int AvlGetHeight(struct avl_node* tree) {
    if (tree == NULL) {
        return 0;
    }

    return 1 + MAX(AvlGetHeight(tree->left), AvlGetHeight(tree->right));
}

static int AvlGetBalance(struct avl_node* tree) {
    if (tree == NULL) {
        return 0;
    }

    return AvlGetHeight(tree->left) - AvlGetHeight(tree->right);
}

static struct avl_node* AvlRotateLeft(struct avl_node* tree) {
    struct avl_node* new_root = tree->right;
    struct avl_node* new_right = new_root->left;
    new_root->left = tree;
    tree->right = new_right;
    return new_root;
}

static struct avl_node* AvlRotateRight(struct avl_node* tree) {
    struct avl_node* new_root = tree->left;
    struct avl_node* new_left = new_root->right;
    new_root->right = tree;
    tree->left = new_left;
    return new_root;
}

static struct avl_node* AvlBalance(struct avl_node* tree) {
    if (tree == NULL) {
        return NULL;
    }

    int bf = AvlGetBalance(tree);
    assert(bf >= -2 && bf <= 2);

    if (bf == -2) {
        if (AvlGetBalance(tree->right) == 1) {
            tree->right = AvlRotateRight(tree->right);
        }
        return AvlRotateLeft(tree);

    } else if (bf == 2) {
        if (AvlGetBalance(tree->left) == -1) {
            tree->left = AvlRotateLeft(tree->left);
        }
        return AvlRotateRight(tree);
    
    } else {
        return tree;
    }
}


static struct avl_node* AvlInsert(struct avl_node* tree, void* data, avl_comparator comparator) {
    struct avl_node* new_tree;
    
    assert(comparator != NULL);
    assert(tree != NULL);

    if (comparator(data, tree->data) < 0) {
        struct avl_node* left_tree;
        if (tree->left == NULL) {
            left_tree = AvlCreateNode(data, NULL, NULL);
        } else {
            left_tree = AvlInsert(tree->left, data, comparator);
        }
        new_tree = AvlCreateNode(tree->data, left_tree, tree->right);

    } else {
        struct avl_node* right_tree;
        if (tree->right == NULL) {
            right_tree = AvlCreateNode(data, NULL, NULL);
        } else {
            right_tree = AvlInsert(tree->right, data, comparator);
        }
        new_tree = AvlCreateNode(tree->data, tree->left, right_tree);
    }

    FreeHeap(tree);

    return AvlBalance(new_tree);
}

static struct avl_node* AvlDelete(struct avl_node* tree, void* data, avl_comparator comparator) {    
    if (tree == NULL) {
        return NULL;
    }

    struct avl_node* to_free = NULL;

    if (comparator(data, tree->data) < 0) {
        /*
         * Recurse down the left side until we find the right element.
         */
        tree->left = AvlDelete(tree->left, data, comparator);

    } else if (comparator(data, tree->data) > 0) {
        /*
         * Recurse down the right side until we find the right element.
         */
        tree->right = AvlDelete(tree->right, data, comparator);

    } else if (tree->left == NULL) {
        /*
         * Right child only, so bring that child up.
         */
        to_free = tree;
        tree = tree->right;

    } else if (tree->right == NULL) {
        /*
         * Left child only, so bring that child up.
         */
        to_free = tree;
        tree = tree->left;
        
    } else {
        /*
         * Two children, so swap value with our successor, and then that value
         * again now that we've moved it to be deeper down into the tree). 
         * This will continue until we reach a different case.
         */
        
        struct avl_node* node = tree->right;
        while (node->left != NULL) {
            node = node->left;
        }

        tree->data = node->data;
        tree->right = AvlDelete(tree->right, node->data, comparator);
    }

    /* 
     * If NULL is passed in, nothing happens (which is want we want).
     */

    FreeHeap(to_free);      // TODO: this appears to be buggy..?

    return AvlBalance(tree);
}

static void* AvlGet(struct avl_node* tree, void* data, avl_comparator comparator) {
    if (tree == NULL) {
        return NULL;
    }

    if (comparator(tree->data, data) == 0) {
        /*
         * Must return tree->data, as tree->data != data if there is a custom comparator.
         */
        return tree->data;
    }

    void* left = AvlGet(tree->left, data, comparator);
    if (left != NULL) {
        return left;
    }
    return AvlGet(tree->right, data, comparator);
}

static void AvlPrint(struct avl_node* tree, void(*printer)(void*)) {
    if (tree == NULL) { 
        return;
    }
    AvlPrint(tree->left, printer);
    if (printer == NULL) {
        LogWriteSerial("[[0x%X]], \n", tree->data);
    } else {
        printer(tree->data);
    }
    AvlPrint(tree->right, printer);
}

static bool AvlContains(struct avl_node* tree, void* data, avl_comparator comparator) {
    if (tree == NULL) {
        return false;
    }

    if (comparator(tree->data, data) == 0) {
        return true;
    }

    return AvlContains(tree->left, data, comparator) || AvlContains(tree->right, data, comparator);
}

static void AvlDestroy(struct avl_node* tree, avl_deletion_handler handler) {
    if (tree == NULL) {
        return;
    }

    AvlDestroy(tree->left, handler);
    AvlDestroy(tree->right, handler);
    if (handler != NULL) {
        handler(tree->data);
    }
    FreeHeap(tree);
}

static int AvlDefaultComparator(void* a, void* b) {
    if (a == b) return 0;
    return (a < b) ? -1 : 1;
}

struct avl_tree* AvlTreeCreate(void) {
    struct avl_tree* tree = AllocHeap(sizeof(struct avl_tree));
    tree->size = 0;
    tree->root = NULL;
    tree->deletion_handler = NULL;
    tree->equality_handler = AvlDefaultComparator;
    return tree;
}

avl_deletion_handler AvlTreeSetDeletionHandler(struct avl_tree* tree, avl_deletion_handler handler) {
    avl_deletion_handler ret = tree->deletion_handler;
    tree->deletion_handler = handler;
    return ret;
}

avl_comparator AvlTreeSetComparator(struct avl_tree* tree, avl_comparator handler) {
    avl_comparator ret = tree->equality_handler;
    tree->equality_handler = handler;
    return ret;
}

void AvlTreeInsert(struct avl_tree* tree, void* data) {
    if (tree->root == NULL) {
        tree->root = AvlCreateNode(data, NULL, NULL);
    } else {
        tree->root = AvlInsert(tree->root, data, tree->equality_handler);
    }
    tree->size++;
}

void AvlTreeDelete(struct avl_tree* tree, void* data) {
    tree->root = AvlDelete(tree->root, data, tree->equality_handler);
    tree->size--;
}

bool AvlTreeContains(struct avl_tree* tree, void* data) {
    return AvlContains(tree->root, data, tree->equality_handler);
}

void* AvlTreeGet(struct avl_tree* tree, void* data) {
    return AvlGet(tree->root, data, tree->equality_handler);
}

int AvlTreeSize(struct avl_tree* tree) {
    return tree->size;
}

void AvlTreeDestroy(struct avl_tree* tree) {
    AvlDestroy(tree->root, tree->deletion_handler);
    FreeHeap(tree);
}

struct avl_node* AvlTreeGetRootNode(struct avl_tree* tree) {
    return tree->root;
}

struct avl_node* AvlTreeGetLeft(struct avl_node* node) {
    if (node == NULL) {
        return NULL;
    }
    
    return node->left;
}

struct avl_node* AvlTreeGetRight(struct avl_node* node) {
    if (node == NULL) {
        return NULL;
    }
    
    return node->right;
}

void* AvlTreeGetData(struct avl_node* node) {
    if (node == NULL) {
        return NULL;
    }

    return node->data;
}

void AvlTreePrint(struct avl_tree* tree, void(*printer)(void*)) {
    AvlPrint(tree->root, printer);
}