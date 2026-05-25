#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "pager.h"

Pager *pager_open(const char *filename) {
    const int fd = open(filename, O_RDWR | O_CREAT, 0600);

    if (fd < 0) {
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    const off_t file_length = lseek(fd, 0, SEEK_END);

    Pager *pager = malloc(sizeof(Pager));
    pager->fd = fd;
    pager->num_pages = (uint32_t) (file_length / PAGE_SIZE);

    for (int i = 0; i < MAX_PAGES; ++i) {
        pager->pages[i] = NULL;
        pager->is_dirty[i] = false;
    }

    return pager;
}

void *pager_get_page(Pager *pager, const uint32_t page_num) {
    if (page_num >= MAX_PAGES) {
        printf("Pager number out of bounds: %u\n", page_num);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        void *page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);
        if (page_num < pager->num_pages) {
            lseek(pager->fd, (off_t) page_num * PAGE_SIZE, SEEK_SET);
            read(pager->fd, page,PAGE_SIZE);
        } else {
            pager->is_dirty[page_num] = true;
            pager->num_pages = page_num + 1;
        }

        pager->pages[page_num] = page;
    }

    return pager->pages[page_num];
}

void pager_mark_dirty(Pager *pager, const uint32_t page_num) {
    pager->is_dirty[page_num] = true;
}

void pager_flush(Pager *pager, const uint32_t page_num) {
    if (pager->pages[page_num] == NULL)
        return;

    lseek(pager->fd, (off_t) page_num * PAGE_SIZE, SEEK_SET);
    write(pager->fd, pager->pages[page_num], PAGE_SIZE);
    pager->is_dirty[page_num] = false;
}

void pager_close(Pager *pager) {
    for (int i = 0; i < MAX_PAGES; ++i) {
        if (pager->pages[i]) {
            if (pager->is_dirty[i])
                pager_flush(pager, i);

            free(pager->pages[i]);
        }
    }

    close(pager->fd);
    free(pager);
}
