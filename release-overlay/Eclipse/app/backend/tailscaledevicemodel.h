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

public:
    enum Roles { NameRole = Qt::UserRole + 1, IpRole, OnlineRole };
    Q_ENUM(Roles)

    explicit TailscaleDeviceModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_Loading; }
    QString error() const { return m_Error; }

    Q_INVOKABLE void refresh();

signals:
    void loadingChanged();
    void errorChanged();

private:
    struct Device {
        QString name;
        QString ip;
        bool online;
    };

    static QVector<Device> parseStatus(const QByteArray& json, QString* error);
    void setError(const QString& error);
    void setLoading(bool loading);

    QProcess m_Process;
    QTimer m_Timeout;
    QVector<Device> m_Devices;
    bool m_Loading = false;
    QString m_Error;
};
