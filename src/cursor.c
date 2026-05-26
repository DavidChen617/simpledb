#include "cursor.h"
#include "btree.h"

Cursor *table_start(Table *table) {
    Cursor *cursor = table_find(table, 0);
    void *node = pager_get_page(table->pager, cursor->page_num);
    cursor->end_of_table = (*leaf_node_num_cells(node) == 0);
    return cursor;
}

void *cursor_value(const Cursor *cursor) {
    void *node = pager_get_page(cursor->table->pager, cursor->page_num);
    return leaf_node_value(cursor->table, node, cursor->cell_num);
}

void cursor_advance(Cursor *cursor) {
    void *node = pager_get_page(cursor->table->pager, cursor->page_num);
    ++cursor->cell_num;

    if (cursor->cell_num >= *leaf_node_num_cells(node)) {
        const uint32_t next = *leaf_node_next_leaf(node);
        if (next == INVALID_PAGE_NUM)
            cursor->end_of_table = true;
        else {
            cursor->page_num = next;
            cursor->cell_num = 0;
        }
    }
}
