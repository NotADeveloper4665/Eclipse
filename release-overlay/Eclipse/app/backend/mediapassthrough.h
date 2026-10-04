#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// Native microphone/camera forwarding to a Linux receiver over an authenticated SSH pipe.
class MediaPassthrough : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString kind MEMBER m_Kind)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY inputsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sharing READ sharing NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool supported READ supported CONSTANT)
public:
    explicit MediaPassthrough(QObject* parent = nullptr);
    ~MediaPassthrough() override;
    QVariantList inputs() const { return m_Inputs; }
    bool busy() const { return m_Busy; }
    bool sharing() const { return m_Sharing; }
    QString status() const { return m_Status; }
    bool supported() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void start(const QString& sshTarget, const QString& input, const QString& cameraOutput = QStringLiteral("/dev/video42"));
    Q_INVOKABLE void stop();
signals:
    void inputsChanged();
    void stateChanged();
private:
    void setStatus(const QString& status);
    void fail(const QString& status);
    QString m_Kind = QStringLiteral("microphone");
    QString m_Status;
    QString m_Input;
    QString m_Log;
    QByteArray m_ReceiverOutput;
    QVariantList m_Inputs;
    bool m_Busy = false;
    bool m_Sharing = false;
    bool m_Stopping = false;
    bool m_Ready = false;
    QProcess m_Discovery;
    QProcess m_Capture;
    QProcess m_Receiver;
    QTimer m_Timeout;
};
