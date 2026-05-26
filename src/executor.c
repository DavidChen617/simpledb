#include "table.h"
#include "executor.h"
#include <stdlib.h>
#include "btree.h"
#include "row.h"

ExecuteResult execute_insert(Table *table, const uint32_t key, char **tokens, int num_tokens) {
    Cursor *cursor = table_find(table, key);
    void *node = pager_get_page(table->pager, cursor->page_num);
    if (!cursor->end_of_table && *leaf_node_key(table, node, cursor->cell_num) == key) {
        free(cursor);
        return EXECUTE_DUPLICATE_KEY;
    }

    void *row_buf = malloc(table->meta->row_size);
    if (row_serialize(table->meta, tokens, num_tokens, row_buf) < 0) {
        free(row_buf);
        free(cursor);
        return EXECUTE_TABLE_FULL;
    }

    leaf_node_insert(cursor, key, row_buf);
    free(row_buf);
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Table *table, const char *where_col, const char *where_op,
                             const char *where_val) {
    Cursor *cursor = table_start(table);
    while (!cursor->end_of_table) {
        void *row = cursor_value(cursor);
        if (row_match(table->meta, row, where_col, where_op, where_val))
            row_print(table->meta, row);
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Table *table, const uint32_t key, char **tokens, const int num_tokens) {
    Cursor *cursor = table_find(table, key);
    void *node = pager_get_page(table->pager, cursor->page_num);

    if (cursor->end_of_table || *leaf_node_key(table, node, cursor->cell_num) != key) {
        free(cursor);
        return EXECUTE_KEY_NOT_FOUND;
    }

    void *row = cursor_value(cursor);
    if (row_serialize(table->meta, tokens, num_tokens, row) < 0) {
        free(cursor);
        return EXECUTE_TABLE_FULL;
    }

    pager_mark_dirty(table->pager, cursor->page_num);
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Table *table, const uint32_t key) {
    Cursor *cursor = table_find(table, key);
    void *node = pager_get_page(table->pager, cursor->page_num);

    if (cursor->end_of_table || *leaf_node_key(table, node, cursor->cell_num) != key) {
        free(cursor);
        return EXECUTE_KEY_NOT_FOUND;
    }

    leaf_node_delete(cursor);
    free(cursor);
    return EXECUTE_SUCCESS;
}
