
/*
 * adt/tree.c - Self Balancing Binary Search Tree
 *
 * Implements a generic AVL tree data structure (this is an implementation 
 * detail - any binary tree could work).
 */

#include <common.h>
#include <assert.h>
#include <heap.h>
#include <tree.h>
#include <log.h>

static struct tree_node* AvlCreateNode(void* data, struct tree_node* left, struct tree_node* right) {
    struct tree_node* tree = AllocHeap(sizeof(struct tree_node));
    tree->left = left;
    tree->right = right;
    tree->data = data;
    return tree;
}

static int AvlGetHeight(struct tree_node* tree) {
    if (tree != NULL && ((size_t) tree) < 0xF000000) {
        LogWriteSerial("AvlGetHeight: 0x%X\n", tree);
    }
    if (tree == NULL) {
        return 0;
    }

    bool bad_left = tree->left != NULL && (((size_t) (tree->left)) < 0xF000000);
    bool bad_right = tree->right != NULL && (((size_t) (tree->right)) < 0xF000000);
    if (bad_left || bad_right) {
        LogWriteSerial("UH OH! AvlGetHeight on tree 0x%X -> l = 0x%X, r = 0x%X\n", tree, tree->left, tree->right);
    }

    return 1 + MAX(AvlGetHeight(tree->left), AvlGetHeight(tree->right));
}

static int AvlGetBalance(struct tree_node* tree) {
    if (tree == NULL) {
        return 0;
    }

    return AvlGetHeight(tree->left) - AvlGetHeight(tree->right);
}

static struct tree_node* AvlRotateLeft(struct tree_node* tree) {
    struct tree_node* new_root = tree->right;
    struct tree_node* new_right = new_root->left;
    new_root->left = tree;
    tree->right = new_right;
    return new_root;
}

static struct tree_node* AvlRotateRight(struct tree_node* tree) {
    struct tree_node* new_root = tree->left;
    struct tree_node* new_left = new_root->right;
    new_root->right = tree;
    tree->left = new_left;
    return new_root;
}

static struct tree_node* AvlBalance(struct tree_node* tree) {
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

static struct tree_node* AvlInsert(struct tree_node* tree, void* data, tree_comparator comparator) {
    struct tree_node* new_tree;
    
    // TODO: surely there's a better way that involves less node creation and 
    // deletion...
    
    assert(comparator != NULL);
    assert(tree != NULL);

    if (comparator(data, tree->data) < 0) {
        struct tree_node* left_tree;
        if (tree->left == NULL) {
            left_tree = AvlCreateNode(data, NULL, NULL);
        } else {
            left_tree = AvlInsert(tree->left, data, comparator);
        }

        new_tree = AvlCreateNode(tree->data, left_tree, tree->right);

    } else {
        struct tree_node* right_tree;
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

static struct tree_node* AvlDelete(struct tree_node* tree, void* data, tree_comparator comparator) {    
    if (tree == NULL) {
        return NULL;
    }

    struct tree_node* to_free = NULL;

    if (comparator(data, tree->data) < 0) {
        tree->left = AvlDelete(tree->left, data, comparator);

    } else if (comparator(data, tree->data) > 0) {
        tree->right = AvlDelete(tree->right, data, comparator);

    } else if (tree->left == NULL) {
        to_free = tree;
        tree = tree->right;

    } else if (tree->right == NULL) {
        to_free = tree;
        tree = tree->left;
        
    } else {
        struct tree_node* node = tree->right;
        while (node->left != NULL) {
            node = node->left;
        }

        tree->data = node->data;
        tree->right = AvlDelete(tree->right, node->data, comparator);
    }

    /* 
     * If NULL is passed in to FreeHeap, nothing happens (which is want we want).
     */
    FreeHeap(to_free);

    return AvlBalance(tree);
}

/**
 * Given an object, find it in the AVL tree and return it. This is useful if the
 * comparator only compares part of the object, and so the entire object can be
 * retrieved by searching for only part of it.
 */
static void* AvlGet(struct tree_node* tree, void* data, tree_comparator comparator) {
    if (tree == NULL) {
        return NULL;
    }

    if (comparator(tree->data, data) == 0) {
        /*
         * Must return `tree->data`, (and not `data`), as tree->data != data if
         * there is a custom comparator.
         */
        return tree->data;
    }

    void* left = AvlGet(tree->left, data, comparator);
    if (left != NULL) {
        return left;
    }
    return AvlGet(tree->right, data, comparator);
}

static void AvlPrint(struct tree_node* tree, void(*printer)(void*)) {
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

static bool AvlContains(struct tree_node* tree, void* data, tree_comparator comparator) {
    if (tree == NULL) {
        return false;
    }

    if (comparator(tree->data, data) == 0) {
        return true;
    }

    return AvlContains(tree->left, data, comparator) || AvlContains(tree->right, data, comparator);
}

static void AvlDestroy(struct tree_node* tree, tree_deletion_handler handler) {
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

struct tree* TreeCreate(void) {
    struct tree* tree = AllocHeap(sizeof(struct tree));
    tree->size = 0;
    tree->root = NULL;
    tree->deletion_handler = NULL;
    tree->equality_handler = AvlDefaultComparator;
    return tree;
}

tree_deletion_handler TreeSetDeletionHandler(
    struct tree* tree, tree_deletion_handler handler
) {
    tree_deletion_handler ret = tree->deletion_handler;
    tree->deletion_handler = handler;
    return ret;
}

tree_comparator TreeSetComparator(struct tree* tree, tree_comparator handler) {
    tree_comparator ret = tree->equality_handler;
    tree->equality_handler = handler;
    return ret;
}

void TreeInsert(struct tree* tree, void* data) {
    if (tree->root == NULL) {
        tree->root = AvlCreateNode(data, NULL, NULL);
    } else {
        tree->root = AvlInsert(tree->root, data, tree->equality_handler);
    }
    tree->size++;
}

void TreeDelete(struct tree* tree, void* data) {
    tree->root = AvlDelete(tree->root, data, tree->equality_handler);
    tree->size--;
}

bool TreeContains(struct tree* tree, void* data) {
    return AvlContains(tree->root, data, tree->equality_handler);
}

void* TreeGet(struct tree* tree, void* data) {
    return AvlGet(tree->root, data, tree->equality_handler);
}

int TreeSize(struct tree* tree) {
    return tree->size;
}

void TreeDestroy(struct tree* tree) {
    AvlDestroy(tree->root, tree->deletion_handler);
    FreeHeap(tree);
}

void TreePrint(struct tree* tree, void(*printer)(void*)) {
    AvlPrint(tree->root, printer);
}