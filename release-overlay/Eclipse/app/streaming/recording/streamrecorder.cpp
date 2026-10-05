#include "streamrecorder.h"

#ifdef HAVE_FFMPEG
#include <QDir>
#include <QFile>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>
#include <opus.h>
}

#include <algorithm>
#include <chrono>
#include <cstring>

namespace {
constexpr std::size_t MAX_QUEUED_BYTES = 96 * 1024 * 1024;
constexpr std::size_t MAX_PREROLL_BYTES = 24 * 1024 * 1024;
constexpr int STARTUP_TIMEOUT_SECONDS = 15;

QString ffmpegErrorString(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return QString::fromLocal8Bit(buffer);
}

AVCodecID codecIdForVideoFormat(int format)
{
    switch (format) {
    case VIDEO_FORMAT_H264:
    case VIDEO_FORMAT_H264_HIGH8_444:
        return AV_CODEC_ID_H264;
    case VIDEO_FORMAT_H265:
    case VIDEO_FORMAT_H265_MAIN10:
    case VIDEO_FORMAT_H265_REXT8_444:
    case VIDEO_FORMAT_H265_REXT10_444:
        return AV_CODEC_ID_HEVC;
    case VIDEO_FORMAT_AV1_MAIN8:
    case VIDEO_FORMAT_AV1_MAIN10:
    case VIDEO_FORMAT_AV1_HIGH8_444:
    case VIDEO_FORMAT_AV1_HIGH10_444:
        return AV_CODEC_ID_AV1;
    default:
        return AV_CODEC_ID_NONE;
    }
}

AVPixelFormat pixelFormatForVideoFormat(int format)
{
    switch (format) {
    case VIDEO_FORMAT_H264_HIGH8_444:
    case VIDEO_FORMAT_H265_REXT8_444:
    case VIDEO_FORMAT_AV1_HIGH8_444:
        return AV_PIX_FMT_YUV444P;
    case VIDEO_FORMAT_H265_MAIN10:
    case VIDEO_FORMAT_AV1_MAIN10:
        return AV_PIX_FMT_YUV420P10LE;
    case VIDEO_FORMAT_H265_REXT10_444:
    case VIDEO_FORMAT_AV1_HIGH10_444:
        return AV_PIX_FMT_YUV444P10LE;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

struct MuxPacket {
    bool video = false;
    bool keyFrame = false;
    std::int64_t pts = 0;
    std::int64_t duration = 0;
    std::uint64_t videoOrder = 0;
    std::vector<std::uint8_t> data;
};
}

StreamRecorder::StreamRecorder() = default;

StreamRecorder::~StreamRecorder()
{
    stop();
}

void StreamRecorder::configureVideo(int videoFormat, int width, int height, int frameRate)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (active()) return;
    m_VideoFormat = videoFormat;
    m_Width = width;
    m_Height = height;
    m_FrameRate = frameRate;
}

void StreamRecorder::configureAudio(const OPUS_MULTISTREAM_CONFIGURATION& config)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (active()) return;
    m_AudioConfig = config;
    m_HasAudioConfig = true;
}

bool StreamRecorder::start(const QString& outputPath, std::int64_t presentationStartUs)
{
    stop();
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_VideoFormat == 0 || m_Width <= 0 || m_Height <= 0 || m_FrameRate <= 0 ||
            !m_HasAudioConfig || m_AudioConfig.channelCount <= 0 || m_AudioConfig.sampleRate <= 0) {
        m_Error = QStringLiteral("Recording could not start because the stream format is not ready.");
        m_State.store(State::Failed);
        return false;
    }
    if (codecIdForVideoFormat(m_VideoFormat) == AV_CODEC_ID_NONE) {
        m_Error = QStringLiteral("Recording does not support the active video codec.");
        m_State.store(State::Failed);
        return false;
    }

    m_OutputPath = outputPath;
    m_Error.clear();
    m_Queue.clear();
    m_QueuedBytes = 0;
    m_RequestedStartUs = presentationStartUs;
    m_NextAudioPtsUs = 0;
    m_AudioClockStarted = false;
    m_NextVideoOrder = 0;
    m_StopRequested = false;
    m_State.store(State::Starting);
    m_Worker = std::thread(&StreamRecorder::workerMain, this);
    return true;
}

