#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "repl.h"
#include "executor.h"
#include "catalog.h"
#include "btree.h"
#include "schema.h"

#define MAX_TOKENS 64

static int tokenize(char *buf, char **tokens) {
    int n = 0;
    char *tok = strtok(buf, " \t\r\n");
    while (tok && n < MAX_TOKENS) {
        tokens[n++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    return n;
}

// create table users (id INT, name TEXT, age INT)
static void handle_create(Database *db, char **t, int n) {
    if (n < 5 || strcasecmp(t[1], "table") != 0) {
        printf("Usage: create table <name> (<col> <type>, ...)\n");
        return;
    }
    const char *tname = t[2];
    if (catalog_find(&db->catalog, tname) >= 0) {
        printf("Errors: table '%s' already exists\n", tname);
        return;
    }

    TableMeta meta = {0};
    strncpy(meta.name, tname, MAX_TABLE_NAME - 1);
    uint32_t offset = 0, col_idx = 0;
    int i = 3;
    while (i < n && col_idx < MAX_COLUMNS) {
        char *col_name = t[i];
        while (*col_name == '(')
            ++col_name;
        int len = (int) strlen(col_name);
        while (len > 0 && (col_name[len - 1] == ',' || col_name[len - 1] == ')'))
            col_name[--len] = '\0';
        if (len == 0) {
            ++i;
            continue;
        }
        ++i;
        if (i >= n)
            break;

        char *col_type = t[i++];
        len = (int) strlen(col_type);
        while (len > 0 && (col_type[len - 1] == ',' || col_type[len - 1] == ')'))
            col_type[--len] = '\0';

        Column *col = &meta.columns[col_idx];
        strncpy(col->name, col_name, MAX_COL_NAME - 1);
        col->offset = offset;
        if (strcasecmp(col_type, "INT") == 0) {
            col->type = COL_INT;
            col->size = sizeof(int32_t);
        } else {
            col->type = COL_TEXT;
            col->size = 64;
        }
        offset += col->size;
        ++col_idx;
    }

    meta.num_columns = col_idx;
    meta.row_size = offset;
    meta.root_page_num = db->pager->num_pages;

    if (catalog_add(&db->catalog, &meta) < 0) {
        printf("Error: max tables reached\n");
        return;
    }

    void *root = pager_get_page(db->pager, meta.root_page_num);
    initialize_leaf_node(root);
    set_node_root(root, true);
    catalog_flush(db->pager, &db->catalog);
    printf("Table '%s' created.\n", tname);
}

void run_repl(Database *db) {
    while (1) {
        char *line = readline("db > ");
        if (!line)
            break;
        if (*line)
            add_history(line);

        char *tokens[MAX_TOKENS];
        int n = tokenize(line, tokens);
        if (n == 0) {
            free(line);
            continue;
        }
        if (tokens[0][0] == '.') {
            if (strcmp(tokens[0], ".exit") == 0) {
                free(line);
                break;
            }
            if (strcmp(tokens[0], ".btree") == 0 && n >= 2) {
                Table *table = table_open(db, tokens[1]);
                if (!table)
                    printf("Error: table '%s' not found\n", tokens[1]);
                else {
                    print_tree(table, table->root_page_num, 0);
                    table_close(table);
                }
                free(line);
                continue;
            }
            if (strcmp(tokens[0], ".list") == 0) {
                for (uint32_t i = 0; i < db->catalog.num_tables; ++i)
                    printf("%s\n", db->catalog.tables[i].name);
                free(line);
                continue;
            }
            printf("Unknown command: %s\n", tokens[0]);
            free(line);
            continue;
        }

        // create table
        if (strcasecmp(tokens[0], "create") == 0) {
            handle_create(db, tokens, n);
            free(line);
            continue;
        }

        // insert <table> <id> <val>
        if (strcasecmp(tokens[0], "insert") == 0) {
            // support: insert into <table> ...
            int ti = (n > 1 && strcasecmp(tokens[1], "into") == 0) ? 2 : 1;
            if (n < ti + 2) {
                printf("Usage: insert <table> <id> <val>...\n");
                free(line);
                continue;
            }
            Table *table = table_open(db, tokens[ti]);
            if (!table) {
                printf("Error: table '%s' not found\n", tokens[ti]);
                free(line);
                continue;
            }

            uint32_t key = (uint32_t) atoi(tokens[ti + 1]);
            ExecuteResult r = execute_insert(table, key, &tokens[ti + 1], n - ti - 1);
            if (r == EXECUTE_DUPLICATE_KEY)
                printf("Error: duplicate key %u\n", key);
            else if (r == EXECUTE_SUCCESS)
                printf("Inserted.\n");

            table_close(table);
            free(line);
            continue;
        }

        // select * from <table> [where <col> <op> <val>]
        if (strcasecmp(tokens[0], "select") == 0) {
            int fi = -1;
            for (int i = 1; i < n; ++i)
                if (strcasecmp(tokens[i], "from") == 0) {
                    fi = i;
                    break;
                }
            if (fi < 0 || fi + 1 >= n) {
                printf("Usage: select * from <table> [where <col> <op> <val>]\n");
                free(line);
                continue;
            }
            Table *table = table_open(db, tokens[fi + 1]);
            if (!table) {
                printf("Error: table '%s' not found\n", tokens[fi + 1]);
                free(line);
                continue;
            }
            const char *wcol = NULL, *wop = NULL, *wval = NULL;
            int wi = -1;
            for (int i = fi + 2; i < n; ++i)
                if (strcasecmp(tokens[i], "where") == 0) {
                    wi = i;
                    break;
                }
            if (wi >= 0 && wi + 3 < n) {
                wcol = tokens[wi + 1];
                wop = tokens[wi + 2];
                wval = tokens[wi + 3];
            }

            execute_select(table, wcol, wop, wval);
            table_close(table);
            free(line);
            continue;
        }

        if (strcasecmp(tokens[0], "update") == 0) {
            if (n < 4) {
                printf("Usage: update <table> <id> <val>..\n");
                free(line);
                continue;
            }

            Table *table = table_open(db, tokens[1]);
            if (!table) {
                printf("Error: table '%s' not found\n", tokens[1]);
                free(line);
                continue;
            }

            const uint32_t key = (uint32_t) atoi(tokens[2]);
            ExecuteResult r = execute_update(table, key, &tokens[2], n -2);
            if (r == EXECUTE_KEY_NOT_FOUND)
                printf("Error: key %u not found\n", key);
            else if (r == EXECUTE_SUCCESS)
                printf("Updated.\n");
            table_close(table);
            free(line);
            continue;
        }

        // update table id
        if (strcasecmp(tokens[0], "delete") == 0) {
            int ti = (n > 1 && strcasecmp(tokens[1], "from") == 0) ? 2 : 1;
            if (n < ti + 2) {
                printf("Usage: delete <table> <id>\n");
                free(line);
                continue;
            }

            Table *table = table_open(db, tokens[ti]);
            if (!table) {
                printf("Error: table '%s' not found\n", tokens[ti]);
                free(line);
                continue;
            }

            const uint32_t key = (uint32_t) atoi(tokens[ti + 1]);
            const ExecuteResult r = execute_delete(table, key);
            if (r == EXECUTE_KEY_NOT_FOUND)
                printf("Error: key '%s' not found\n", tokens[ti]);
            else if (r == EXECUTE_SUCCESS)
                printf("Deleted.\n");
            table_close(table);
            free(line);
            continue;
        }

        printf("Unknown command: %s\n", tokens[0]);
        free(line);
    }
}
