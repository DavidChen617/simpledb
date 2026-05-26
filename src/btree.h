#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "table.h"

struct Cursor;

typedef enum {
    NODE_INTERNAL,
    NODE_LEAF
} NodeType;

// Common node header (6 bytes)
#define NODE_TYPE_SIZE 1
#define IS_ROOT_SIZE 1
#define PARENT_POINTER_SIZE 4
#define COMMON_NODE_HEADER_SIZE 6

// Leaf node header (14 bytes)
#define LEAF_NODE_NUM_CELLS_SIZE 4
#define LEAF_NODE_NEXT_LEAF_SIZE 4
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_OFFSET (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

// Leaf cell: [key(4 bytes)][row_data]
#define LEAF_NODE_KEY_SIZE 4
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)

// Internal node header (14 bytes)
#define INTERNAL_NODE_NUM_KEYS_SIZE 4
#define INTERNAL_NODE_RIGHT_CHILD_SIZE 4
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

// Internal node cell: [child_page(4)][separator_key(4)]
#define INTERNAL_NODE_KEY_SIZE 4
#define INTERNAL_NODE_CHILD_SIZE 4
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)

// 一個 internal node 的物理容量上限
#define INTERNAL_NODE_CAPACITY \
    ((PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) / INTERNAL_NODE_CELL_SIZE)

// 強制提早分裂，方便觀察 B-Tree 行為
// 改成 INTERNAL_NODE_CAPACITY 即可使用完整容量
#define INTERNAL_NODE_MAX_CELLS 3

#define INVALID_PAGE_NUM UINT32_MAX

// Leaf node accessors
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(const Table *table, void *node, uint32_t cell_num);
uint32_t *leaf_node_key(const Table *table, void *node, uint32_t cell_num);
void *leaf_node_value(const Table *table, void *node, uint32_t cell_num);
uint32_t *leaf_node_next_leaf(void *node);
void initialize_leaf_node(void *node);

// Internal node accessors
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
void initialize_internal_node(void *node);

// Node type / root
NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType node_type);
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);

// CRUD
void leaf_node_insert(const struct Cursor *cursor, uint32_t key, const void *row_data);
void leaf_node_delete(const struct Cursor *cursor);
void leaf_node_handle_underflow(const Table *table, uint32_t page_num);

// Search
struct Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);
struct Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
struct Cursor *table_find(Table *table, uint32_t key);

// Split
void leaf_node_split_and_insert(const Cursor *cursor, uint32_t key, const void *row_data);
void create_new_root(Table *table, uint32_t right_child_page_num);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);

// Debug
void print_tree(Table *table, uint32_t page_num, uint32_t indent_level);

#endif
