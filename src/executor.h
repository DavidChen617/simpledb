#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "table.h"
#include "cursor.h"

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_KEY_NOT_FOUND,
    EXECUTE_TABLE_FULL,
} ExecuteResult;

ExecuteResult execute_insert(Table *table, uint32_t key, char **tokens, int num_tokens);

ExecuteResult execute_select(Table *table,
                             char **cols, int num_cols,
                             const char *where_col, const char *where_op,
                             const char *where_val);

ExecuteResult execute_update(Table *table, uint32_t key, char **tokens, int num_tokens);

ExecuteResult execute_delete(Table *table, uint32_t key);

#endif
