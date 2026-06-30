#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "textures.h"
#include "gameloop.h"
#include "options.h"
#include "terrain.h"
#include "data.h"
#include "main.h"
#include "gui.h"
#include "audio.h"
#include "net.h"
#include "loc.h"
#include "imgui_renderer.h"
#include "discord_presence.h"
#include "gl_renderer.h"
#include "mcpi.h"

/* Minecraft 4k, C edition. Version 0.7
 *
 * Credits:
 *   notch       - creating the original game
 *   sashakoshka - C port, modifications
 *   samsebe     - deciphering the meaning of some of the code
 *   gracie bell - daylight function
 *   https://gist.github.com/nowl/828013 - perlin noise
 *
 *   ... & contributors on github!
 *   https://github.com/sashakoshka/m4kc/graphs/contributors
 *
 * If you distribute a modified copy of this, just include this
 * notice.
 */

/* TerraM4KC 0.1.0
 * Credits:
 *  sashakoshka - Creating the base game
 *  rmcvxzz     - Creator of TerraM4KC
 */

#define MAX_FPS 60
#define MIN_FRAME_MILLISECONDS 1000 / MAX_FPS

static int controlLoop(Inputs *, const uint8_t *);
static int handleEvent(Inputs *, const uint8_t *, SDL_Event);

int g_debug_mode = 0;
int g_use_opengl = 0;
int g_mcpi_on    = 0;

int main(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			g_debug_mode = 1;
		}
		if (strcmp(argv[i], "--opengl") == 0) {
			g_use_opengl = 1;
		}
		if (strcmp(argv[i], "--mcpi-on") == 0) {
			g_mcpi_on = 1;
		}
	}

	SDL_Window *window      = NULL;
	SDL_Renderer *renderer  = NULL;
	const uint8_t *keyboard = SDL_GetKeyboardState(NULL);

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("cant make window\n");
		goto exit;
	}

	window =
	    SDL_CreateWindow("TerraM4KC", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_W,
	                     WINDOW_H, SDL_WINDOW_SHOWN | (g_use_opengl ? SDL_WINDOW_OPENGL : 0));
	if (window == NULL) {
		printf("%s\n", SDL_GetError());
		goto exit;
	}

	if (g_use_opengl) {
		if (gl_renderer_init(window)) {
			printf("OpenGL renderer init failed\n");
			goto exit;
		}
		printf("[main] using OpenGL 3.3 renderer\n");
	} else {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		if (renderer == NULL) {
			printf("%s\n", SDL_GetError());
			goto exit;
		}
		SDL_RenderSetScale(renderer, BUFFER_SCALE, BUFFER_SCALE);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	}

	if (g_debug_mode && !g_use_opengl) {
		imgui_init(window, renderer);
	}

	//--- initializing modules ---//

	int err = 0;

	err = data_init();
	if (err) {
		gameLoop_error("Cannot initialize data module.");
	}

	err = options_init();
	if (err) {
		gameLoop_error("Cannot initialize options module.");
	}

	loc_scan_langs();
	if (options.lang[0] == '\0') {
		strncpy(options.lang, "en", sizeof(options.lang));
	}
	if (loc_load(options.lang) != 0) {
		printf("Failed to load language: %s, falling back to en\n", options.lang);
		loc_load("en");
	}

	err = audio_init();
	if (err) {
		printf("Audio init failed, continuing without sound\n");
	}

	err = discord_rpc_init();
	if (err) {
		printf("Discord RPC init failed, continuing without Discord integration\n");
	}

	genTextures(45390874);

	if (g_mcpi_on) {
		if (mcpi_init()) {
			printf("MCPI server init failed, continuing without it\n");
		}
	}

	Inputs inputs = {0};
	int running   = 1;
	while (running) {
		uint32_t frameStartTime = SDL_GetTicks();

		if (g_debug_mode && !g_use_opengl) {
			imgui_new_frame();
		}

		running &= controlLoop(&inputs, keyboard);

		if (g_use_opengl) {
			running &= gl_renderer_frame(&inputs);
		} else {
			running &= gameLoop(&inputs, renderer);
		}

		if (g_debug_mode && !g_use_opengl) {
			imgui_render();
		}

		if (!g_use_opengl) {
			SDL_RenderPresent(renderer);
			SDL_UpdateWindowSurface(window);
		}

		inputs.keyTyped = 0;
		inputs.keySym   = 0;

		uint32_t frameDuration = SDL_GetTicks() - frameStartTime;
		if (frameDuration < MIN_FRAME_MILLISECONDS) {
			SDL_Delay(MIN_FRAME_MILLISECONDS - frameDuration);
		}
	}

