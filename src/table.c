#include <stdlib.h>
#include "table.h"
#include "btree.h"

static void compute_table_layout(Table *t) {
    t->cell_size = LEAF_NODE_KEY_SIZE + t->meta->row_size;
    const uint32_t space = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
    t->max_cells = space / t->cell_size;
    t->min_cells = t->max_cells / 2;
    t->right_split_count = (t->max_cells + 1) / 2;
    t->left_split_count = (t->max_cells + 1) - t->right_split_count;
}

Database *db_open(const char *filename) {
    Database *db = malloc(sizeof(Database));
    db->pager = pager_open(filename);

    if (db->pager->num_pages == 0) {
        db->catalog.num_tables = 0;
        catalog_flush(db->pager, &db->catalog);
    } else
        catalog_load(db->pager, &db->catalog);

    return db;
}

void db_close(Database *db) {
    catalog_flush(db->pager, &db->catalog);
    pager_close(db->pager);
    free(db);
}

Table *table_open(Database *db, const char *name) {
    const int idx = catalog_find(&db->catalog, name);
    if (idx < 0)
        return NULL;
    Table *t = malloc(sizeof(Table));
    t->pager = db->pager;
    t->meta = &db->catalog.tables[idx];
    t->root_page_num = t->meta->root_page_num;
    compute_table_layout(t);
    return t;
}

void table_close(Table *table) {
    free(table);
}
