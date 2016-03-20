#ifdef BACKEND_SDL

#include <assert.h>
#include <string.h>

#include "platform/audio.h"
#include "platform/gfx.h"
#include "gameboy.h"

#include <SDL2/SDL.h>

static bool initialized = false;

static SDL_AudioDeviceID device;

void audioInit() {
    SDL_AudioSpec as;
    as.freq = (int) audioGetSampleRate();
    as.format = AUDIO_S16SYS;
    as.channels = 2;
    as.silence = 0;
    as.samples = 2048;
    as.size = 0;
    as.callback = NULL;
    as.userdata = NULL;
    if((device = SDL_OpenAudioDevice(NULL, 0, &as, &as, 0)) < 0) {
        return;
    }

    SDL_PauseAudioDevice(device, false);

    initialized = true;
}

void audioCleanup() {
    initialized = false;

    if(device != 0) {
        SDL_PauseAudioDevice(device, true);
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

u16 audioGetSampleRate() {
    return 44100;
}

void audioClear() {
    if(!initialized) {
        return;
    }

    SDL_ClearQueuedAudio(device);
}

void audioPlay(u32* buffer, long samples) {
    if(!initialized) {
        return;
    }

    // If we're fast-forwarding, clear the audio queue to prevent build-up.
    if(gfxGetFastForward()) {
        SDL_ClearQueuedAudio(device);
    }

    SDL_QueueAudio(device, buffer, samples * sizeof(u32));
}

#endif