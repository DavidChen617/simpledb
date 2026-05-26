#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "table.h"

typedef struct Cursor{
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
} Cursor;

Cursor *table_start(Table *table);
void *cursor_value(const Cursor *cursor);
void cursor_advance(Cursor *cursor);

#endif
