#ifndef ROW_H
#define ROW_H

#include "schema.h"

// 從字串 token 陣列序列化成 row bytes
// tokens[i] 對應 meta->columns[i]
// 1 成功，0 失敗
int row_serialize(TableMeta *meta, char **tokens, int num_tokens, void *dest);

// 把 row bytes 印成 (val1, val2) 格式
void row_print(const TableMeta *meta, void *row_data);

#endif
