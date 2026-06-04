#include "unity.h"
#include "catalog.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

static TableMeta make_table_meta(const char *name) {
    TableMeta meta = {0};
    strncpy(meta.name, name, MAX_TABLE_NAME - 1);
    meta.num_columns = 1;
    strncpy(meta.columns[0].name, "id", MAX_COL_NAME - 1);
    meta.columns[0].type = COL_INT;
    meta.columns[0].size = sizeof(int32_t);
    meta.columns[0].offset = 0;
    meta.row_size = sizeof(int32_t);
    return meta;
}

void test_find_empty_catalog(void) {
    Catalog catalog = {0};
    TEST_ASSERT_EQUAL(-1, catalog_find(&catalog, "users"));
}

void test_add_and_find(void) {
    Catalog catalog = {0};
    TableMeta meta = make_table_meta("users");

    catalog_add(&catalog, &meta);

    TEST_ASSERT_EQUAL(0, catalog_find(&catalog, "users"));
    TEST_ASSERT_EQUAL_UINT32(1, catalog.num_tables);
}

void test_find_not_found(void) {
    Catalog catalog = {0};
    TableMeta meta = make_table_meta("users");
    catalog_add(&catalog, &meta);

    TEST_ASSERT_EQUAL(-1, catalog_find(&catalog, "orders"));
}

void test_remove_shifts_elements(void) {
    Catalog catalog = {0};
    TableMeta m1 = make_table_meta("users");
    TableMeta m2 = make_table_meta("orders");
    catalog_add(&catalog, &m1);
    catalog_add(&catalog, &m2);

    catalog_remove(&catalog, 0);

    TEST_ASSERT_EQUAL(-1, catalog_find(&catalog, "users"));
    TEST_ASSERT_EQUAL(0, catalog_find(&catalog, "orders"));
    TEST_ASSERT_EQUAL_UINT32(1, catalog.num_tables);
}

void test_add_max_tables(void) {
    Catalog catalog = {0};
    for (int i = 0; i < MAX_TABLES; i++) {
        char name[MAX_TABLE_NAME];
        snprintf(name, sizeof(name), "table%d", i);
        TableMeta meta = make_table_meta(name);
        TEST_ASSERT_NOT_EQUAL(-1, catalog_add(&catalog, &meta));
    }
    TableMeta extra = make_table_meta("overflow");
    TEST_ASSERT_EQUAL(-1, catalog_add(&catalog, &extra));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_find_empty_catalog);
    RUN_TEST(test_add_and_find);
    RUN_TEST(test_find_not_found);
    RUN_TEST(test_remove_shifts_elements);
    RUN_TEST(test_add_max_tables);
    return UNITY_END();
}
