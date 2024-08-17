#include "SDLAudioPlayer.h"
#include <spdlog/spdlog.h>

namespace Ship {
SDLAudioPlayer::SDLAudioPlayer() : AudioPlayer() {
}

SDLAudioPlayer::~SDLAudioPlayer() {
    SPDLOG_TRACE("destruct SDL audio player");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SDLAudioPlayer::DoInit(void) {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        SPDLOG_ERROR("SDL init error: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = this->GetSampleRate();
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 512;
    want.callback = NULL;
    mDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (mDevice == 0) {
        SPDLOG_ERROR("SDL_OpenAudio error: {}", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(mDevice, 0);
    return true;
}

int SDLAudioPlayer::Buffered(void) {
    // 4 is sizeof(int16_t) * num_channels (2 for stereo)
    return SDL_GetQueuedAudioSize(mDevice) / 4;
}

int SDLAudioPlayer::GetDesiredBuffered(void) {
    return this->mDesiredBuffered;
}

void SDLAudioPlayer::Play(const uint8_t* buf, size_t len) {
    if (Buffered() < 6000) {
        // Don't fill the audio buffer too much in case this happens
        SDL_QueueAudio(mDevice, buf, len);
    }
}
} // namespace Ship
