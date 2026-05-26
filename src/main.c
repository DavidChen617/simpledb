#include <stdio.h>
#include "table.h"
#include "repl.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <database_file>\n", argv[0]);
        return 1;
    }

    Database *db = db_open(argv[1]);
    run_repl(db);
    db_close(db);
    return 0;
}
