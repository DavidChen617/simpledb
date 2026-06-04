#include "unity.h"
#include "row.h"
#include <string.h>
#include <stdlib.h>

static TableMeta make_meta(void) {
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
    return meta;
}

void setUp(void) {}
void tearDown(void) {}

void test_serialize_int(void) {
    TableMeta meta = make_meta();
    char *tokens[] = {"42", "Alice"};
    uint8_t buf[sizeof(int32_t) + 64];

    TEST_ASSERT_EQUAL(1, row_serialize(&meta, tokens, 2, buf));

    int32_t v;
    memcpy(&v, buf, sizeof(int32_t));
    TEST_ASSERT_EQUAL_INT32(42, v);
}

void test_serialize_text(void) {
    TableMeta meta = make_meta();
    char *tokens[] = {"1", "Bob"};
    uint8_t buf[sizeof(int32_t) + 64];

    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_EQUAL_STRING("Bob", (char *)(buf + sizeof(int32_t)));
}

void test_serialize_insufficient_tokens(void) {
    TableMeta meta = make_meta();
    char *tokens[] = {"1"};
    uint8_t buf[sizeof(int32_t) + 64];

    TEST_ASSERT_EQUAL(0, row_serialize(&meta, tokens, 1, buf));
}

void test_match_no_where(void) {
    TableMeta meta = make_meta();
    uint8_t buf[sizeof(int32_t) + 64];
    char *tokens[] = {"10", "Alice"};
    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_TRUE(row_match(&meta, buf, NULL, NULL, NULL));
}

void test_match_int_eq(void) {
    TableMeta meta = make_meta();
    uint8_t buf[sizeof(int32_t) + 64];
    char *tokens[] = {"10", "Alice"};
    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_TRUE(row_match(&meta, buf, "id", "=", "10"));
    TEST_ASSERT_FALSE(row_match(&meta, buf, "id", "=", "99"));
}

void test_match_int_gt_lt(void) {
    TableMeta meta = make_meta();
    uint8_t buf[sizeof(int32_t) + 64];
    char *tokens[] = {"50", "Alice"};
    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_TRUE(row_match(&meta, buf, "id", ">", "30"));
    TEST_ASSERT_FALSE(row_match(&meta, buf, "id", ">", "50"));
    TEST_ASSERT_TRUE(row_match(&meta, buf, "id", "<", "100"));
    TEST_ASSERT_FALSE(row_match(&meta, buf, "id", "<", "50"));
}

void test_match_int_gte_lte(void) {
    TableMeta meta = make_meta();
    uint8_t buf[sizeof(int32_t) + 64];
    char *tokens[] = {"50", "Alice"};
    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_TRUE(row_match(&meta, buf, "id", ">=", "50"));
    TEST_ASSERT_TRUE(row_match(&meta, buf, "id", "<=", "50"));
    TEST_ASSERT_FALSE(row_match(&meta, buf, "id", ">=", "51"));
}

void test_match_text_eq(void) {
    TableMeta meta = make_meta();
    uint8_t buf[sizeof(int32_t) + 64];
    char *tokens[] = {"1", "Alice"};
    row_serialize(&meta, tokens, 2, buf);

    TEST_ASSERT_TRUE(row_match(&meta, buf, "name", "=", "Alice"));
    TEST_ASSERT_FALSE(row_match(&meta, buf, "name", "=", "Bob"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_int);
    RUN_TEST(test_serialize_text);
    RUN_TEST(test_serialize_insufficient_tokens);
    RUN_TEST(test_match_no_where);
    RUN_TEST(test_match_int_eq);
    RUN_TEST(test_match_int_gt_lt);
    RUN_TEST(test_match_int_gte_lte);
    RUN_TEST(test_match_text_eq);
    return UNITY_END();
}
