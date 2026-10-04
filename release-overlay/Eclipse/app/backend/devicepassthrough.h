#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// USB/IP exports local devices to the streaming host (the reverse of usbip attach).
// Sharing requires an explicit user selection; it never starts when streaming starts.
class DevicePassthrough : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sharing READ sharing NOTIFY stateChanged)
    Q_PROPERTY(bool canStop READ canStop NOTIFY stateChanged)
    Q_PROPERTY(bool supported READ supported CONSTANT)
public:
    explicit DevicePassthrough(QObject* parent = nullptr, const QString& usbRoot = QStringLiteral("/sys/bus/usb/devices"));
    ~DevicePassthrough() override;
    QVariantList devices() const { return m_Devices; }
    QString status() const { return m_Status; }
    bool busy() const { return m_Busy; }
    bool sharing() const { return m_Sharing; }
    bool canStop() const { return m_Exported && !m_Stopping; }
    bool supported() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void share(const QString& sshTarget, const QString& busId);
    Q_INVOKABLE void stop();
    static bool validTarget(const QString& target);
    static bool validBusId(const QString& busId);

signals:
    void devicesChanged();
    void stateChanged();

private:
    void setStatus(const QString& text);
    void openTunnel();
    void restoreDevice();
    void finishRestore(bool success);
    QString m_UsbRoot;
    QVariantList m_Devices;
    QString m_Status;
    QString m_Target;
    QString m_BusId;
    QString m_Usbip;
    bool m_Busy = false;
    bool m_Sharing = false;
    bool m_Exported = false;
    bool m_Stopping = false;
    QByteArray m_Output;
    QString m_Failure;
    int m_Port;
    QProcess m_Local;
    QProcess m_Tunnel;
    QTimer m_Timeout;
};
