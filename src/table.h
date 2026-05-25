#ifndef TABLE_H
#define TABLE_H
#include "pager.h"
#include "schema.h"
#include "catalog.h"

typedef struct {
    Pager *pager;
    Catalog catalog;
} Database;

typedef struct {
    uint32_t root_page_num;
    Pager *pager;
    TableMeta *meta;
    // B Tree layout，由 meta->row_size 計算而來
    uint32_t cell_size;
    uint32_t max_cells;
    uint32_t min_cells;
    uint32_t left_split_count;
    uint32_t right_split_count;
} Table;

Database *db_open(const char *filename);

void db_close(Database *db);

Table *table_open(Database *db, const char *name);

void table_close(Table *table);

#endif
