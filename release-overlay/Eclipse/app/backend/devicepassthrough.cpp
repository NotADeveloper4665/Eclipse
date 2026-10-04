#include "devicepassthrough.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QRandomGenerator>

namespace {
QString readAttribute(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QString shellQuote(QString text)
{
    text.replace('\'', "'\\''");
    return "'" + text + "'";
}

// Runs on a Linux receiver over SSH. Only detaches the port created by this share.
// stdin EOF releases it; keepalives also stop an abandoned SSH connection.
const char receiverScript[] = R"SH(
set -eu
bus="$1"
tcp_port="$2"
port=""
cleanup() {
    if [ -n "$port" ]; then
        sudo -n usbip detach --port="$port" >&2 || true
        port=""
    fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
command -v usbip >/dev/null || { echo 'Install USB/IP tools on the Linux host' >&2; exit 1; }
[ -d /sys/module/vhci_hcd ] || { echo 'Load vhci-hcd on the host first' >&2; exit 1; }
sudo -n usbip --tcp-port "$tcp_port" attach --remote=127.0.0.1 --busid="$bus" >&2
port=$(usbip port | awk -v bus="$bus" -v tcp_port="$tcp_port" '
/^Port [0-9]+:/ { p=$2; sub(/:$/, "", p) }
index($0, "usbip://127.0.0.1:" tcp_port "/" bus) && $0 ~ ("/" bus "$") { print p; exit }
')
[ -n "$port" ] || { echo 'Attached device but could not identify its port; inspect usbip port on host' >&2; exit 1; }
printf 'ECLIPSE_USBIP_READY %s\n' "$port"
cat >/dev/null
)SH";
}

DevicePassthrough::DevicePassthrough(QObject* parent, const QString& usbRoot)
    : QObject(parent), m_UsbRoot(usbRoot), m_Port(QRandomGenerator::global()->bounded(20000, 60000))
{
    auto unplugCheck = new QTimer(this);
    unplugCheck->setInterval(2000);
    connect(unplugCheck, &QTimer::timeout, this, [this] {
        if (m_Exported && !m_Stopping && !QFile::exists(m_UsbRoot + "/" + m_BusId + "/idVendor")) {
            stop();
            m_Failure = tr("The shared USB device was unplugged.");
        }
    });
    unplugCheck->start();
    m_Timeout.setSingleShot(true);
    connect(&m_Timeout, &QTimer::timeout, this, [this] {
        m_Failure = tr("Device forwarding timed out. Check SSH, the host USB/IP setup, and local usbipd.");
        // Don't interrupt a PolicyKit prompt: the bind may already have completed.
        if (m_Tunnel.state() != QProcess::NotRunning) m_Tunnel.kill();
        else if (!m_Exported) { m_Busy = false; setStatus(m_Failure); }
    });
    connect(&m_Local, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
        const bool ok = code == 0 && exitStatus == QProcess::NormalExit;
        if (m_Stopping) {
            finishRestore(ok);
        }
        else if (ok) {
            m_Exported = true;
            openTunnel();
        }
        else {
            m_Busy = false;
            setStatus(tr("Could not export USB device: %1").arg(QString::fromUtf8(m_Local.readAllStandardError()).trimmed()));
        }
    });
    connect(&m_Local, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        if (m_Stopping) finishRestore(false);
        else { m_Busy = false; setStatus(tr("Could not start the local USB/IP permission helper.")); }
    });
    connect(&m_Tunnel, &QProcess::readyReadStandardOutput, this, [this] {
        m_Output += m_Tunnel.readAllStandardOutput();
        const QRegularExpression ready("(?:^|\\n)ECLIPSE_USBIP_READY [0-9]+\\r?\\n");
        if (ready.match(QString::fromUtf8(m_Output)).hasMatch() && !m_Sharing && !m_Stopping) {
            m_Timeout.stop();
            m_Busy = false;
            m_Sharing = true;
            setStatus(tr("Sharing %1 with %2. Stop sharing to return it to this PC.").arg(m_BusId, m_Target));
        }
        if (m_Output.size() > 8192) m_Output = m_Output.right(4096);
    });
    connect(&m_Tunnel, &QProcess::readyReadStandardError, this, [this] {
        m_Failure += QString::fromUtf8(m_Tunnel.readAllStandardError());
        m_Failure = m_Failure.right(4096);
    });
    connect(&m_Tunnel, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
        m_Timeout.stop();
        if (!m_Stopping || code != 0) {
            if (m_Failure.isEmpty()) m_Failure = tr("SSH forwarding ended unexpectedly.");
        }
        restoreDevice();
    });
    connect(&m_Tunnel, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_Timeout.stop();
            m_Failure = tr("Could not start SSH.");
            restoreDevice();
        }
    });
    refresh();
}

