#include "tailscaledevicemodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

TailscaleDeviceModel::TailscaleDeviceModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_Timeout.setSingleShot(true);
    m_Timeout.setInterval(8000);
    connect(&m_Timeout, &QTimer::timeout, this, [this] {
        m_TimedOut = true;
        m_Process.kill();
        clearDevices();
        setLoading(false);
        setStatusText(tr("Tailscale timed out"));
        setError(tr("Tailscale did not respond in time. Check that Tailscale is running, then refresh."));
    });
    connect(&m_Process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        m_Timeout.stop();
        if (m_TimedOut) {
            m_TimedOut = false;
            return;
        }
        setLoading(false);
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const QString diagnostic = QString::fromLocal8Bit(m_Process.readAllStandardError()).trimmed().left(240);
            clearDevices();
            setStatusText(tr("Unable to read Tailscale status"));
            setError(diagnostic.isEmpty()
                     ? tr("Tailscale could not read device status. Check that Tailscale is running and signed in.")
                     : tr("Tailscale could not read device status. Check that Tailscale is running and signed in.\n%1").arg(diagnostic));
            return;
        }

        QString parseError;
        QString status;
        const QVector<Device> devices = parseStatus(m_Process.readAllStandardOutput(), &status, &parseError);
        setStatusText(status);
        if (!parseError.isEmpty()) {
            clearDevices();
            setError(parseError);
            return;
        }

        beginResetModel();
        m_Devices = devices;
        endResetModel();
        setError(devices.isEmpty() ? tr("No other Tailscale devices were found.") : QString());
    });

    connect(&m_Process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_Timeout.stop();
            clearDevices();
            setLoading(false);
            setStatusText(tr("Tailscale CLI unavailable"));
            setError(tr("Tailscale CLI was not found. Install Tailscale and make sure its CLI is available."));
        }
    });
}

int TailscaleDeviceModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_Devices.size();
}

QVariant TailscaleDeviceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_Devices.size()) {
        return QVariant();
    }
    const Device& device = m_Devices.at(index.row());
    switch (role) {
    case NameRole: return device.name;
    case IpRole: return device.ip;
    case OnlineRole: return device.online;
    default: return QVariant();
    }
}

QHash<int, QByteArray> TailscaleDeviceModel::roleNames() const
{
    return {{NameRole, "name"}, {IpRole, "ip"}, {OnlineRole, "online"}};
}

void TailscaleDeviceModel::refresh()
{
    if (m_Process.state() != QProcess::NotRunning) {
        m_Process.kill();
        m_Process.waitForFinished(500);
    }
    m_TimedOut = false;

    QString program = QStandardPaths::findExecutable(
#ifdef Q_OS_WIN
        QStringLiteral("tailscale.exe")
#else
        QStringLiteral("tailscale")
#endif
    );
#ifdef Q_OS_MACOS
    if (program.isEmpty()) {
        const QString appCli = QStringLiteral("/Applications/Tailscale.app/Contents/MacOS/Tailscale");
        if (QFileInfo::exists(appCli)) program = appCli;
    }
#endif
    if (program.isEmpty()) {
        clearDevices();
        setLoading(false);
        setStatusText(tr("Tailscale CLI unavailable"));
        setError(tr("Tailscale CLI was not found. Install Tailscale and make sure its CLI is available."));
        return;
    }

    setError(QString());
    setStatusText(tr("Checking Tailscale…"));
    setLoading(true);
    m_Process.setProgram(program);
    m_Process.setArguments({QStringLiteral("status"), QStringLiteral("--json")});
    m_Process.start();
    m_Timeout.start();
}

QVector<TailscaleDeviceModel::Device> TailscaleDeviceModel::parseStatus(const QByteArray& json, QString* statusText, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *statusText = QObject::tr("Invalid Tailscale status");
        *error = QObject::tr("Tailscale returned invalid device data.");
        return {};
    }

    const QJsonObject status = document.object();
    const QString backendState = status.value(QStringLiteral("BackendState")).toString();
    if (backendState == QStringLiteral("Running")) {
        *statusText = QObject::tr("Connected to Tailscale");
    }
    else if (backendState == QStringLiteral("NeedsLogin")) {
        *statusText = QObject::tr("Sign-in required");
        *error = QObject::tr("Tailscale needs you to sign in. Open the Tailscale app, sign in to your tailnet, then refresh.");
        return {};
    }
    else if (backendState == QStringLiteral("Stopped")) {
        *statusText = QObject::tr("Tailscale is stopped");
        *error = QObject::tr("Start the Tailscale app or service, then refresh this list.");
        return {};
    }
    else {
        *statusText = backendState.isEmpty()
                ? QObject::tr("Tailscale is not connected")
                : QObject::tr("Tailscale: %1").arg(backendState);
        *error = QObject::tr("Tailscale is not connected to a tailnet. Start it or sign in, then refresh.");
        return {};
    }

    QVector<Device> devices;
    const QJsonObject peers = status.value(QStringLiteral("Peer")).toObject();
    for (auto it = peers.constBegin(); it != peers.constEnd(); ++it) {
        const QJsonObject peer = it.value().toObject();
        const QJsonArray addresses = peer.value(QStringLiteral("TailscaleIPs")).toArray();
        QString selectedIp;
        // Moonlight's host discovery accepts IPv4 and IPv6; prefer the familiar
        // 100.x address when both are advertised by a peer.
        for (const QJsonValue& addressValue : addresses) {
            const QString address = addressValue.toString();
            QHostAddress parsed;
            if (parsed.setAddress(address) && parsed.protocol() == QAbstractSocket::IPv4Protocol) {
                selectedIp = address;
                break;
            }
            if (selectedIp.isEmpty() && parsed.setAddress(address)) selectedIp = address;
        }
        if (selectedIp.isEmpty()) continue;

        QString name = peer.value(QStringLiteral("HostName")).toString();
        if (name.isEmpty()) name = peer.value(QStringLiteral("DNSName")).toString();
        if (name.endsWith('.')) name.chop(1);
        if (name.isEmpty()) name = selectedIp;
        devices.push_back({name, selectedIp, peer.value(QStringLiteral("Online")).toBool()});
    }

    std::sort(devices.begin(), devices.end(), [](const Device& a, const Device& b) {
        if (a.online != b.online) return a.online > b.online;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    error->clear();
    return devices;
}

void TailscaleDeviceModel::clearDevices()
{
    if (m_Devices.isEmpty()) {
        return;
    }
    beginResetModel();
    m_Devices.clear();
    endResetModel();
}

void TailscaleDeviceModel::setError(const QString& error)
{
    if (m_Error != error) {
        m_Error = error;
        emit errorChanged();
    }
}

void TailscaleDeviceModel::setLoading(bool loading)
{
    if (m_Loading != loading) {
        m_Loading = loading;
        emit loadingChanged();
    }
}

void TailscaleDeviceModel::setStatusText(const QString& statusText)
{
    if (m_StatusText != statusText) {
        m_StatusText = statusText;
        emit statusChanged();
    }
}
