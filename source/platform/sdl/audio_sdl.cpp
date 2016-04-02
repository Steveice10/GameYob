#ifdef BACKEND_SDL

#include "platform/common/manager.h"
#include "platform/audio.h"
#include "platform/gfx.h"

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

u32 audioGetSampleRate() {
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

    // If we're fast-forwarding or not going fast enough, clear the audio queue to prevent build-up.
    if(mgrGetFastForward() || SDL_GetQueuedAudioSize(device) >= (u32) ((audioGetSampleRate() / 59.7) * 10) * sizeof(u32)) {
        SDL_ClearQueuedAudio(device);
    }

    SDL_QueueAudio(device, buffer, samples * sizeof(u32));
}

#endif