/* The Finder's mock listing (web/src/desktop/apps/FinderApp/files.ts), kept
 * for lp-render's previews so the parity PNGs never show a real home folder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "maryui/lp_app.h"
#include "maryui/lp_files.h"

struct mock { const char *name; enum lp_file_kind kind; int64_t size; int hours_ago; };

static const struct mock REPOSITORIES[] = {
    { "Mary", LP_FILE_FOLDER, 0, 2 }, { "MaryUI", LP_FILE_FOLDER, 0, 1 }, { "Bonnie", LP_FILE_FOLDER, 0, 2 },
    { "Conduit", LP_FILE_FOLDER, 0, 3 }, { "Fleet", LP_FILE_FOLDER, 0, 2 }, { "Frigate", LP_FILE_FOLDER, 0, 24 * 4 },
    { "Sewn", LP_FILE_FOLDER, 0, 2 }, { "Thread", LP_FILE_FOLDER, 0, 1 },
    { "Design principles.md", LP_FILE_DOCUMENT, 12288, 24 * 9 }, { "monogram.svg", LP_FILE_IMAGE, 6144, 1 },
    { "package.json", LP_FILE_CODE, 1024, 1 }, { "mary-intro.m4a", LP_FILE_MUSIC, 3200000, 24 * 6 },
};

int lp_finder_mock_fill(lp_file_list *l) {
    lp_files_list_free(l);
    time_t now = time(NULL);
    int n = (int)(sizeof REPOSITORIES / sizeof REPOSITORIES[0]);
    l->entries = calloc((size_t)n, sizeof *l->entries);
    if (!l->entries) return -1;
    l->cap = n;
    for (int i = 0; i < n; i++) {
        lp_file_entry *e = &l->entries[l->count++];
        snprintf(e->name, sizeof e->name, "%s", REPOSITORIES[i].name);
        e->kind = REPOSITORIES[i].kind;
        e->is_dir = e->kind == LP_FILE_FOLDER;
        e->size = REPOSITORIES[i].size;
        e->mtime = now - REPOSITORIES[i].hours_ago * 3600;
        e->mode = e->is_dir ? 040755 : 0100644;
    }
    l->free_bytes = 412800000000ull;
    return n;
}
