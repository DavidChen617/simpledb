#include "unity.h"
#include "table.h"
#include "executor.h"
#include "btree.h"
#include "cursor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char db_path[64];
static Database *db;

static void create_users_table(Database *d) {
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
    snprintf(db_path, sizeof(db_path), "/tmp/simpledb_test_crud_%d.db", getpid());
    db = db_open(db_path);
    create_users_table(db);
}

void tearDown(void) {
    db_close(db);
    unlink(db_path);
}

void test_insert_and_count(void) {
    Table *t = table_open(db, "users");
    char *r1[] = {"1", "Alice"};
    char *r2[] = {"2", "Bob"};
    char *r3[] = {"3", "Carol"};
    execute_insert(t, 1, r1, 2);
    execute_insert(t, 2, r2, 2);
    execute_insert(t, 3, r3, 2);
    TEST_ASSERT_EQUAL(3, count_rows(t));
    table_close(t);
}

void test_insert_duplicate_key(void) {
    Table *t = table_open(db, "users");
    char *r1[] = {"1", "Alice"};
    execute_insert(t, 1, r1, 2);
    ExecuteResult r = execute_insert(t, 1, r1, 2);
    TEST_ASSERT_EQUAL(EXECUTE_DUPLICATE_KEY, r);
    table_close(t);
}

void test_delete(void) {
    Table *t = table_open(db, "users");
    char *r1[] = {"1", "Alice"};
    char *r2[] = {"2", "Bob"};
    execute_insert(t, 1, r1, 2);
    execute_insert(t, 2, r2, 2);

    TEST_ASSERT_EQUAL(EXECUTE_SUCCESS, execute_delete(t, 1));
    TEST_ASSERT_EQUAL(1, count_rows(t));
    TEST_ASSERT_EQUAL(EXECUTE_KEY_NOT_FOUND, execute_delete(t, 99));
    table_close(t);
}

void test_update(void) {
    Table *t = table_open(db, "users");
    char *r1[] = {"1", "Alice"};
    execute_insert(t, 1, r1, 2);

    char *upd[] = {"1", "Alicia"};
    TEST_ASSERT_EQUAL(EXECUTE_SUCCESS, execute_update(t, 1, upd, 2));

    Cursor *c = table_find(t, 1);
    void *row = cursor_value(c);
    TEST_ASSERT_EQUAL_STRING("Alicia", (char *)row + sizeof(int32_t));
    free(c);
    table_close(t);
}

void test_update_not_found(void) {
    Table *t = table_open(db, "users");
    char *upd[] = {"99", "Ghost"};
    TEST_ASSERT_EQUAL(EXECUTE_KEY_NOT_FOUND, execute_update(t, 99, upd, 2));
    table_close(t);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_insert_and_count);
    RUN_TEST(test_insert_duplicate_key);
    RUN_TEST(test_delete);
    RUN_TEST(test_update);
    RUN_TEST(test_update_not_found);
    return UNITY_END();
}
