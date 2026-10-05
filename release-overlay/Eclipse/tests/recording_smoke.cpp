#include "../app/streaming/recording/streamrecorder.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cmath>

static void check(bool value, const char* message)
{
    if (!value) qFatal("FAIL: %s", message);
}

static AVFormatContext* openInput(const QString& path)
{
    AVFormatContext* format = nullptr;
    int result = avformat_open_input(&format, path.toUtf8().constData(), nullptr, nullptr);
    check(result >= 0, "input media opens");
    result = avformat_find_stream_info(format, nullptr);
    check(result >= 0, "input stream metadata parses");
    return format;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    check(argc == 5, "usage: recording-smoke video.bitstream audio.ogg output.mkv h264|hevc|av1");
    AVFormatContext* video = openInput(QString::fromLocal8Bit(argv[1]));
    AVFormatContext* audio = openInput(QString::fromLocal8Bit(argv[2]));
    const int videoIndex = av_find_best_stream(video, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audioIndex = av_find_best_stream(audio, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const QString codecName = QString::fromLocal8Bit(argv[4]).toLower();
    AVCodecID expectedCodec = AV_CODEC_ID_NONE;
    int moonlightFormat = 0;
    if (codecName == "h264") {
        expectedCodec = AV_CODEC_ID_H264;
        moonlightFormat = VIDEO_FORMAT_H264;
    }
    else if (codecName == "hevc") {
        expectedCodec = AV_CODEC_ID_HEVC;
        moonlightFormat = VIDEO_FORMAT_H265;
    }
    else if (codecName == "av1") {
        expectedCodec = AV_CODEC_ID_AV1;
        moonlightFormat = VIDEO_FORMAT_AV1_MAIN8;
    }
    check(expectedCodec != AV_CODEC_ID_NONE, "codec argument is supported");
    check(videoIndex >= 0 && video->streams[videoIndex]->codecpar->codec_id == expectedCodec,
          "test video uses the requested codec");
    check(audioIndex >= 0 && audio->streams[audioIndex]->codecpar->codec_id == AV_CODEC_ID_OPUS,
          "test audio is Opus");

    StreamRecorder recorder;
    // Raw H.264/HEVC fixtures carry no packet timestamps, so their test clock
    // is the explicit 30 fps lavfi input rate. The OBU demuxer supplies AV1
    // timestamps at its 25 fps default; use that same clock for DTS generation.
    const AVRational videoRate = video->streams[videoIndex]->avg_frame_rate;
    const int frameRate = codecName == "av1" && videoRate.num > 0 && videoRate.den > 0 ?
            static_cast<int>(std::lround(av_q2d(videoRate))) : 30;
    recorder.configureVideo(moonlightFormat, video->streams[videoIndex]->codecpar->width,
                            video->streams[videoIndex]->codecpar->height, std::max(1, frameRate));
    OPUS_MULTISTREAM_CONFIGURATION config = {};
    config.sampleRate = 48000;
    config.samplesPerFrame = 960;
    config.channelCount = 2;
    config.streams = 1;
    config.coupledStreams = 1;
    config.mapping[0] = 0;
    config.mapping[1] = 1;
    recorder.configureAudio(config);
    check(recorder.start(QString::fromLocal8Bit(argv[3]), 0), "recorder starts");

    AVPacket* packet = av_packet_alloc();
    check(packet != nullptr, "packet allocates");
    int videoPackets = 0;
    while (av_read_frame(video, packet) >= 0) {
        if (packet->stream_index == videoIndex) {
            const std::int64_t pts = packet->pts == AV_NOPTS_VALUE ? packet->dts : packet->pts;
            std::int64_t ptsUs = pts == AV_NOPTS_VALUE ? videoPackets * 1000000LL / frameRate :
                    av_rescale_q(pts, video->streams[videoIndex]->time_base, AVRational{1, 1000000});
            if (pts == AV_NOPTS_VALUE && codecName != "h264") {
                static const int decodeToPresentationOrder[] = {0, 3, 1, 2};
                const int presentationFrame = (videoPackets / 4) * 4 +
                        decodeToPresentationOrder[videoPackets % 4];
                ptsUs = presentationFrame * 1000000LL / 30;
            }
            recorder.submitVideo(packet->data, packet->size, ptsUs,
                                 (packet->flags & AV_PKT_FLAG_KEY) != 0);
            ++videoPackets;
        }
        av_packet_unref(packet);
    }
    int audioPackets = 0;
    while (av_read_frame(audio, packet) >= 0) {
        if (packet->stream_index == audioIndex) {
            const std::int64_t pts = packet->pts == AV_NOPTS_VALUE ? packet->dts : packet->pts;
            std::int64_t ptsUs = av_rescale_q(pts, audio->streams[audioIndex]->time_base, AVRational{1, 1000000});
            // Audio callbacks can arrive in bursts. Simulate arrival-time
            // jitter while keeping the first packet as the stream anchor.
            if (audioPackets > 0 && audioPackets % 2 == 0) ptsUs -= 50000;
            recorder.submitAudio(packet->data, packet->size, ptsUs);
            ++audioPackets;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&video);
    avformat_close_input(&audio);
    check(videoPackets >= 30 && audioPackets >= 30, "test media supplies both tracks");

    recorder.stop();
    check(recorder.state() == StreamRecorder::State::Idle, qPrintable(recorder.statusText()));
    check(QFileInfo::exists(QString::fromLocal8Bit(argv[3])), "output file exists");

    AVFormatContext* output = openInput(QString::fromLocal8Bit(argv[3]));
    int outputVideo = av_find_best_stream(output, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int outputAudio = av_find_best_stream(output, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    check(outputVideo >= 0 && outputAudio >= 0, "output contains video and audio");
    check(output->streams[outputVideo]->codecpar->codec_id == expectedCodec,
          "recorded video retains its incoming codec");
    check(output->streams[outputAudio]->codecpar->codec_id == AV_CODEC_ID_OPUS,
          "recorded audio remains Opus");
    int outputVideoPackets = 0;
    int outputAudioPackets = 0;
    std::int64_t lastAudioDts = AV_NOPTS_VALUE;
    packet = av_packet_alloc();
    check(packet != nullptr, "output packet allocates");
    while (av_read_frame(output, packet) >= 0) {
        if (packet->stream_index == outputVideo) ++outputVideoPackets;
        if (packet->stream_index == outputAudio) {
            if (lastAudioDts != AV_NOPTS_VALUE) {
                check(packet->dts > lastAudioDts, "audio packet timestamps remain strictly increasing under callback jitter");
            }
            lastAudioDts = packet->dts;
            ++outputAudioPackets;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&output);
    check(outputVideoPackets >= 30 && outputAudioPackets >= 30, "output packets from both tracks are readable");
    qInfo() << "PASS:" << codecName << "and Opus packets mux, close, and demux from" << argv[3];
    return 0;
}
