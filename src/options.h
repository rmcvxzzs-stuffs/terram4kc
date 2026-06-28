#pragma once
#include "inputbuffer.h"

typedef struct {
        int    fogType;
        int    drawDistance;
        int    trapMouse;
        double fov;
        InputBuffer username;
        char   lang[8];
} Options;

extern Options options;

int options_init (void);
int options_load (void);
int options_save (void);
