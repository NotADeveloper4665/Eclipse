#include "mediapassthrough.h"
#include "devicepassthrough.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#ifdef Q_OS_LINUX
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
QString quote(QString text) { text.replace('\'', "'\\''"); return "'" + text + "'"; }
const char microphoneReceiver[] = R"SH(
set -eu
sink="eclipse_mic_$$"
null_id=""
source_id=""
cleanup() {
    [ -z "$source_id" ] || pactl unload-module "$source_id" >&2 || true
    [ -z "$null_id" ] || pactl unload-module "$null_id" >&2 || true
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
command -v ffmpeg >/dev/null
command -v pactl >/dev/null
null_id=$(pactl load-module module-null-sink sink_name="$sink" sink_properties=device.description=Eclipse_Microphone_Input)
source_id=$(pactl load-module module-remap-source master="$sink.monitor" source_name="${sink}_source" source_properties=device.description=Eclipse_Microphone)
printf 'ECLIPSE_MEDIA_READY\n' >&2
ffmpeg -nostdin -hide_banner -loglevel warning -fflags nobuffer -flags low_delay -probesize 32768 -analyzeduration 0 -f ogg -i pipe:0 -ac 1 -ar 48000 -f pulse -device "$sink" -buffer_duration 20 Eclipse_Microphone
)SH";
const char cameraReceiver[] = R"SH(
set -eu
output="$1"
name=$(cat "/sys/class/video4linux/${output##*/}/name")
printf '%s' "$name" | grep -qi 'eclipse\|loopback' || { echo 'Choose an Eclipse/v4l2loopback virtual camera on the host' >&2; exit 1; }
[ -w "$output" ] || { echo 'Host virtual camera is not writable by the SSH user' >&2; exit 1; }
command -v ffmpeg >/dev/null
printf 'ECLIPSE_MEDIA_READY\n' >&2
exec ffmpeg -nostdin -hide_banner -loglevel warning -fflags nobuffer -flags low_delay -probesize 32768 -analyzeduration 0 -f matroska -i pipe:0 -an -vf format=yuv420p -f v4l2 "$output"
)SH";
}

MediaPassthrough::MediaPassthrough(QObject* parent) : QObject(parent)
{
    m_Capture.setStandardOutputProcess(&m_Receiver);
    m_Timeout.setSingleShot(true);
    connect(&m_Timeout, &QTimer::timeout, this, [this] { fail(tr("Forwarding timed out. Check SSH and the host's audio/camera setup.")); });
    connect(&m_Discovery, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
        m_Inputs.clear();
        m_Inputs.append(QVariantMap{{"label", tr("Default microphone")}, {"id", "default"}});
        if (code == 0) {
            const QJsonArray sources = QJsonDocument::fromJson(m_Discovery.readAllStandardOutput()).array();
            for (const QJsonValue& source : sources) {
                const QJsonObject object = source.toObject();
                const QString name = object.value("name").toString();
                if (name.isEmpty() || name.endsWith(".monitor") || name.startsWith("eclipse_mic_")) continue;
                m_Inputs.append(QVariantMap{{"label", object.value("description").toString(name)}, {"id", name}});
            }
        }
        emit inputsChanged();
    });
    connect(&m_Receiver, &QProcess::readyReadStandardError, this, [this] {
        const QByteArray bytes = m_Receiver.readAllStandardError();
        m_ReceiverOutput += bytes;
        m_Log = (m_Log + QString::fromUtf8(bytes)).right(4096);
        if (!m_Ready && m_ReceiverOutput.contains("ECLIPSE_MEDIA_READY\n") && !m_Stopping) {
            m_Ready = true;
            QStringList args = {"-nostdin", "-hide_banner", "-loglevel", "warning"};
            if (m_Kind == "microphone") {
                args << "-f" << "pulse" << "-i" << m_Input << "-ac" << "1" << "-ar" << "48000"
                     << "-c:a" << "libopus" << "-b:a" << "64k" << "-application" << "lowdelay"
                     << "-frame_duration" << "10" << "-flush_packets" << "1" << "-page_duration" << "10000" << "-f" << "ogg" << "pipe:1";
            }
            else {
                args << "-f" << "v4l2" << "-framerate" << "30" << "-video_size" << "640x480" << "-i" << m_Input
                     << "-an" << "-c:v" << "mjpeg" << "-q:v" << "5" << "-flush_packets" << "1" << "-f" << "matroska" << "-cluster_time_limit" << "1" << "pipe:1";
            }
            m_Capture.start("ffmpeg", args);
        }
        if (m_ReceiverOutput.size() > 8192) m_ReceiverOutput = m_ReceiverOutput.right(4096);
    });
    connect(&m_Capture, &QProcess::readyReadStandardError, this, [this] {
        m_Log = (m_Log + QString::fromUtf8(m_Capture.readAllStandardError())).right(4096);
    });
    connect(&m_Capture, &QProcess::started, this, [this] {
        m_Timeout.stop(); m_Busy = false; m_Sharing = true;
        setStatus(m_Kind == "microphone" ? tr("Microphone forwarding started. Select Eclipse Microphone in the host application.") : tr("Webcam forwarding started at 640×480, 30 FPS. Select Eclipse Webcam on the host."));
    });
    connect(&m_Capture, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        if (!m_Stopping) fail(tr("Capture stopped: %1").arg(m_Log.trimmed()));
    });
    connect(&m_Receiver, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        if (!m_Stopping) fail(tr("Host forwarding stopped: %1").arg(m_Log.trimmed()));
        m_Busy = false; m_Sharing = false; emit stateChanged();
    });
    for (QProcess* process : {&m_Capture, &m_Receiver}) {
        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) fail(tr("Could not start FFmpeg or SSH. Install the forwarding dependencies."));
        });
    }
}