void StreamRecorder::stop()
{
    requestStop();
    if (m_Worker.joinable() && m_Worker.get_id() != std::this_thread::get_id()) {
        m_Worker.join();
    }
}

void StreamRecorder::requestStop()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_State.load() == State::Starting || m_State.load() == State::Recording) {
            m_StopRequested = true;
            m_State.store(State::Stopping);
        }
    }
    m_Condition.notify_all();
}

bool StreamRecorder::active() const
{
    const State current = m_State.load();
    return current == State::Starting || current == State::Recording || current == State::Stopping;
}

QString StreamRecorder::outputPath() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_OutputPath;
}

QString StreamRecorder::statusText() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    const State current = m_State.load();
    if (current == State::Failed) return m_Error;
    if (current == State::Starting) return QStringLiteral("Preparing recording; waiting for a keyframe…");
    if (current == State::Recording) return QStringLiteral("Recording to %1").arg(QDir::toNativeSeparators(m_OutputPath));
    if (current == State::Stopping) return QStringLiteral("Finalizing %1").arg(QDir::toNativeSeparators(m_OutputPath));
    if (!m_OutputPath.isEmpty()) return QStringLiteral("Saved recording: %1").arg(QDir::toNativeSeparators(m_OutputPath));
    return {};
}

void StreamRecorder::submitVideo(const std::uint8_t* data, int size, std::int64_t ptsUs, bool keyFrame)
{
    if (size <= 0 || data == nullptr) return;
    Packet packet;
    packet.video = true;
    packet.keyFrame = keyFrame;
    packet.data.assign(data, data + size);
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_State.load() != State::Starting && m_State.load() != State::Recording) return;
    if (ptsUs < m_RequestedStartUs) return;
    if (m_QueuedBytes + static_cast<std::size_t>(size) > MAX_QUEUED_BYTES) {
        m_Error = QStringLiteral("Recording stopped because the disk writer could not keep up; the stream was not affected.");
        m_StopRequested = true;
        m_State.store(State::Failed);
        m_Condition.notify_one();
        return;
    }
    packet.pts = ptsUs;
    packet.duration = std::max<std::int64_t>(1, 1000000 / std::max(1, m_FrameRate));
    packet.videoOrder = m_NextVideoOrder++;
    m_QueuedBytes += packet.data.size();
    m_Queue.push_back(std::move(packet));
    m_Condition.notify_one();
}

void StreamRecorder::submitVideoDecodeUnit(PDECODE_UNIT decodeUnit)
{
    if (!decodeUnit || decodeUnit->fullLength <= 0 || !decodeUnit->bufferList) return;
    Packet packet;
    packet.video = true;
    packet.keyFrame = decodeUnit->frameType == FRAME_TYPE_IDR;
    packet.pts = static_cast<std::int64_t>(decodeUnit->presentationTimeUs);
    packet.duration = std::max<std::int64_t>(1, 1000000 / std::max(1, m_FrameRate));
    packet.data.reserve(decodeUnit->fullLength);
    for (PLENTRY entry = decodeUnit->bufferList; entry != nullptr; entry = entry->next) {
        if (entry->length > 0) packet.data.insert(packet.data.end(), entry->data, entry->data + entry->length);
    }
    if (packet.data.size() != static_cast<std::size_t>(decodeUnit->fullLength)) return;

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_State.load() != State::Starting && m_State.load() != State::Recording) return;
    if (packet.pts < m_RequestedStartUs) return;
    packet.videoOrder = m_NextVideoOrder++;
    if (m_QueuedBytes + packet.data.size() > MAX_QUEUED_BYTES) {
        m_Error = QStringLiteral("Recording stopped because the disk writer could not keep up; the stream was not affected.");
        m_StopRequested = true;
        m_State.store(State::Failed);
        m_Condition.notify_one();
        return;
    }
    m_QueuedBytes += packet.data.size();
    m_Queue.push_back(std::move(packet));
    m_Condition.notify_one();
}

