#include <string.h>
#include "catalog.h"

void catalog_load(Pager *pager, Catalog *catalog) {
    const void *page = pager_get_page(pager, CATALOG_PAGE);
    memcpy(catalog, page, sizeof(Catalog));
}

void catalog_flush(Pager *pager, const Catalog *catalog) {
    void *page = pager_get_page(pager, CATALOG_PAGE);
    memcpy(page, catalog, sizeof(Catalog));

    pager_mark_dirty(pager, CATALOG_PAGE);
}

int catalog_find(const Catalog *catalog, const char *name) {
    for (uint32_t i = 0; i < catalog->num_tables; ++i)
        if (strcmp(catalog->tables[i].name, name) == 0)
            return (int) i;

    return -1;
}

int catalog_add(Catalog *catalog, const TableMeta *meta) {
    if (catalog->num_tables >= MAX_TABLES)
        return -1;

    catalog->tables[catalog->num_tables++] = *meta;
    return (int) catalog->num_tables - 1;
}

void catalog_remove(Catalog *catalog, const int idx) {
    for (uint32_t i = (uint32_t)idx; i < catalog->num_tables - 1; ++i)
        catalog->tables[i] = catalog->tables[i + 1];
    catalog->num_tables--;
}