MediaPassthrough::~MediaPassthrough()
{
    m_Stopping = true;
    disconnect(&m_Capture, nullptr, this, nullptr);
    disconnect(&m_Receiver, nullptr, this, nullptr);
    m_Capture.terminate();
    if (!m_Capture.waitForFinished(1000)) { m_Capture.kill(); m_Capture.waitForFinished(1000); }
    if (!m_Receiver.waitForFinished(2000)) { m_Receiver.terminate(); m_Receiver.waitForFinished(1000); }
}

bool MediaPassthrough::supported() const
{
#ifdef Q_OS_LINUX
    return true;
#else
    return false;
#endif
}

void MediaPassthrough::setStatus(const QString& status) { m_Status = status; emit stateChanged(); }
void MediaPassthrough::fail(const QString& status) { stop(); setStatus(status); }

void MediaPassthrough::refresh()
{
    if (m_Busy || m_Sharing) return;
    m_Inputs.clear();
    if (!supported()) { setStatus(tr("Native forwarding currently supports Linux clients and Linux hosts.")); emit inputsChanged(); return; }
    if (m_Kind == "microphone") {
        m_Inputs.append(QVariantMap{{"label", tr("Default microphone")}, {"id", "default"}});
        emit inputsChanged();
        if (!QStandardPaths::findExecutable("pactl").isEmpty()) m_Discovery.start("pactl", {"-f", "json", "list", "sources"});
    }
    else if (m_Kind == "camera") {
#ifdef Q_OS_LINUX
        QDir root("/sys/class/video4linux");
        for (const QString& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString path = "/dev/" + name;
            const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            v4l2_capability caps = {};
            const bool queried = ::ioctl(fd, VIDIOC_QUERYCAP, &caps) == 0;
            ::close(fd);
            const unsigned flags = caps.capabilities & V4L2_CAP_DEVICE_CAPS ? caps.device_caps : caps.capabilities;
            if (queried && (flags & V4L2_CAP_VIDEO_CAPTURE)) {
                m_Inputs.append(QVariantMap{{"label", QString::fromLocal8Bit(reinterpret_cast<const char*>(caps.card)) + " (" + path + ")"}, {"id", path}});
            }
        }
#endif
        emit inputsChanged();
    }
    setStatus(m_Inputs.isEmpty() ? tr("No accessible capture device found.") : tr("Choose a device and click Start forwarding."));
}

void MediaPassthrough::start(const QString& target, const QString& input, const QString& output)
{
    if (m_Busy || m_Sharing || m_Capture.state() != QProcess::NotRunning || m_Receiver.state() != QProcess::NotRunning) return;
    if (!supported() || !DevicePassthrough::validTarget(target) || (m_Kind != "camera" && m_Kind != "microphone")) { setStatus(tr("Enter a valid Linux host SSH alias or user@host.")); return; }
    bool present = false;
    for (const QVariant& item : m_Inputs) present |= item.toMap().value("id").toString() == input;
    if (!present) { setStatus(tr("Refresh and choose an available capture device.")); return; }
    if (m_Kind == "camera" && !QRegularExpression("\\A/dev/video[0-9]+\\z").match(output).hasMatch()) { setStatus(tr("Host camera must be a /dev/video number backed by v4l2loopback.")); return; }
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty() || QStandardPaths::findExecutable("ssh").isEmpty()) { setStatus(tr("Install FFmpeg and OpenSSH client first.")); return; }
    m_Input = input; m_Ready = false; m_Stopping = false; m_Busy = true; m_Log.clear(); m_ReceiverOutput.clear();
    setStatus(tr("Connecting and preparing the host capture device..."));
    QString command = "sh -c " + quote(QString::fromLatin1(m_Kind == "microphone" ? microphoneReceiver : cameraReceiver)) + " eclipse-media";
    if (m_Kind == "camera") command += " " + quote(output);
    m_Receiver.start("ssh", {"-T", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes", "-o", "ConnectTimeout=10",
                            "-o", "ServerAliveInterval=5", "-o", "ServerAliveCountMax=3", "--", target, command});
    m_Timeout.start(20000);
}

void MediaPassthrough::stop()
{
    m_Timeout.stop(); m_Stopping = true; m_Sharing = false;
    // Terminate capture; pipe EOF lets the host remove its virtual audio modules.
    if (m_Capture.state() != QProcess::NotRunning) {
        m_Capture.terminate();
        QTimer::singleShot(1000, &m_Capture, [this] { if (m_Stopping && m_Capture.state() != QProcess::NotRunning) m_Capture.kill(); });
    }
    if (!m_Ready && m_Receiver.state() != QProcess::NotRunning) m_Receiver.terminate();
    m_Busy = m_Capture.state() != QProcess::NotRunning || m_Receiver.state() != QProcess::NotRunning;
    QTimer::singleShot(3000, &m_Receiver, [this] { if (m_Stopping && m_Receiver.state() != QProcess::NotRunning) m_Receiver.terminate(); });
    setStatus(tr("Forwarding stopped."));
}
