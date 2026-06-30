#pragma once

#include <SDL2/SDL.h>

/* gl_renderer
 * OpenGL 3.3 core profile renderer for TerraM4KC.
 * Only active when launched with --opengl.
 * All entry points are no-ops when compiled without GL support.
 */

int  gl_renderer_init    (SDL_Window *window);
void gl_renderer_quit    (void);

/* Call once per frame instead of gameLoop() + SDL_RenderPresent().
 * Returns 0 to quit, 1 to keep running - same contract as gameLoop(). */
int  gl_renderer_frame   (void *inputs);

/* Expose the GL context in case other modules need it (e.g. ImGui) */
SDL_GLContext gl_renderer_get_context (void);