void StreamRecorder::submitAudio(const std::uint8_t* data, int size, std::int64_t presentationTimeUs)
{
    if (size <= 0 || data == nullptr) return;
    const int samples = opus_packet_get_nb_samples(data, size, m_AudioConfig.sampleRate);
    if (samples <= 0) return;
    Packet packet;
    packet.video = false;
    packet.duration = av_rescale_q(samples, AVRational{1, m_AudioConfig.sampleRate}, AVRational{1, 1000000});
    packet.data.assign(data, data + size);
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_State.load() != State::Starting && m_State.load() != State::Recording) return;
    if (m_QueuedBytes + static_cast<std::size_t>(size) > MAX_QUEUED_BYTES) {
        m_Error = QStringLiteral("Recording stopped because the disk writer could not keep up; the stream was not affected.");
        m_StopRequested = true;
        m_State.store(State::Failed);
        m_Condition.notify_one();
        return;
    }
    if (presentationTimeUs < m_RequestedStartUs) return;
    // Audio callbacks can be delivered in bursts, so their wall-clock arrival
    // times are not a reliable packet timeline. Anchor the track once, then
    // advance it by each Opus packet's decoded sample count to keep audio DTS
    // strictly ordered for the muxer.
    if (!m_AudioClockStarted) {
        packet.pts = presentationTimeUs;
        m_AudioClockStarted = true;
    }
    else {
        packet.pts = m_NextAudioPtsUs;
    }
    m_NextAudioPtsUs = packet.pts + packet.duration;
    m_QueuedBytes += packet.data.size();
    m_Queue.push_back(std::move(packet));
    m_Condition.notify_one();
}

void StreamRecorder::setError(const QString& error)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Error = error;
    m_State.store(State::Failed);
}

