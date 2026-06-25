#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"

static ma_engine engine;
static ma_sound clickSound;
static int initialized = 0;

int audio_init(void) {
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("ma_engine_init failed: %d\n", result);
        return -1;
    }

    result = ma_sound_init_from_file(&engine, "assets/sounds/click.wav", 0, NULL, NULL, &clickSound);
  //printf("Trying to load: assets/sounds/click.wav\n");
  //printf("ma_result: %d\n", result);
    
    if (result != MA_SUCCESS) {
        printf("ma_sound_init_from_file failed, audio disabled\n");
        ma_engine_uninit(&engine);
        return -1;
    }

    initialized = 1;
    printf("Audio initialized successfully!\n");
    return 0;
}

void audio_quit(void) {
    if (!initialized) return;
    ma_sound_uninit(&clickSound);
    ma_engine_uninit(&engine);
    initialized = 0;
}

void audio_play_click(void) {
    if (!initialized) return;
    ma_sound_seek_to_pcm_frame(&clickSound, 0);
    ma_sound_start(&clickSound);
}