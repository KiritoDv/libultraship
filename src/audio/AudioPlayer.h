#pragma once
#include "stdint.h"
#include "stddef.h"
#include <string>

namespace Ship {
class AudioPlayer {

  public:
    AudioPlayer();
    ~AudioPlayer();

    bool Init(void);
    virtual int Buffered(void) = 0;
    virtual int GetDesiredBuffered(void) = 0;
    virtual void Play(const uint8_t* buf, size_t len) = 0;

    bool IsInitialized(void);

    constexpr int GetSampleRate() const {
        return mSampleRate;
    }

  protected:
    virtual bool DoInit(void) = 0;

    int mDesiredBuffered = 1100;
    int mSampleRate = 32000;
  private:
    bool mInitialized;
};
} // namespace Ship

#ifdef _WIN32
#include "WasapiAudioPlayer.h"
#endif

#include "SDLAudioPlayer.h"
