CC	= gcc
CFLAGS	= -Wall -Wextra -std=c11 -Isrc
LDFLAGS	= -lreadline
SRC	= src/pager.c src/catalog.c src/row.c \
                  src/table.c src/btree.c src/cursor.c \
                  src/executor.c src/repl.c src/main.c
TARGET	= simpledb
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)