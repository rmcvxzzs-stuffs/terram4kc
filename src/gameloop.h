#pragma once

#include <SDL2/SDL.h>
#include "terrain.h"
#include "main.h"
#include "player.h"
 
int  gameLoop            (Inputs *inputs, SDL_Renderer *renderer);
void gameLoop_resetGame  ();
void gameLoop_error      (char *);
int  gameLoop_screenshot (SDL_Renderer *, const char *);

/* GL renderer accessors */
int  gl_getBlockSelected     (void);
void gl_getBlockSelectCoords (int *x, int *y, int *z);
int  gl_getGuiOn             (void);
int  gl_getDebugOn           (void);