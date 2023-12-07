
#define DATA_BYTES_IN_LONG      31
#define DATA_BYTES_IN_SHORT     11
#define MAX_BITS_PER_EDGE       (DATA_BYTES_IN_SHORT * 8)

struct long_bool_list {
    uint8_t data[DATA_BYTES_IN_LONG];
    uint8_t length;
};

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

struct radix_tree {
    struct node* root;
};

static struct short_bool_list CreateShortListByTruncatingLong(const struct long_bool_list* list) {
    assert(list->length <= DATA_BYTES_IN_SHORT);
    struct short_bool_list s;
    s.length = list->length;
    memcpy((void*) s.data, (const void*) list->data, DATA_BYTES_IN_SHORT);
    return s;
}

static void SetBitOfLongList(struct long_bool_list* list, int index, bool b) {
    if (b) {
        list->data[index / 8] |= (1 << (index % 8));
    } else {
        list->data[index / 8] &= ~(1 << (index % 8));
    }
}

static bool GetBitOfLongList(const struct long_bool_list* list, int index) {
    return (list->data[index / 8] >> (index % 8)) & 1;
}

static struct long_bool_list RemoveStartOfLongList(const struct long_bool_list* list, int num_to_remove) {
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

static struct edge* GetEdgeFromNode(struct node* node, bool right) {
    return right ? node->right : node->left;
}

static void AddEdgeToNode(struct node* node, struct edge* edge, bool right) {
    if (right) {
        node->right = edge;
    } else {
        node->left = edge;
    }
}

static void AddEdgeToNodeFromLabel(struct node* node, const struct long_bool_list* label) {
    struct edge* edge = AllocHeap(sizeof(struct edge));
    edge->label = label;
    AddEdgeToNode(GetBitOfLongList(list, 0), CreateEdgeFromNode(list, node));
}

static int GetNumEdgesInNode(struct node* node) {
    return (node->left == NULL ? 0 : 1) + (node->right == NULL ? 0 : 1)
}

static struct edge* CreateEdgeInternal(const struct long_bool_list* label) {
    struct edge* edge = AllocHeap(sizeof(struct edge));
    
    while (label->length >= MAX_BITS_PER_EDGE) {
        struct node* new_node = (struct node*) AllocHeap(sizeof(struct node));
        edge->label = CreateShortListByTruncatingLong(label);
        edge->next = new_node;

        bool right = GetBitOfLongList(label, MAX_BITS_PER_EDGE - 1);
        label = RemoveStartOfLongList(label, MAX_BITS_PER_EDGE - 1);
        edge = AllocHeap(sizeof(struct edge));
        AddEdgeToNode(new_node, edge, right);
    }

    edge->label = *label;
    return edge;
}

static struct edge* CreateEdgeFromNode(const struct long_bool_list* label, struct node* node) {
    struct edge* edge = CreateEdgeInternal(label);
    edge->next = node;
    return edge;
}

static struct edge* CreateEdgeFromData(const struct long_bool_list* label, void* data) {
    struct edge* edge = CreateEdgeInternal(label);
    edge->next = (struct node*) AllocHeap(sizeof(struct node));
    edge->next->data = data;
    return edge;
}
