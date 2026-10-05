#pragma once

#include <Limelight.h>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class StreamRecorder
{
public:
    enum class State { Idle, Starting, Recording, Stopping, Failed };

    StreamRecorder();
    ~StreamRecorder();

    StreamRecorder(const StreamRecorder&) = delete;
    StreamRecorder& operator=(const StreamRecorder&) = delete;

    void configureVideo(int videoFormat, int width, int height, int frameRate);
    void configureAudio(const OPUS_MULTISTREAM_CONFIGURATION& config);
    bool start(const QString& outputPath, std::int64_t presentationStartUs);
    void requestStop();
    void stop();
    bool active() const;
    State state() const { return m_State.load(); }
    QString outputPath() const;
    QString statusText() const;

    void submitVideo(const std::uint8_t* data, int size, std::int64_t ptsUs, bool keyFrame);
    void submitVideoDecodeUnit(PDECODE_UNIT decodeUnit);
    void submitAudio(const std::uint8_t* data, int size, std::int64_t presentationTimeUs);

private:
    struct Packet {
        bool video = false;
        bool keyFrame = false;
        std::int64_t pts = 0;
        std::int64_t duration = 0;
        std::uint64_t videoOrder = 0;
        std::vector<std::uint8_t> data;
    };

    void workerMain();
    void setError(const QString& error);

    mutable std::mutex m_Mutex;
    std::condition_variable m_Condition;
    std::deque<Packet> m_Queue;
    std::thread m_Worker;
    std::atomic<State> m_State{State::Idle};
    bool m_StopRequested = false;
    std::size_t m_QueuedBytes = 0;
    std::int64_t m_RequestedStartUs = 0;
    std::int64_t m_NextAudioPtsUs = 0;
    bool m_AudioClockStarted = false;
    std::uint64_t m_NextVideoOrder = 0;
    int m_VideoFormat = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_FrameRate = 0;
    OPUS_MULTISTREAM_CONFIGURATION m_AudioConfig{};
    bool m_HasAudioConfig = false;
    QString m_OutputPath;
    QString m_Error;
};
