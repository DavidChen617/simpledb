CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src -Ivendor/unity
SRC    = ../src/pager.c ../src/catalog.c ../src/row.c \
         ../src/table.c ../src/btree.c ../src/cursor.c \
         ../src/executor.c
UNITY  = vendor/unity/unity.c

UNIT_BINS        = unit/test_row unit/test_catalog
INTEGRATION_BINS = integration/test_crud integration/test_persistence
ALL_BINS         = $(UNIT_BINS) $(INTEGRATION_BINS)

all: $(ALL_BINS)

unit/test_row: unit/test_row.c $(SRC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

unit/test_catalog: unit/test_catalog.c $(SRC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

integration/test_crud: integration/test_crud.c $(SRC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

integration/test_persistence: integration/test_persistence.c $(SRC) $(UNITY)
	$(CC) $(CFLAGS) $^ -o $@

run: all
	@echo "=== Unit Tests ==="
	@./unit/test_row
	@./unit/test_catalog
	@echo ""
	@echo "=== Integration Tests ==="
	@./integration/test_crud
	@./integration/test_persistence

clean:
	rm -f $(ALL_BINS)
