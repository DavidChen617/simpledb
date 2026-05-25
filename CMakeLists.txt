cmake_minimum_required(VERSION 4.0)
project(simpledb C)

set(CMAKE_C_STANDARD 11)

add_executable(simpledb
        src/pager.h
        src/pager.c
        src/schema.h
        src/catalog.h)
