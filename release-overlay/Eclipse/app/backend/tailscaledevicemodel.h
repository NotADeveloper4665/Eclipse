#pragma once

#include <QAbstractListModel>
#include <QProcess>
#include <QTimer>
#include <QVector>

class TailscaleDeviceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IpRole, OnlineRole };
    Q_ENUM(Roles)

    explicit TailscaleDeviceModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_Loading; }
    QString error() const { return m_Error; }
    QString statusText() const { return m_StatusText; }

    Q_INVOKABLE void refresh();

signals:
    void loadingChanged();
    void errorChanged();
    void statusChanged();

private:
    struct Device {
        QString name;
        QString ip;
        bool online;
    };

    static QVector<Device> parseStatus(const QByteArray& json, QString* statusText, QString* error);
    void clearDevices();
    void setError(const QString& error);
    void setLoading(bool loading);
    void setStatusText(const QString& statusText);

    QProcess m_Process;
    QTimer m_Timeout;
    QVector<Device> m_Devices;
    bool m_Loading = false;
    QString m_Error;
    QString m_StatusText;
    bool m_TimedOut = false;
};