void StreamRecorder::workerMain()
{
    AVFormatContext* format = nullptr;
    AVStream* audioStream = nullptr;
    AVStream* videoStream = nullptr;
    AVBSFContext* bsf = nullptr;
    bool headerWritten = false;
    std::int64_t recordingOriginUs = 0;
    std::int64_t presentationShiftUs = 0;
    std::uint64_t firstVideoOrder = 0;
    std::size_t prerollBytes = 0;
    std::deque<MuxPacket> pendingVideo;
    std::deque<MuxPacket> pendingAudio;
    auto startTime = std::chrono::steady_clock::now();

    auto fail = [&](const QString& message) {
        setError(message);
    };
    auto cleanup = [&]() {
        if (bsf) av_bsf_free(&bsf);
        if (format) {
            if (format->pb) avio_closep(&format->pb);
            avformat_free_context(format);
        }
        if (!headerWritten) QFile::remove(m_OutputPath);
    };
    auto writePacket = [&](const MuxPacket& input) -> bool {
        AVStream* stream = input.video ? videoStream : audioStream;
        if (!stream) return false;
        AVPacket* packet = av_packet_alloc();
        if (!packet) return false;
        int result = av_new_packet(packet, static_cast<int>(input.data.size()));
        if (result < 0) {
            av_packet_free(&packet);
            return false;
        }
        std::memcpy(packet->data, input.data.data(), input.data.size());
        packet->stream_index = stream->index;
        packet->pts = input.pts - recordingOriginUs + presentationShiftUs;
        packet->dts = input.video ?
                static_cast<std::int64_t>(input.videoOrder - firstVideoOrder) *
                std::max<std::int64_t>(1, 1000000 / std::max(1, m_FrameRate)) : packet->pts;
        packet->duration = input.duration;
        if (input.video && input.keyFrame) packet->flags |= AV_PKT_FLAG_KEY;
        const AVRational sourceTimeBase{1, 1000000};
        av_packet_rescale_ts(packet, sourceTimeBase, stream->time_base);
        result = av_interleaved_write_frame(format, packet);
        av_packet_free(&packet);
        return result >= 0;
    };

    AVCodecID codecId = codecIdForVideoFormat(m_VideoFormat);
    int result = avformat_alloc_output_context2(&format, nullptr, "matroska", m_OutputPath.toUtf8().constData());
    if (result < 0 || !format) {
        fail(QStringLiteral("Could not create the Matroska recording: %1").arg(ffmpegErrorString(result)));
        cleanup();
        return;
    }
    if (!(format->oformat->flags & AVFMT_NOFILE)) {
        result = avio_open(&format->pb, m_OutputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (result < 0) {
            fail(QStringLiteral("Could not open the recording file: %1").arg(ffmpegErrorString(result)));
            cleanup();
            return;
        }
    }

    audioStream = avformat_new_stream(format, nullptr);
    if (!audioStream) {
        fail(QStringLiteral("Could not create the recording audio stream."));
        cleanup();
        return;
    }
    audioStream->time_base = AVRational{1, m_AudioConfig.sampleRate};
    AVCodecParameters* audio = audioStream->codecpar;
    audio->codec_type = AVMEDIA_TYPE_AUDIO;
    audio->codec_id = AV_CODEC_ID_OPUS;
    audio->sample_rate = m_AudioConfig.sampleRate;
    av_channel_layout_default(&audio->ch_layout, m_AudioConfig.channelCount);

    // Matroska stores Opus configuration in the standard OpusHead packet.
    const int channels = m_AudioConfig.channelCount;
    const bool mapped = channels > 2;
    const int opusHeadSize = mapped ? 21 + channels : 19;
    audio->extradata = static_cast<std::uint8_t*>(av_mallocz(opusHeadSize + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!audio->extradata) {
        fail(QStringLiteral("Could not allocate Opus recording metadata."));
        cleanup();
        return;
    }
    audio->extradata_size = opusHeadSize;
    std::memcpy(audio->extradata, "OpusHead", 8);
    audio->extradata[8] = 1;
    audio->extradata[9] = static_cast<std::uint8_t>(channels);
    AV_WL16(audio->extradata + 10, 0); // Opus pre-skip
    AV_WL32(audio->extradata + 12, m_AudioConfig.sampleRate);
    AV_WL16(audio->extradata + 16, 0); // output gain
    audio->extradata[18] = mapped ? 1 : 0;
    if (mapped) {
        audio->extradata[19] = static_cast<std::uint8_t>(m_AudioConfig.streams);
        audio->extradata[20] = static_cast<std::uint8_t>(m_AudioConfig.coupledStreams);
        for (int i = 0; i < channels; ++i) audio->extradata[21 + i] = m_AudioConfig.mapping[i];
    }

    const AVBitStreamFilter* filter = av_bsf_get_by_name("extract_extradata");
    if (!filter || av_bsf_alloc(filter, &bsf) < 0 || !bsf) {
        fail(QStringLiteral("This FFmpeg build cannot prepare the video stream for recording."));
        cleanup();
        return;
    }
    bsf->par_in->codec_type = AVMEDIA_TYPE_VIDEO;
    bsf->par_in->codec_id = codecId;
    bsf->par_in->width = m_Width;
    bsf->par_in->height = m_Height;
    bsf->par_in->format = pixelFormatForVideoFormat(m_VideoFormat);
    bsf->time_base_in = AVRational{1, 1000000};
    result = av_bsf_init(bsf);
    if (result < 0) {
        fail(QStringLiteral("Could not initialize FFmpeg's video stream parser: %1").arg(ffmpegErrorString(result)));
        cleanup();
        return;
    }

    auto startMuxing = [&]() -> bool {
        if (bsf->par_out->extradata_size <= 0) {
            return false;
        }
        auto firstKeyFrame = std::find_if(pendingVideo.begin(), pendingVideo.end(),
                                          [](const MuxPacket& packet) { return packet.keyFrame; });
        if (firstKeyFrame == pendingVideo.end()) return false;
        const int reorderWindow = std::min(12, std::max(1, m_FrameRate / 5));
        if (std::distance(firstKeyFrame, pendingVideo.end()) < reorderWindow) return false;
        recordingOriginUs = firstKeyFrame->pts;
        firstVideoOrder = firstKeyFrame->videoOrder;
        const std::int64_t frameDurationUs = std::max<std::int64_t>(1, 1000000 / std::max(1, m_FrameRate));
        for (auto it = firstKeyFrame; it != pendingVideo.end(); ++it) {
            const std::int64_t decodeTimeUs = static_cast<std::int64_t>(it->videoOrder - firstVideoOrder) * frameDurationUs;
            presentationShiftUs = std::max(presentationShiftUs, decodeTimeUs - (it->pts - recordingOriginUs));
        }
        presentationShiftUs = std::max<std::int64_t>(0, presentationShiftUs);
        pendingVideo.erase(pendingVideo.begin(), firstKeyFrame);
        pendingAudio.erase(std::remove_if(pendingAudio.begin(), pendingAudio.end(),
                                         [&](const MuxPacket& packet) { return packet.pts < recordingOriginUs; }),
                           pendingAudio.end());
        videoStream = avformat_new_stream(format, nullptr);
        if (!videoStream) return false;
        result = avcodec_parameters_copy(videoStream->codecpar, bsf->par_out);
        if (result < 0) return false;
        videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        videoStream->codecpar->codec_id = codecId;
        videoStream->codecpar->width = m_Width;
        videoStream->codecpar->height = m_Height;
        videoStream->codecpar->format = pixelFormatForVideoFormat(m_VideoFormat);
        videoStream->time_base = AVRational{1, 1000000};
        result = avformat_write_header(format, nullptr);
        if (result < 0) {
            fail(QStringLiteral("FFmpeg could not start the recording: %1").arg(ffmpegErrorString(result)));
            return false;
        }
        headerWritten = true;
        if (m_State.load() == State::Starting) m_State.store(State::Recording);
        while (!pendingAudio.empty()) {
            if (!writePacket(pendingAudio.front())) return false;
            pendingAudio.pop_front();
        }
        while (!pendingVideo.empty()) {
            if (!writePacket(pendingVideo.front())) return false;
            pendingVideo.pop_front();
        }
        return true;
    };

    for (;;) {
        Packet input;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Condition.wait_for(lock, std::chrono::milliseconds(100), [&]() {
                return !m_Queue.empty() || m_StopRequested;
            });
            if (m_Queue.empty()) {
                if (m_StopRequested) break;
                if (!headerWritten && std::chrono::steady_clock::now() - startTime >
                        std::chrono::seconds(STARTUP_TIMEOUT_SECONDS)) {
                    lock.unlock();
                    fail(QStringLiteral("Recording stopped: no video keyframe arrived while the recorder prepared the file."));
                    break;
                }
                continue;
            }
            input = std::move(m_Queue.front());
            m_QueuedBytes -= input.data.size();
            m_Queue.pop_front();
        }

        if (!input.video) {
            if (headerWritten) {
                MuxPacket mux;
                mux.pts = input.pts;
                mux.duration = input.duration;
                mux.data = std::move(input.data);
                if (!writePacket(mux)) {
                    fail(QStringLiteral("Writing audio to the recording failed."));
                    break;
                }
            }
            else {
                prerollBytes += input.data.size();
                if (prerollBytes > MAX_PREROLL_BYTES) {
                    fail(QStringLiteral("Recording stopped because it could not find a video keyframe to begin the file."));
                    break;
                }
                MuxPacket mux;
                mux.pts = input.pts;
                mux.duration = input.duration;
                mux.data = std::move(input.data);
                pendingAudio.push_back(std::move(mux));
            }
            continue;
        }

        AVPacket* videoPacket = av_packet_alloc();
        if (!videoPacket) {
            fail(QStringLiteral("Could not allocate a video packet for recording."));
            break;
        }
        result = av_new_packet(videoPacket, static_cast<int>(input.data.size()));
        if (result < 0) {
            av_packet_free(&videoPacket);
            fail(QStringLiteral("Could not copy a video packet for recording: %1").arg(ffmpegErrorString(result)));
            break;
        }
        std::memcpy(videoPacket->data, input.data.data(), input.data.size());
        videoPacket->pts = input.pts;
        videoPacket->dts = input.pts;
        videoPacket->duration = input.duration;
        if (input.keyFrame) videoPacket->flags |= AV_PKT_FLAG_KEY;
        result = av_bsf_send_packet(bsf, videoPacket);
        av_packet_free(&videoPacket);
        if (result < 0) {
            fail(QStringLiteral("FFmpeg could not parse a video packet: %1").arg(ffmpegErrorString(result)));
            break;
        }

        for (;;) {
            AVPacket* filtered = av_packet_alloc();
            if (!filtered) {
                fail(QStringLiteral("Could not allocate a parsed video packet for recording."));
                break;
            }
            result = av_bsf_receive_packet(bsf, filtered);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                av_packet_free(&filtered);
                break;
            }
            if (result < 0) {
                av_packet_free(&filtered);
                fail(QStringLiteral("FFmpeg could not extract video metadata: %1").arg(ffmpegErrorString(result)));
                break;
            }

            std::size_t extraSize = 0;
            std::uint8_t* newExtra = av_packet_get_side_data(filtered, AV_PKT_DATA_NEW_EXTRADATA, &extraSize);
            if (newExtra && extraSize > 0) {
                av_freep(&bsf->par_out->extradata);
                bsf->par_out->extradata = static_cast<std::uint8_t*>(av_mallocz(extraSize + AV_INPUT_BUFFER_PADDING_SIZE));
                if (bsf->par_out->extradata) {
                    std::memcpy(bsf->par_out->extradata, newExtra, extraSize);
                    bsf->par_out->extradata_size = static_cast<int>(extraSize);
                }
            }

            MuxPacket mux;
            mux.video = true;
            mux.keyFrame = (filtered->flags & AV_PKT_FLAG_KEY) != 0 || input.keyFrame;
            mux.pts = filtered->pts == AV_NOPTS_VALUE ? input.pts : filtered->pts;
            mux.duration = filtered->duration > 0 ? filtered->duration : input.duration;
            mux.videoOrder = input.videoOrder;
            mux.data.assign(filtered->data, filtered->data + filtered->size);
            av_packet_free(&filtered);

            if (!headerWritten) {
                prerollBytes += mux.data.size();
                if (prerollBytes > MAX_PREROLL_BYTES) {
                    fail(QStringLiteral("Recording stopped because FFmpeg could not extract the video configuration."));
                    break;
                }
                pendingVideo.push_back(std::move(mux));
                const auto firstKeyFrame = std::find_if(pendingVideo.cbegin(), pendingVideo.cend(),
                                                        [](const MuxPacket& packet) { return packet.keyFrame; });
                const int reorderWindow = std::min(12, std::max(1, m_FrameRate / 5));
                const bool hasKeyFrameAndPreroll = firstKeyFrame != pendingVideo.cend() &&
                        std::distance(firstKeyFrame, pendingVideo.cend()) >= reorderWindow;
                if (bsf->par_out->extradata_size > 0 && hasKeyFrameAndPreroll) {
                    if (!startMuxing()) {
                        if (m_State.load() != State::Failed)
                            fail(QStringLiteral("FFmpeg could not finalize the recording streams."));
                        break;
                    }
                }
            }
            else if (!writePacket(mux)) {
                fail(QStringLiteral("Writing video to the recording failed."));
                break;
            }
        }
        if (m_State.load() == State::Failed) break;
    }

    if (headerWritten) {
        result = av_write_trailer(format);
        if (result < 0) fail(QStringLiteral("The recording file could not be finalized: %1").arg(ffmpegErrorString(result)));
    }
    else if (m_State.load() != State::Failed) {
        fail(QStringLiteral("Recording stopped before the first video keyframe was ready."));
    }
    cleanup();

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_State.load() != State::Failed) m_State.store(State::Idle);
}
#else

StreamRecorder::StreamRecorder() = default;
StreamRecorder::~StreamRecorder() = default;
void StreamRecorder::configureVideo(int, int, int, int) {}
void StreamRecorder::configureAudio(const OPUS_MULTISTREAM_CONFIGURATION&) {}
bool StreamRecorder::start(const QString&, std::int64_t)
{
    m_Error = QStringLiteral("Stream recording requires an FFmpeg-enabled build.");
    m_State.store(State::Failed);
    return false;
}
void StreamRecorder::requestStop() {}
void StreamRecorder::stop() {}
bool StreamRecorder::active() const { return false; }
QString StreamRecorder::outputPath() const { return {}; }
QString StreamRecorder::statusText() const { return m_Error; }
void StreamRecorder::submitVideo(const std::uint8_t*, int, std::int64_t, bool) {}
void StreamRecorder::submitVideoDecodeUnit(PDECODE_UNIT) {}
void StreamRecorder::submitAudio(const std::uint8_t*, int, std::int64_t) {}
void StreamRecorder::workerMain() {}
void StreamRecorder::setError(const QString&) {}

#endif
