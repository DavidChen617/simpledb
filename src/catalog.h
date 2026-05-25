#ifndef CATALOG_H
#define CATALOG_H
#include <assert.h>
#include "schema.h"
#include "pager.h"
#define CATALOG_PAGE 0

typedef struct {
    uint32_t num_tables;
    TableMeta tables[MAX_TABLES];
} Catalog;

static_assert(sizeof(Catalog) <= PAGE_SIZE, "Catalog exceeds one page- reduce MAX_TABLES or MAX_COLUMNS");

void catalog_load(Pager *pager, Catalog *catalog);

void catalog_flush(Pager *pager, const Catalog *catalog);

int catalog_find(const Catalog *catalog, const char *name);

int catalog_add(Catalog *catalog, const TableMeta *meta);

void catalog_remove(Catalog *catalog, int idx);

#endif
