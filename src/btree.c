#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cursor.h"
#include "btree.h"

// byte offset 常數(從 node 起點算起)
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_OFFSET NODE_TYPE_SIZE
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define LEAF_NODE_CELLS_OFFSET LEAF_NODE_HEADER_SIZE
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_CELLS_OFFSET INTERNAL_NODE_HEADER_SIZE

static uint32_t *node_parent(void *node) {
    return (uint32_t *) ((char *) node + PARENT_POINTER_OFFSET);
}

// Leaf node accessors
uint32_t *leaf_node_num_cells(void *node) {
    return (uint32_t *) ((char *) node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void *leaf_node_cell(const Table *table, void *node, const uint32_t cell_num) {
    return (char *) node + LEAF_NODE_CELLS_OFFSET + cell_num * table->cell_size;
}

uint32_t *leaf_node_key(const Table *table, void *node, const uint32_t cell_num) {
    return (uint32_t *) leaf_node_cell(table, node, cell_num);
}

void *leaf_node_value(const Table *table, void *node, const uint32_t cell_num) {
    return (char *) leaf_node_cell(table, node, cell_num) + LEAF_NODE_KEY_SIZE;
}

uint32_t *leaf_node_next_leaf(void *node) {
    return (uint32_t *) ((char *) node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

void initialize_leaf_node(void *node) {
    set_node_type(node, NODE_LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node) = 0;
    *leaf_node_next_leaf(node) = INVALID_PAGE_NUM;
}

// Internal node accessor
uint32_t *internal_node_num_keys(void *node) {
    return (uint32_t *) ((char *) node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint32_t *internal_node_right_child(void *node) {
    return (uint32_t *) ((char *) node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

uint32_t *internal_node_child(void *node, const uint32_t child_num) {
    const uint32_t num_keys = *internal_node_num_keys(node);
    if (child_num > num_keys) {
        printf("Error: child num %d > num_key %d", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    if (child_num == num_keys)
        return internal_node_right_child(node);

    return internal_node_cell(node, child_num);
}

uint32_t *internal_node_key(void *node, const uint32_t key_num) {
    return (uint32_t *) ((char *) internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE);
}

uint32_t *internal_node_cell(void *node, const uint32_t cell_num) {
    return (uint32_t *) ((char *) node + INTERNAL_NODE_CELLS_OFFSET + cell_num * INTERNAL_NODE_CELL_SIZE);
}

void initialize_internal_node(void *node) {
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

// Node type / root
NodeType get_node_type(void *node) {
    return (NodeType) *((uint8_t *) ((char *) node + NODE_TYPE_OFFSET));
}

void set_node_type(void *node, const NodeType node_type) {
    *((uint8_t *) ((char *) node + NODE_TYPE_OFFSET)) = (uint8_t) node_type;
}

bool is_node_root(void *node) {
    return (bool) *((uint8_t *) ((char *) node + IS_ROOT_OFFSET));
}

void set_node_root(void *node, const bool is_root) {
    *((uint8_t *) ((char *) node + IS_ROOT_OFFSET)) = (uint8_t) is_root;
}

// Search helpers
static uint32_t get_node_max_key(Table *table, void *node) {
    if (get_node_type(node) == NODE_LEAF)
        return *leaf_node_key(table, node, *leaf_node_num_cells(node) - 1);

    void *rc = pager_get_page(table->pager, *internal_node_right_child(node));
    return get_node_max_key(table, rc);
}

static uint32_t internal_node_find_child(void *node, const uint32_t key) {
    const uint32_t num_keys = *internal_node_num_keys(node);
    uint32_t lo = 0, hi = num_keys;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;

        if (*internal_node_key(node, mid) >= key)
            hi = mid;
        else
            lo = mid + 1;
    }

    return lo;
}

static void update_internal_node_key(void *parent, const uint32_t old_key, const uint32_t new_key) {
    const uint32_t idx = internal_node_find_child(parent, old_key);
    *internal_node_key(parent, idx) = new_key;
}

// Leaf find
Cursor *leaf_node_find(Table *table, const uint32_t page_num, const uint32_t key) {
    void *node = pager_get_page(table->pager, page_num);
    const uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;
    uint32_t lo = 0, hi = num_cells;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        const uint32_t k = *leaf_node_key(table, node, mid);
        if (k == key) {
            cursor->cell_num = mid;
            cursor->end_of_table = false;
            return cursor;
        }
        if (key < k)
            hi = mid;
        else
            lo = mid + 1;
    }

    cursor->cell_num = lo;
    cursor->end_of_table = (lo == num_cells);
    return cursor;
};

Cursor *internal_node_find(Table *table, const uint32_t page_num, const uint32_t key) {
    void *node = pager_get_page(table->pager, page_num);
    const uint32_t child_page = *internal_node_child(node, internal_node_find_child(node, key));
    void *child = pager_get_page(table->pager, child_page);

    if (get_node_type(child) == NODE_LEAF)
        return leaf_node_find(table, child_page, key);

    return internal_node_find(table, child_page, key);
}

Cursor *table_find(Table *table, const uint32_t key) {
    void *root = pager_get_page(table->pager, table->root_page_num);

    if (get_node_type(root) == NODE_LEAF)
        return leaf_node_find(table, table->root_page_num, key);

    return internal_node_find(table, table->root_page_num, key);
}

// Leaf insert
void leaf_node_insert(const Cursor *cursor, const uint32_t key, const void *row_data) {
    void *node = pager_get_page(cursor->table->pager, cursor->page_num);
    const uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= cursor->table->max_cells) {
        leaf_node_split_and_insert(cursor, key, row_data);
        return;
    }

    for (uint32_t i = num_cells; i > cursor->cell_num; --i) {
        memcpy(
            leaf_node_cell(cursor->table, node, i),
            leaf_node_cell(cursor->table, node, i - 1),
            cursor->table->cell_size);
    }

    *leaf_node_num_cells(node) += 1;
    *leaf_node_key(cursor->table, node, cursor->cell_num) = key;
    memcpy(
        leaf_node_value(cursor->table, node, cursor->cell_num),
        row_data,
        cursor->table->meta->row_size);
    pager_mark_dirty(cursor->table->pager, cursor->page_num);
}

// Leaf delete
void leaf_node_delete(const Cursor *cursor) {
    void *node = pager_get_page(cursor->table->pager, cursor->page_num);
    const uint32_t num_cells = *leaf_node_num_cells(node);
    for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++) {
        memcpy(
            leaf_node_cell(cursor->table, node, i),
            leaf_node_cell(cursor->table, node, i + 1),
            cursor->table->cell_size);
    }

    (*leaf_node_num_cells(node))--;

    pager_mark_dirty(cursor->table->pager, cursor->page_num);

    if (!is_node_root(node) && *leaf_node_num_cells(node) < cursor->table->min_cells)
        leaf_node_handle_underflow(cursor->table, cursor->page_num);
}

// leaf underflow
static uint32_t find_child_index_in_parent(void *parent, uint32_t child_page_num) {
    const uint32_t num_keys = *internal_node_num_keys(parent);
    for (uint32_t i = 0; i < num_keys; ++i)
        if (*internal_node_child(parent, i) == child_page_num)
            return i;

    return num_keys;
}

static void collapse_root(const Table *table) {
    void *root = pager_get_page(table->pager, table->root_page_num);
    const uint32_t child_page = *internal_node_right_child(root);
    const void *child = pager_get_page(table->pager, child_page);
    memcpy(root, child, PAGE_SIZE);
    set_node_root(root, true);
    pager_mark_dirty(table->pager, table->root_page_num);

    if (get_node_type(root) == NODE_INTERNAL) {
        for (uint32_t i = 0; i < *internal_node_num_keys(root); ++i) {
            void *c = pager_get_page(table->pager, *internal_node_child(root, i));
            *node_parent(c) = table->root_page_num;
        }
        void *c = pager_get_page(table->pager, *internal_node_right_child(root));
        *node_parent(c) = table->root_page_num;
    }
}

void leaf_node_handle_underflow(const Table *table, const uint32_t page_num) {
    void *node = pager_get_page(table->pager, page_num);
    const uint32_t num_cells = *leaf_node_num_cells(node);
    const uint32_t parent_page = *node_parent(node);
    void *parent = pager_get_page(table->pager, parent_page);
    const uint32_t num_keys = *internal_node_num_keys(parent);
    const uint32_t my_idx = find_child_index_in_parent(parent, page_num);

    // 向右兄弟借
    if (my_idx < num_keys) {
        const uint32_t right_page = *internal_node_child(parent, my_idx + 1);
        void *right = pager_get_page(table->pager, right_page);
        const uint32_t right_num = *leaf_node_num_cells(right);

        if (right_num > table->min_cells) {
            memcpy(leaf_node_cell(table, node, num_cells),
                   leaf_node_cell(table, right, 0),
                   table->cell_size);

            ++(*leaf_node_num_cells(node));
            for (uint32_t i = 0; i < right_num - 1; ++i)
                memcpy(leaf_node_cell(table, right, i),
                   leaf_node_cell(table, right, i + 1),
                   table->cell_size);

            --(*leaf_node_num_cells(right));
            *internal_node_key(parent, my_idx) = *leaf_node_key(table, node, num_cells);
            pager_mark_dirty(table->pager, page_num);
            pager_mark_dirty(table->pager, right_page);
            pager_mark_dirty(table->pager, parent_page);
            return;
        }
    }

    // 向左兄弟借
    if (my_idx > 0) {
        const uint32_t left_page = *internal_node_child(parent, my_idx - 1);
        void *left = pager_get_page(table->pager, left_page);
        const uint32_t left_num = *leaf_node_num_cells(left);
        if (left_num > table->min_cells) {
            for (uint32_t i = num_cells; i > 0; --i)
                memcpy(
                leaf_node_cell(table, node, i),
                leaf_node_cell(table, node, i - 1),
                table->cell_size);
            memcpy(
                leaf_node_cell(table, node, 0),
                leaf_node_cell(table, left, left_num - 1),
                table->cell_size);
            ++(*leaf_node_num_cells(node));
            --(*leaf_node_num_cells(left));
            *internal_node_key(parent, my_idx - 1) =
                    *leaf_node_key(table, left, left_num - 2);
            pager_mark_dirty(table->pager, page_num);
            pager_mark_dirty(table->pager, left_page);
            pager_mark_dirty(table->pager, parent_page);

            return;
        }
    }

    // 合併
    if (my_idx < num_keys) {
        const uint32_t right_page = *internal_node_child(parent, my_idx + 1);
        void *right = pager_get_page(table->pager, right_page);
        const uint32_t right_num = *leaf_node_num_cells(right);
        for (uint32_t i = 0; i < right_num; ++i)
            memcpy(leaf_node_cell(table, node, num_cells + i),
               leaf_node_cell(table, right, i),
               table->cell_size);
        *leaf_node_num_cells(node) += right_num;
        *leaf_node_next_leaf(node) = *leaf_node_next_leaf(right);
        if (my_idx + 1 == num_keys)
            *internal_node_right_child(parent) = page_num;
        else {
            *internal_node_key(parent, my_idx) = *internal_node_key(parent, my_idx + 1);
            for (uint32_t i = my_idx + 1; i < num_keys - 1; ++i) {
                memcpy(internal_node_cell(parent, i),
                       internal_node_cell(parent, i + 1),
                       INTERNAL_NODE_CELL_SIZE);
            }
        }
    } else {
        const uint32_t left_page = *internal_node_child(parent, my_idx - 1);
        void *left = pager_get_page(table->pager, left_page);
        const uint32_t left_num = *leaf_node_num_cells(left);
        for (uint32_t i = 0; i < num_cells; ++i)
            memcpy(
            leaf_node_cell(table, left, left_num + i),
            leaf_node_cell(table, node, i),
            table->cell_size);

        *leaf_node_num_cells(left) += num_cells;
        *leaf_node_next_leaf(left) = *leaf_node_next_leaf(node);
        *internal_node_right_child(parent) = left_page;
    }
    --(*internal_node_num_keys(parent));
    pager_mark_dirty(table->pager, page_num);
    pager_mark_dirty(table->pager, parent_page);

    if (is_node_root(parent) && *internal_node_num_keys(parent) == 0)
        collapse_root(table);
}

// Left spilt
void leaf_node_split_and_insert(const Cursor *cursor, const uint32_t key, const void *row_data) {
    void *old_node = pager_get_page(cursor->table->pager, cursor->page_num);
    const uint32_t old_max = get_node_max_key(cursor->table, old_node);
    const uint32_t new_page = cursor->table->pager->num_pages;
    void *new_node = pager_get_page(cursor->table->pager, new_page);
    initialize_leaf_node(new_node);
    const uint32_t max_cells = cursor->table->max_cells;
    const uint32_t left_split = cursor->table->left_split_count;

    for (int32_t i = (int32_t) max_cells; i >= 0; --i) {
        void *dest_node;
        uint32_t idx;
        if (i >= (int32_t) left_split) {
            dest_node = new_node;
            idx = (uint32_t) i - left_split;
        } else {
            dest_node = old_node;
            idx = (uint32_t) i;
        }

        if (i == (int32_t) cursor->cell_num) {
            *leaf_node_key(cursor->table, dest_node, idx) = key;
            memcpy(
                leaf_node_value(cursor->table, dest_node, idx),
                row_data,
                cursor->table->meta->row_size);
        } else if (i > (int32_t) cursor->cell_num) {
            memcpy(leaf_node_cell(cursor->table, dest_node, idx),
                   leaf_node_cell(cursor->table, old_node, (uint32_t)i - 1),
                   cursor->table->cell_size);
        } else
            memcpy(
            leaf_node_cell(cursor->table, dest_node, idx),
            leaf_node_cell(cursor->table, old_node, (uint32_t)i),
            cursor->table->cell_size);
    }

    *leaf_node_num_cells(old_node) = left_split;
    *leaf_node_num_cells(new_node) = cursor->table->right_split_count;
    const uint32_t old_next = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page;
    *leaf_node_next_leaf(new_node) = old_next;
    pager_mark_dirty(cursor->table->pager, cursor->page_num);

    if (is_node_root(old_node))
        create_new_root(cursor->table, new_page);
    else {
        const uint32_t parent_page = *node_parent(old_node);
        const uint32_t new_max = get_node_max_key(cursor->table, old_node);
        void *parent = pager_get_page(cursor->table->pager, parent_page);

        update_internal_node_key(parent, old_max, new_max);
        pager_mark_dirty(cursor->table->pager, parent_page);
        *node_parent(new_node) = parent_page;
        internal_node_insert(cursor->table, parent_page, new_page);
    }
}

// Create new root
void create_new_root(Table *table, const uint32_t right_child_page_num) {
    void *root = pager_get_page(table->pager, table->root_page_num);
    void *right_child = pager_get_page(table->pager, right_child_page_num);
    const uint32_t left_child_page_num = table->pager->num_pages;
    void *left_child = pager_get_page(table->pager, left_child_page_num);

    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);
    *node_parent(left_child) = table->root_page_num;

    if (get_node_type(left_child) == NODE_INTERNAL) {
        for (uint32_t i = 0; i < *internal_node_num_keys(left_child); ++i) {
            void *c = pager_get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(c) = left_child_page_num;
        }

        void *c = pager_get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(c) = left_child_page_num;
    }

    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;
    *internal_node_key(root, 0) = get_node_max_key(table, left_child);
    *internal_node_right_child(root) = right_child_page_num;
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
    pager_mark_dirty(table->pager, table->root_page_num);
}

void internal_node_insert(Table *table, const uint32_t parent_page_num, const uint32_t child_page_num) {
    void *parent = pager_get_page(table->pager, parent_page_num);
    void *child = pager_get_page(table->pager, child_page_num);
    const uint32_t child_max_key = get_node_max_key(table, child);
    const uint32_t num_keys = *internal_node_num_keys(parent);

    if (num_keys >= INTERNAL_NODE_MAX_CELLS) {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    const uint32_t right_child_page = *internal_node_right_child(parent);
    void *right_child = pager_get_page(table->pager, right_child_page);

    if (child_max_key > get_node_max_key(table, right_child)) {
        *internal_node_child(parent, num_keys) = right_child_page;
        *internal_node_key(parent, num_keys) = get_node_max_key(table, right_child);
        *internal_node_right_child(parent) = child_page_num;
    } else {
        const uint32_t idx = internal_node_find_child(parent, child_max_key);
        for (uint32_t i = num_keys; i > idx; --i)
            memcpy(internal_node_cell(parent, i),
               internal_node_cell(parent, i - 1),
               INTERNAL_NODE_CELL_SIZE);

        *internal_node_child(parent, idx) = child_page_num;
        *internal_node_key(parent, idx) = child_max_key;
    }

    *internal_node_num_keys(parent) = num_keys + 1;
    pager_mark_dirty(table->pager, parent_page_num);
}

void internal_node_split_and_insert(Table *table, const uint32_t parent_page_num, const uint32_t child_page_num) {
    void *old_node = pager_get_page(table->pager, parent_page_num);
    const uint32_t old_max = get_node_max_key(table, old_node);

    const uint32_t new_page_num = table->pager->num_pages;
    void *new_node = pager_get_page(table->pager, new_page_num);
    initialize_internal_node(new_node);
    *node_parent(new_node) = *node_parent(old_node);

    const uint32_t mid = INTERNAL_NODE_MAX_CELLS / 2;
    // 把 old_node 右半搬到 new_node
    for (uint32_t i = mid + 1; i < INTERNAL_NODE_MAX_CELLS; ++i) {
        const uint32_t dest = i - (mid + 1);
        memcpy(
            internal_node_cell(new_node, dest),
            internal_node_cell(old_node, i),
            INTERNAL_NODE_CELL_SIZE);
        void *move_child = pager_get_page(table->pager, *internal_node_child(old_node, i));
        *node_parent(move_child) = new_page_num;
    }
    *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
    void *rc = pager_get_page(table->pager, *internal_node_right_child(new_node));
    *node_parent(rc) = new_page_num;

    *internal_node_num_keys(new_node) = INTERNAL_NODE_MAX_CELLS - (mid + 1);
    *internal_node_right_child(old_node) = *internal_node_child(old_node, mid);
    *internal_node_num_keys(old_node) = mid;
    pager_mark_dirty(table->pager, parent_page_num);

    if (is_node_root(old_node)) {
        create_new_root(table, new_page_num);
    } else {
        const uint32_t grandparent_page = *node_parent(old_node);
        const uint32_t new_max = get_node_max_key(table, old_node);
        void *grandparent = pager_get_page(table->pager, grandparent_page);
        update_internal_node_key(grandparent, old_max, new_max);
        pager_mark_dirty(table->pager, grandparent_page);
        *node_parent(new_node) = grandparent_page;
        internal_node_insert(table, grandparent_page, new_page_num);
    }

    void *child_node = pager_get_page(table->pager, child_page_num);
    const uint32_t child_max_key = get_node_max_key(table, child_node);
    if (is_node_root(pager_get_page(table->pager, parent_page_num)))
        internal_node_insert(table, parent_page_num, child_page_num);
    else {
        void *old_after = pager_get_page(table->pager, parent_page_num);
        if (child_max_key <= get_node_max_key(table, old_after))
            internal_node_insert(table, parent_page_num, child_page_num);
        else
            internal_node_insert(table, new_page_num, child_page_num);
    }
}

void print_tree(Table *table, const uint32_t page_num, const uint32_t indent_level) {
    void *node = pager_get_page(table->pager, page_num);
    for (uint32_t i = 0; i < indent_level; ++i)
        printf("  ");

    if (get_node_type(node) == NODE_LEAF) {
        const uint32_t num_cells = *leaf_node_num_cells(node);
        printf("leaf (n=%u): ", num_cells);
        for (uint32_t i = 0; i < num_cells; ++i) {
            if (i > 0)
                printf(", ");
            printf("%u", *leaf_node_key(table, node, i));
        }
        printf("\n");
    } else {
        const uint32_t num_keys = *internal_node_num_keys(node);
        printf("internal (n=%u)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; ++i) {
            print_tree(table, *internal_node_child(node, i), indent_level + 1);
            for (uint32_t j = 0; j < indent_level + 1; ++j)
                printf("  ");
            printf("key %u\n", *internal_node_key(node, i));
        }
        print_tree(table, *internal_node_right_child(node), indent_level + 1);
    }
}