DevicePassthrough::~DevicePassthrough()
{
    // Closing the SSH channel asks the receiver to detach, then return the local device.
    disconnect(&m_Local, nullptr, this, nullptr);
    disconnect(&m_Tunnel, nullptr, this, nullptr);
    if (m_Tunnel.state() != QProcess::NotRunning) {
        m_Tunnel.closeWriteChannel();
        if (!m_Tunnel.waitForFinished(3000)) { m_Tunnel.kill(); m_Tunnel.waitForFinished(1000); }
    }
    if (m_Local.state() != QProcess::NotRunning) {
        // Allow a pending bind to settle before attempting restoration.
        m_Local.waitForFinished(3000);
        if (m_Local.state() == QProcess::NotRunning && m_Local.exitCode() == 0 && !m_Stopping) m_Exported = true;
    }
    if (m_Exported && m_Local.state() == QProcess::NotRunning) {
        m_Local.start("pkexec", {m_Usbip, "unbind", "--busid=" + m_BusId});
        m_Local.waitForFinished(3000);
    }
}

bool DevicePassthrough::supported() const
{
#ifdef Q_OS_LINUX
    return true;
#else
    return false;
#endif
}

bool DevicePassthrough::validTarget(const QString& target)
{
    return !target.isEmpty() && !target.startsWith('-') && target.size() <= 255 &&
            QRegularExpression("\\A[A-Za-z0-9_.@:\\[\\]-]+\\z").match(target).hasMatch();
}

bool DevicePassthrough::validBusId(const QString& busId)
{
    return QRegularExpression("\\A[0-9]+-[0-9]+(?:\\.[0-9]+)*\\z").match(busId).hasMatch();
}

void DevicePassthrough::setStatus(const QString& text)
{
    m_Status = text;
    emit stateChanged();
}

void DevicePassthrough::refresh()
{
    m_Devices.clear();
    if (!supported()) {
        setStatus(tr("USB/IP forwarding currently supports Linux clients and Linux hosts."));
        emit devicesChanged();
        return;
    }
    QDir root(m_UsbRoot);
    for (const QString& bus : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!validBusId(bus)) continue;
        const QString path = root.filePath(bus);
        // Exclude hubs; interface classes identify composite cameras/headsets.
        if (readAttribute(path + "/bDeviceClass") == "09") continue;
        bool audio = false, camera = false;
        for (const QString& iface : root.entryList({bus + ":*"}, QDir::Dirs)) {
            const QString klass = readAttribute(root.filePath(iface) + "/bInterfaceClass");
            audio |= klass == "01";
            camera |= klass == "0e";
        }
        QString name = readAttribute(path + "/manufacturer") + " " + readAttribute(path + "/product");
        if (name.trimmed().isEmpty()) name = readAttribute(path + "/idVendor") + ":" + readAttribute(path + "/idProduct");
        QVariantMap device;
        device["busId"] = bus;
        device["label"] = name.trimmed() + " (" + bus + ")";
        device["audio"] = audio;
        device["camera"] = camera;
        m_Devices.append(device);
    }
    emit devicesChanged();
    if (!m_Busy && !m_Sharing && !m_Exported) {
        setStatus(tr("Select a USB microphone, webcam, or another device to share."));
    }
}

