#pragma once

#include "renderer.h"
#ifdef HAVE_SOUNDTOUCH
#include <soundtouch/SoundTouch.h>
#include <memory>
#include <vector>
#endif
#include "SDL_compat.h"

class SdlAudioRenderer : public IAudioRenderer
{
public:
    SdlAudioRenderer();

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual AudioFormat getAudioBufferFormat();

private:
    bool m_Wan = false;
#ifdef HAVE_SOUNDTOUCH
    std::unique_ptr<soundtouch::SoundTouch> m_Tempo;
    std::vector<float> m_TempoBuffer;
    unsigned int m_Channels = 0;
#endif
    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    Uint32 m_FrameSize;
    Uint32 m_FrameDurationMs;
};
