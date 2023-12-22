
#include <assert.h>
#include <string.h>
#include <heap.h>
#include <radixtrie.h>
#include <log.h>

// A very cursed port from https://iq.opengenus.org/radix-tree

#define DATA_BYTES_IN_SHORT     15
#define MAX_BITS_PER_EDGE       (DATA_BYTES_IN_SHORT * 8)

struct short_bool_list {
    uint8_t data[DATA_BYTES_IN_SHORT];
    uint8_t length;
};

struct node;

struct edge {
    struct short_bool_list label;
    struct node* next;
};

struct node {
    struct edge* left;
    struct edge* right;
    void* data;
};

struct radix_trie {
    struct node* root;
};

static struct short_bool_list PAGEABLE_CODE_SECTION CreateShortListByTruncatingLong(const struct long_bool_list* list) {
    struct short_bool_list s;
    s.length = list->length;
    inline_memcpy((void*) s.data, (const void*) list->data, DATA_BYTES_IN_SHORT);
    return s;
}

static void PAGEABLE_CODE_SECTION SetBitOfLongList(struct long_bool_list* list, int index, bool b) {
    if (b) {
        list->data[index / 8] |= (1 << (index % 8));
    } else {
        list->data[index / 8] &= ~(1 << (index % 8));
    }
}

static void PAGEABLE_CODE_SECTION SetBitOfShortList(struct short_bool_list* list, int index, bool b) {
    if (b) {
        list->data[index / 8] |= (1 << (index % 8));
    } else {
        list->data[index / 8] &= ~(1 << (index % 8));
    }
}

static bool PAGEABLE_CODE_SECTION GetBitOfShortList(const struct short_bool_list* list, int index) {
    return (list->data[index / 8] >> (index % 8)) & 1;
}

static bool PAGEABLE_CODE_SECTION GetBitOfLongList(const struct long_bool_list* list, int index) {
    return (list->data[index / 8] >> (index % 8)) & 1;
}

static struct short_bool_list PAGEABLE_CODE_SECTION RemoveStartOfShortList(const struct short_bool_list* list, int num_to_remove) {
    struct short_bool_list out;

    int out_bits = list->length - num_to_remove;
    if (out_bits < 0) {
        out_bits = 0;
    }
    out.length = out_bits;

    for (int i = 0; i < out_bits; ++i) {
        SetBitOfShortList(&out, i, GetBitOfShortList(list, i + num_to_remove));
    }

    return out;
} 

static struct long_bool_list PAGEABLE_CODE_SECTION RemoveStartOfLongList(const struct long_bool_list* list, int num_to_remove) {
    struct long_bool_list out;

    int out_bits = list->length - num_to_remove;
    if (out_bits < 0) {
        out_bits = 0;
    }
    out.length = out_bits;

    for (int i = 0; i < out_bits; ++i) {
        SetBitOfLongList(&out, i, GetBitOfLongList(list, i + num_to_remove));
    }

    return out;
} 

static struct edge* PAGEABLE_CODE_SECTION GetEdgeFromNode(struct node* node, bool right) {
    return right ? node->right : node->left;
}

static void PAGEABLE_CODE_SECTION AddEdgeToNode(struct node* node, struct edge* edge, bool right) {
    if (right) {
        node->right = edge;
    } else {
        node->left = edge;
    }
}


static struct node* PAGEABLE_CODE_SECTION CreateNode(void) {
    struct node* node = AllocHeapEx(sizeof(struct node), HEAP_UNFREEABLE | HEAP_ALLOW_PAGING);
    node->left = NULL;
    node->right = NULL;
    node->data = NULL;
    return node;
}

static struct edge* PAGEABLE_CODE_SECTION CreateEdgeInternal(struct long_bool_list label) {
    struct edge* edge = AllocHeapEx(sizeof(struct edge), HEAP_UNFREEABLE | HEAP_ALLOW_PAGING);
    struct edge* first_edge = edge;
    while (label.length >= MAX_BITS_PER_EDGE) {
        struct node* new_node = CreateNode();
        edge->label = CreateShortListByTruncatingLong(&label);
        edge->next = new_node;

        bool right = GetBitOfLongList(&label, MAX_BITS_PER_EDGE - 1);
        label = RemoveStartOfLongList(&label, MAX_BITS_PER_EDGE - 1);
        edge = AllocHeapEx(sizeof(struct edge), HEAP_UNFREEABLE | HEAP_ALLOW_PAGING);
        AddEdgeToNode(new_node, edge, right);
    }

    edge->label = CreateShortListByTruncatingLong(&label);
    return first_edge;
}

static struct edge* PAGEABLE_CODE_SECTION CreateEdgeFromNodeShort(const struct short_bool_list* label, struct node* node) {
    struct edge* edge = AllocHeapEx(sizeof(struct edge), HEAP_UNFREEABLE | HEAP_ALLOW_PAGING);
    edge->label = *label;
    edge->next = node;
    return edge;
}

static struct edge* PAGEABLE_CODE_SECTION CreateEdgeFromData(const struct long_bool_list* label, void* data) {
    struct edge* edge = CreateEdgeInternal(*label);
    edge->next = CreateNode();
    edge->next->data = data;
    return edge;
}

