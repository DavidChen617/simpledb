#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "row.h"

int row_serialize(TableMeta *meta, char **tokens, const int num_tokens, void *dest) {
    if (num_tokens < (int)meta->num_columns) {
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
        void *field = (char *)row_data + col->offset;
        if (i > 0)
            printf(", ");

        if (col->type == COL_INT) {
            int32_t v;
            memcpy(&v, field, sizeof(int32_t));
            printf("%d", v);
        }else
            printf("%s", (char *)field);
    }
    printf(")\n");
}
