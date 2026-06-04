#include "unity.h"
#include "table.h"
#include "executor.h"
#include "btree.h"
#include "cursor.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char db_path[64];

static void setup_table(Database *d) {
    if (d->catalog.num_tables > 0)
        return;

    TableMeta meta = {0};
    strncpy(meta.name, "users", MAX_TABLE_NAME - 1);
    meta.num_columns = 2;

    strncpy(meta.columns[0].name, "id", MAX_COL_NAME - 1);
    meta.columns[0].type = COL_INT;
    meta.columns[0].size = sizeof(int32_t);
    meta.columns[0].offset = 0;

    strncpy(meta.columns[1].name, "name", MAX_COL_NAME - 1);
    meta.columns[1].type = COL_TEXT;
    meta.columns[1].size = 64;
    meta.columns[1].offset = sizeof(int32_t);

    meta.row_size = sizeof(int32_t) + 64;
    meta.root_page_num = d->pager->num_pages;

    catalog_add(&d->catalog, &meta);
    void *root = pager_get_page(d->pager, meta.root_page_num);
    initialize_leaf_node(root);
    set_node_root(root, true);
    catalog_flush(d->pager, &d->catalog);
}

static int count_rows(Table *t) {
    Cursor *c = table_start(t);
    int n = 0;
    while (!c->end_of_table) { n++; cursor_advance(c); }
    free(c);
    return n;
}

void setUp(void) {
    snprintf(db_path, sizeof(db_path), "/tmp/simpledb_test_persist_%d.db", getpid());
}

void tearDown(void) {
    unlink(db_path);
}

void test_data_persists_after_reopen(void) {
    // Session 1: insert rows and close
    Database *db1 = db_open(db_path);
    setup_table(db1);
    Table *t1 = table_open(db1, "users");
    char *r1[] = {"1", "Alice"};
    char *r2[] = {"2", "Bob"};
    char *r3[] = {"3", "Carol"};
    execute_insert(t1, 1, r1, 2);
    execute_insert(t1, 2, r2, 2);
    execute_insert(t1, 3, r3, 2);
    table_close(t1);
    db_close(db1);

    // Session 2: verify rows still exist
    Database *db2 = db_open(db_path);
    Table *t2 = table_open(db2, "users");
    TEST_ASSERT_EQUAL(3, count_rows(t2));
    table_close(t2);
    db_close(db2);
}

void test_catalog_persists_after_reopen(void) {
    Database *db1 = db_open(db_path);
    setup_table(db1);
    TEST_ASSERT_EQUAL_UINT32(1, db1->catalog.num_tables);
    db_close(db1);

    Database *db2 = db_open(db_path);
    TEST_ASSERT_EQUAL_UINT32(1, db2->catalog.num_tables);
    TEST_ASSERT_EQUAL(0, catalog_find(&db2->catalog, "users"));
    db_close(db2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_data_persists_after_reopen);
    RUN_TEST(test_catalog_persists_after_reopen);
    return UNITY_END();
}
