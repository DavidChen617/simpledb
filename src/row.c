#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "row.h"

int row_serialize(TableMeta *meta, char **tokens, const int num_tokens, void *dest) {
    if (num_tokens < (int) meta->num_columns) {
        printf("Expected %d values, got %d\n", meta->num_columns, num_tokens);
        return 0;
    }

    memset(dest, 0, meta->row_size);

    for (uint32_t i = 0; i < meta->num_columns; ++i) {
        Column *col = &meta->columns[i];
        void *target = (char *) dest + col->offset;

        if (col->type == COL_INT) {
            int32_t v = atoi(tokens[i]);
            memcpy(target, &v, sizeof(int32_t));
        } else
            strncpy((char *)target, tokens[i], col->size - 1);
    }

    return 1;
}

void row_print(const TableMeta *meta, void *row_data) {
    printf("(");
    for (uint32_t i = 0; i < meta->num_columns; ++i) {
        const Column *col = &meta->columns[i];
        void *field = (char *) row_data + col->offset;
        if (i > 0)
            printf(", ");

        if (col->type == COL_INT) {
            int32_t v;
            memcpy(&v, field, sizeof(int32_t));
            printf("%d", v);
        } else
            printf("%s", (char *) field);
    }
    printf(")\n");
}

bool row_match(const TableMeta *meta, void *row_data, const char *col_name, const char *op, const char *val) {
    // 沒有 where
    if (!col_name)
        return true;

    for (uint32_t i = 0; i < meta->num_columns; ++i) {
        const Column *col = &meta->columns[i];
        if (strcmp(col->name, col_name) != 0)
            continue;

        void *field = (char *) row_data + col->offset;
        if (col->type == COL_INT) {
            int32_t row_val;
            memcpy(&row_val, field, sizeof(int32_t));
            const int32_t cmp_val = atoi(val);
            if (strcmp(op, "=") == 0)
                return row_val == cmp_val;
            if (strcmp(op, ">") == 0)
                return row_val > cmp_val;
            if (strcmp(op, "<") == 0)
                return row_val < cmp_val;
            if (strcmp(op, ">=") == 0)
                return row_val >= cmp_val;
            if (strcmp(op, "<=") == 0)
                return row_val <= cmp_val;
        } else {
            const int cmp = strcmp((char *) field, val);
            if (strcmp(op, "=") == 0)
                return cmp == 0;
            if (strcmp(op, ">") == 0)
                return cmp > 0;
            if (strcmp(op, "<") == 0)
                return cmp < 0;
        }
    }

    return false;
}

void row_print_cols(const TableMeta *meta, void *row_data, char **cols, int num_cols) {
    printf("(");
    for (int j = 0; j < num_cols; ++j) {
        if (j > 0)
            printf(", ");
        for (uint32_t i = 0; i < meta->num_columns; ++i) {
            const Column *col = &meta->columns[i];
            if (strcasecmp(col->name, cols[j]) != 0)
                continue;

            void *field = (char *) row_data + col->offset;
            if (col->type == COL_INT) {
                int32_t v;
                memcpy(&v, field, sizeof(int32_t));
                printf("%d", v);
            } else
                printf("%s", (char *) field);

            break;
        }
    }
    printf(")\n");
}