void DevicePassthrough::share(const QString& sshTarget, const QString& busId)
{
    if (m_Busy || m_Sharing || m_Exported) return;
    if (!supported() || !validTarget(sshTarget) || !validBusId(busId)) {
        setStatus(tr("Enter a Linux host SSH alias or user@host and select a local USB device."));
        return;
    }
    bool present = false;
    for (const QVariant& device : m_Devices) present |= device.toMap().value("busId").toString() == busId;
    if (!present || !QFile::exists(m_UsbRoot + "/" + busId + "/idVendor")) {
        setStatus(tr("The selected device is no longer connected. Refresh the list."));
        return;
    }
    m_Usbip = QStandardPaths::findExecutable("usbip");
    if (m_Usbip.isEmpty() || QStandardPaths::findExecutable("pkexec").isEmpty() || QStandardPaths::findExecutable("ssh").isEmpty()) {
        setStatus(tr("Install usbip, OpenSSH client, and PolicyKit on this PC first."));
        return;
    }
    // Validate usbipd before taking the device away from its local driver.
    QTcpSocket daemon;
    daemon.connectToHost(QStringLiteral("127.0.0.1"), 3240);
    if (!daemon.waitForConnected(300)) {
        setStatus(tr("Start local usbipd and load usbip-host before sharing (see device forwarding setup)."));
        return;
    }
    m_Target = sshTarget;
    m_BusId = busId;
    m_Failure.clear();
    m_Output.clear();
    m_Busy = true;
    m_Stopping = false;
    setStatus(tr("Exporting %1; approve the local USB/IP permission prompt.").arg(busId));
    m_Local.start("pkexec", {m_Usbip, "bind", "--busid=" + busId});
}

void DevicePassthrough::openTunnel()
{
    setStatus(tr("Connecting to %1 and attaching the device...").arg(m_Target));
    m_Timeout.start(20000);
    const QString remote = "sh -c " + shellQuote(QString::fromLatin1(receiverScript)) + " eclipse-usbip " + shellQuote(m_BusId) + " " + QString::number(m_Port);
    m_Tunnel.start("ssh", {"-T", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes",
                         "-o", "ExitOnForwardFailure=yes", "-o", "ConnectTimeout=10",
                         "-o", "ServerAliveInterval=5", "-o", "ServerAliveCountMax=3",
                         "-R", QString("127.0.0.1:%1:127.0.0.1:3240").arg(m_Port), "--", m_Target, remote});
}

void DevicePassthrough::stop()
{
    // Don't cancel while the export permission dialog is outstanding.
    if (!m_Exported || m_Stopping) return;
    m_Stopping = true;
    m_Busy = true;
    m_Failure.clear();
    setStatus(tr("Stopping forwarding and returning %1 to this PC...").arg(m_BusId));
    if (m_Tunnel.state() != QProcess::NotRunning) {
        m_Tunnel.closeWriteChannel();
        m_Timeout.start(10000);
    }
    else restoreDevice();
}

void DevicePassthrough::restoreDevice()
{
    m_Sharing = false;
    m_Busy = true;
    m_Stopping = true;
    emit stateChanged();
    if (!m_Exported || !QFile::exists(m_UsbRoot + "/" + m_BusId + "/idVendor")) { finishRestore(true); return; }
    m_Local.start("pkexec", {m_Usbip, "unbind", "--busid=" + m_BusId});
}

void DevicePassthrough::finishRestore(bool success)
{
    m_Busy = false;
    m_Sharing = false;
    m_Stopping = false;
    m_Exported = !success;
    if (!success) {
        setStatus(tr("Could not return %1. Retry Stop sharing or run sudo usbip unbind --busid=%1.").arg(m_BusId));
    }
    else if (!m_Failure.trimmed().isEmpty()) {
        setStatus(tr("Device returned locally. Forwarding report: %1").arg(m_Failure.trimmed()));
    }
    else setStatus(tr("Device returned to this PC."));
}
