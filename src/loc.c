#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "loc.h"

static LocEntry s_entries[LOC_MAX_KEYS];
static int      s_count = 0;

char g_available_langs[LOC_MAX_LANGS][LOC_LANG_LEN];
int  g_lang_count = 0;

int loc_scan_langs(void) {
    g_lang_count = 0;
    DIR *d = opendir("lang");
    if (!d) {
        printf("loc_scan_langs: could not open lang/\n");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && g_lang_count < LOC_MAX_LANGS) {
        char *name = entry->d_name;
        int len    = (int)strlen(name);
        if (len > 5 && name[0] != '.' && strcmp(name + len - 5, ".lang") == 0) {
            int code_len = len - 5;
            if (code_len < LOC_LANG_LEN) {
                strncpy(g_available_langs[g_lang_count], name, code_len);
                g_available_langs[g_lang_count][code_len] = '\0';
                g_lang_count++;
            }
        }
    }
    closedir(d);
    return 0;
}

int loc_load(const char *lang_code) {
    char path[256];
    snprintf(path, sizeof(path), "lang/%s.lang", lang_code);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("loc_load: could not open %s\n", path);
        return 1;
    }

    s_count = 0;
    char line[LOC_KEY_LEN + LOC_VAL_LEN + 4];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq      = '\0';
        char *key = line;
        char *val = eq + 1;

        if (s_count >= LOC_MAX_KEYS) {
            printf("loc_load: too many keys, increase LOC_MAX_KEYS\n");
            break;
        }

        strncpy(s_entries[s_count].key, key, LOC_KEY_LEN - 1);
        strncpy(s_entries[s_count].val, val, LOC_VAL_LEN - 1);
        s_entries[s_count].key[LOC_KEY_LEN - 1] = '\0';
        s_entries[s_count].val[LOC_VAL_LEN - 1] = '\0';
        s_count++;
    }

    fclose(f);
    return 0;
}

const char *loc_get(const char *key) {
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].key, key) == 0) {
            return s_entries[i].val;
        }
    }
    return key;
}

void loc_quit(void) {
    s_count = 0;
}