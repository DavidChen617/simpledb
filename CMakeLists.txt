cmake_minimum_required(VERSION 4.0)
project(simpledb C)

set(CMAKE_C_STANDARD 11)

add_executable(simpledb
        src/pager.h
        src/pager.c
        src/schema.h
        src/catalog.h
        src/catalog.c
        src/row.h
        src/row.c
        src/table.h
        src/table.c
        src/btree.h
        src/cursor.h
        src/btree.c
        src/cursor.c
        src/executor.h
        src/executor.c
        src/repl.h
        src/repl.c
        src/main.c
)
