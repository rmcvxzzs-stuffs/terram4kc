#pragma once
#include <dirent.h>

#define LOC_MAX_KEYS  128
#define LOC_KEY_LEN   64
#define LOC_VAL_LEN   128
#define LOC_MAX_LANGS 16
#define LOC_LANG_LEN  8

typedef struct {
    char key[LOC_KEY_LEN];
    char val[LOC_VAL_LEN];
} LocEntry;

extern char g_available_langs[LOC_MAX_LANGS][LOC_LANG_LEN];
extern int  g_lang_count;

int         loc_scan_langs (void);
int         loc_load       (const char *lang_code);
const char *loc_get        (const char *key);
void        loc_quit       (void);

#define T(key) loc_get(key)