exit:
	if (g_mcpi_on)
		mcpi_quit();
	audio_quit();
	if (g_use_opengl)
		gl_renderer_quit();
	discord_rpc_quit();
	loc_quit();
	SDL_Quit();
	return 0;
}

static int controlLoop(Inputs *inputs, const Uint8 *keyboard) {
	SDL_PumpEvents();
	int mouseX = 0, mouseY = 0;
	SDL_GetMouseState(&mouseX, &mouseY);

	inputs->keyboard.space = keyboard[SDL_SCANCODE_SPACE];
	inputs->keyboard.w     = keyboard[SDL_SCANCODE_W];
	inputs->keyboard.s     = keyboard[SDL_SCANCODE_S];
	inputs->keyboard.a     = keyboard[SDL_SCANCODE_A];
	inputs->keyboard.d     = keyboard[SDL_SCANCODE_D];

	if (!SDL_GetRelativeMouseMode()) {
		inputs->mouse.x = mouseX;
		inputs->mouse.y = mouseY;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (!handleEvent(inputs, keyboard, event)) {
			return 0;
		}
	}

	return 1;
}

static int handleEvent(Inputs *inputs, const uint8_t *keyboard, SDL_Event event) {
	if (g_debug_mode) {
		imgui_process_event(&event);

		if (imgui_wants_mouse() && !SDL_GetRelativeMouseMode()) {
			if (event.type == SDL_QUIT)
				return 0;
			if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				// fall through
			} else {
				return 1;
			}
		}
	}

	switch (event.type) {
	case SDL_QUIT:
		return 0;

	case SDL_MOUSEBUTTONDOWN:
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			inputs->mouse.left = 1;
			break;
		case SDL_BUTTON_RIGHT:
			inputs->mouse.right = 1;
			break;
		}
		break;

	case SDL_MOUSEBUTTONUP:
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			inputs->mouse.left = 0;
			break;
		case SDL_BUTTON_RIGHT:
			inputs->mouse.right = 0;
			break;
		}
		break;

	case SDL_KEYDOWN:
		inputs->keySym = event.key.keysym.sym;
		__attribute__((fallthrough));
	case SDL_KEYUP:
		if (event.key.repeat == 0) {
			inputs->keyboard.esc = keyboard[SDL_SCANCODE_ESCAPE];
			inputs->keyboard.f1  = keyboard[SDL_SCANCODE_F1];
			inputs->keyboard.f2  = keyboard[SDL_SCANCODE_F2];
			inputs->keyboard.f3  = keyboard[SDL_SCANCODE_F3];
			inputs->keyboard.f4  = keyboard[SDL_SCANCODE_F4];
			inputs->keyboard.e   = keyboard[SDL_SCANCODE_E];
			inputs->keyboard.t   = keyboard[SDL_SCANCODE_T];
			inputs->keyboard.f   = keyboard[SDL_SCANCODE_F];

			inputs->keyboard.num0 = keyboard[SDL_SCANCODE_0];
			inputs->keyboard.num1 = keyboard[SDL_SCANCODE_1];
			inputs->keyboard.num2 = keyboard[SDL_SCANCODE_2];
			inputs->keyboard.num3 = keyboard[SDL_SCANCODE_3];
			inputs->keyboard.num4 = keyboard[SDL_SCANCODE_4];
			inputs->keyboard.num5 = keyboard[SDL_SCANCODE_5];
			inputs->keyboard.num6 = keyboard[SDL_SCANCODE_6];
			inputs->keyboard.num7 = keyboard[SDL_SCANCODE_7];
			inputs->keyboard.num8 = keyboard[SDL_SCANCODE_8];
			inputs->keyboard.num9 = keyboard[SDL_SCANCODE_9];
		}
		break;

	case SDL_MOUSEWHEEL:
		inputs->mouse.wheel = event.wheel.y;
		break;

	case SDL_MOUSEMOTION:
		if (SDL_GetRelativeMouseMode()) {
			inputs->mouse.x = event.motion.xrel;
			inputs->mouse.y = event.motion.yrel;
		}
		break;

	case SDL_TEXTINPUT:
		inputs->keyTyped = event.text.text[0];
		break;
	}

	return 1;
}

#ifdef __MINGW32__
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;
	return main(__argc, __argv);
}
#endif