#ifdef BACKEND_SDL

#include <assert.h>
#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <SDL2/SDL.h>

// TODO: Fix audio stutters.

class Sound_Queue {
public:
    Sound_Queue() {
        this->bufs = NULL;
        this->free_sem = NULL;
        this->sound_open = false;
    }

    ~Sound_Queue() {
        this->stop();
    }

    const char* start(long sample_rate, int chan_count = 1) {
        assert(!this->bufs);

        this->write_buf = 0;
        this->write_pos = 0;
        this->read_buf = 0;

        this->bufs = new u16[(long) buf_size * buf_count];
        if(!this->bufs) {
            return "Out of memory";
        }

        this->free_sem = SDL_CreateSemaphore(buf_count - 1);
        if(!this->free_sem) {
            return sdl_error("Couldn't create semaphore");
        }

        SDL_AudioSpec as;
        as.freq = (int) sample_rate;
        as.format = AUDIO_S16SYS;
        as.channels = (Uint8) chan_count;
        as.silence = 0;
        as.samples = buf_size / chan_count;
        as.size = 0;
        as.callback = fill_buffer_;
        as.userdata = this;
        if(SDL_OpenAudio(&as, NULL) < 0) {
            return sdl_error("Couldn't open SDL audio");
        }

        SDL_PauseAudio(false);
        this->sound_open = true;

        return NULL;
    }

    void stop() {
        if(this->sound_open) {
            this->sound_open = false;
            SDL_PauseAudio( true );
            SDL_CloseAudio();
        }

        if(this->free_sem) {
            SDL_DestroySemaphore(this->free_sem);
            this->free_sem = NULL;
        }

        delete[] this->bufs;
        this->bufs = NULL;
    }

    void write(const u16* in, int count) {
        while(count) {
            int n = buf_size - write_pos;
            if(n > count) {
                n = count;
            }

            memcpy(this->buf(this->write_buf) + this->write_pos, in, n * sizeof(u16));
            in += n;
            this->write_pos += n;
            count -= n;

            if(this->write_pos >= buf_size) {
                this->write_pos = 0;
                this->write_buf = (this->write_buf + 1) % buf_count;
                SDL_SemWait(this->free_sem);
            }
        }
    }

private:
    enum { buf_size = APU_BUFFER_SIZE };
    enum { buf_count = 3 };

    u16* volatile bufs;
    SDL_sem* volatile free_sem;
    int volatile read_buf;
    int write_buf;
    int write_pos;
    bool sound_open;

    static const char* sdl_error(const char* out) {
        const char* error = SDL_GetError();
        if(error && *error) {
            out = error;
        }

        return out;
    }

    inline u16* buf(int index) {
        assert((unsigned) index < buf_count);

        return this->bufs + (long) index * buf_size;
    }

    void fill_buffer(Uint8* out, int count) {
        if(SDL_SemValue(this->free_sem) < buf_count - 1) {
            memcpy(out, buf(this->read_buf), (size_t) count);
            this->read_buf = (this->read_buf + 1) % buf_count;
            SDL_SemPost(this->free_sem);
        } else {
            memset(out, 0, (size_t) count);
        }
    }

    static void fill_buffer_(void* user_data, Uint8* out, int count) {
        ((Sound_Queue*) user_data)->fill_buffer(out, count);
    }
};

static Sound_Queue* soundQueue;

static u16* audioLeftBuffer;
static u16* audioRightBuffer;
static u16* audioCenterBuffer;

void audioInit() {
    soundQueue = new Sound_Queue();
    soundQueue->start((long) SAMPLE_RATE, 1);

    audioLeftBuffer = (u16*) malloc(APU_BUFFER_SIZE * sizeof(u16));
    audioRightBuffer = (u16*) malloc(APU_BUFFER_SIZE * sizeof(u16));
    audioCenterBuffer = (u16*) malloc(APU_BUFFER_SIZE * sizeof(u16));
}

void audioCleanup() {
    soundQueue->stop();
    delete soundQueue;

    if(audioLeftBuffer != NULL) {
        free(audioLeftBuffer);
        audioLeftBuffer = NULL;
    }

    if(audioRightBuffer != NULL) {
        free(audioRightBuffer);
        audioRightBuffer = NULL;
    }

    if(audioCenterBuffer != NULL) {
        free(audioCenterBuffer);
        audioCenterBuffer = NULL;
    }
}

u16* audioGetLeftBuffer() {
    return audioLeftBuffer;
}

u16* audioGetRightBuffer() {
    return audioRightBuffer;
}

u16* audioGetCenterBuffer() {
    return audioCenterBuffer;
}

void audioPlay(long leftSamples, long rightSamples, long centerSamples) {
    // TODO: Stereo
    soundQueue->write(audioCenterBuffer, (int) centerSamples);
}

#endif