static void PAGEABLE_CODE_SECTION AddEdgeToNodeFromLabel(struct node* restrict node, struct node* restrict next, const struct short_bool_list* label) {
    AddEdgeToNode(node, CreateEdgeFromNodeShort(label, next), GetBitOfShortList(label, 0));
}

struct radix_trie* PAGEABLE_CODE_SECTION RadixTrieCreate(void) {
    struct radix_trie* trie = AllocHeapEx(sizeof(struct radix_trie), HEAP_UNFREEABLE | HEAP_ALLOW_PAGING);
    trie->root = CreateNode();
    return trie;
}

static bool PAGEABLE_CODE_SECTION StartsWith(const struct long_bool_list* l, const struct short_bool_list* s) {
    if (l->length < s->length) return false;

    for (int i = 0; i < s->length; ++i) {
        if (i >= MAX_BITS_PER_EDGE) {
            return true;
        }
        if (GetBitOfLongList(l, i) != GetBitOfShortList(s, i)) {
            return false;
        }
    }

    return true;
}

static int PAGEABLE_CODE_SECTION GetFirstMismatchedBit(const struct long_bool_list* word, const struct short_bool_list* edge_word) {
    int len = word->length < edge_word->length ? word->length : edge_word->length;
    for (int i = 1; i < len; ++i) {
        if (GetBitOfLongList(word, i) != GetBitOfShortList(edge_word, i)) {
            return i;
        }
    }

    return -1;
}

void PAGEABLE_CODE_SECTION RadixTrieInsert(struct radix_trie* trie, const struct long_bool_list* key, void* value) {
    struct node* current = trie->root;
    int current_index = 0;

    while (current_index < key->length) {
        bool transition = GetBitOfLongList(key, current_index);
        struct edge* current_edge = GetEdgeFromNode(current, transition);
        struct long_bool_list current_str = RemoveStartOfLongList(key, current_index);

        if (current_edge == NULL) {
            AddEdgeToNode(current, CreateEdgeFromData(&current_str, value), transition);
            break;
        }
        
        int split_index = GetFirstMismatchedBit(&current_str, &current_edge->label);

        if (split_index == -1) {
            if (current_str.length == current_edge->label.length) {
                current_edge->next->data = value;
                break;
            
            } else if (current_str.length < current_edge->label.length) {
                struct short_bool_list suffix = RemoveStartOfShortList(&current_edge->label, current_str.length);
                // TODO: may be overflow here if very long shared chunk...

                if (current_str.length >= MAX_BITS_PER_EDGE) {
                    LogDeveloperWarning("dicey radix tree truncation from %d\n", current_str.length);
                }

                current_edge->label = CreateShortListByTruncatingLong(&current_str);
                struct node* new_next = CreateNode();
                struct node* after_next = current_edge->next;
                new_next->data = value;
                current_edge->next = new_next;
                AddEdgeToNodeFromLabel(new_next, after_next, &suffix);
                break;

            } else {
                split_index = current_edge->label.length;
            }

        } else {
            struct short_bool_list suffix = RemoveStartOfShortList(&current_edge->label, split_index);
            current_edge->label.length = split_index;       // truncate existing

            struct node* prev_next = current_edge->next;
            current_edge->next = CreateNode();
            AddEdgeToNodeFromLabel(current_edge->next, prev_next, &suffix);
        }

        current = current_edge->next;
        current_index += split_index;
    }
}

void* PAGEABLE_CODE_SECTION RadixTrieGet(struct radix_trie* trie, const struct long_bool_list* key) {
    struct node* current = trie->root;
    int current_index = 0;

    while (current_index < key->length) {
        bool transition = GetBitOfLongList(key, current_index);
        struct edge* current_edge = GetEdgeFromNode(current, transition);
        if (current_edge == NULL) {
            return NULL;
        }
        struct long_bool_list current_str = RemoveStartOfLongList(key, current_index);
        if (!StartsWith(&current_str, &current_edge->label)) {
            return NULL;
        }

        current_index += current_edge->label.length;
        current = current_edge->next;
    }
    
    return current->data;
}

struct long_bool_list PAGEABLE_CODE_SECTION RadixTrieCreateBoolListFromData(uint8_t* data, int num_bytes) {
    struct long_bool_list l;
    l.length = num_bytes * 8;

    for (int i = 0; i < num_bytes; ++i) {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; ++j) {
            SetBitOfLongList(&l, i * 8 + j, byte & 1);
            byte >>= 1;
        }
    }

    return l;
}

static int PAGEABLE_CODE_SECTION MapCharacter(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return 62;
}

struct long_bool_list PAGEABLE_CODE_SECTION RadixTrieCreateBoolListFromData64(char* data) {
    struct long_bool_list l;
    l.length = 0;

    for (int i = 0; i < data[i]; ++i) {
        int c = MapCharacter(data[i]);
        
        for (int j = 0; j < 6; ++j) {
            SetBitOfLongList(&l, i * 6 + j, c & 64);
            c <<= 1;
        }

        l.length += 6;
    }

    return l;
}