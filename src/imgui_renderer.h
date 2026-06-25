#pragma once

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Filled by gameloop each frame for the debug overlay
typedef struct {
    int   fps;
    int   blockSelected;
    int   blockX, blockY, blockZ;
} ImguiDebugState;

extern ImguiDebugState g_imgui_debug;

void imgui_init(SDL_Window *window, SDL_Renderer *renderer);
void imgui_shutdown(void);
void imgui_process_event(SDL_Event *event);
void imgui_new_frame(void);
void imgui_render(void);
int  imgui_wants_mouse(void);
int  imgui_wants_keyboard(void);

#ifdef __cplusplus
}
#endif