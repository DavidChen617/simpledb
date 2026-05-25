#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef RAGER_H
#define RAGER_H

#define PAGE_SIZE 4096
#define MAX_PAGES 100

typedef struct {
    int fd;
    uint32_t num_pages;
    void *pages[MAX_PAGES];
    bool is_dirty[MAX_PAGES];
} Pager;

Pager *pager_open(const char *filename);

void *pager_get_page(Pager *pager, uint32_t page_num);

void pager_mark_dirty(Pager *pager, uint32_t page_num);
void pager_flush(Pager *pager, uint32_t page_num);
void pager_close(Pager *pager);

#endif
