#pragma once
#include <QProcess>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include "backend/identitymanager.h"
#include "backend/nvcomputer.h"

// This session owns its loopback proxy and credentials. No phrase is transmitted.
class QuicTunnel {
    QTemporaryDir directory;
    QProcess process;
public:
    QString local;
    QString error;
    int failureCode = 3;
    bool start(NvComputer *host) {
#ifndef Q_OS_LINUX
        Q_UNUSED(host);
        error = QStringLiteral("QUIC transport currently supports Linux clients only.");
        return false;
#else
        if (!directory.isValid() || host->serverCert.isNull()) {
            failureCode = 4;
            error = QStringLiteral("QUIC requires a saved paired host certificate.");
            return false;
        }
        const int base = host->activeAddress.port();
        if (base < 6 || base > 65504 || host->activeHttpsPort != base - 5) {
            error = QStringLiteral("QUIC requires the standard HTTPS offset for the configured host port.");
            return false;
        }
        QFile script(QStringLiteral(":/streaming/quic/syzygy-quic.py"));
        if (!script.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Missing QUIC transport asset.");
            return false;
        }
        auto write = [&](const QString &name, const QByteArray &data) {
            QFile file(directory.filePath(name));
            if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) return false;
            return file.write(data) == data.size();
        };
        if (!write(QStringLiteral("transport.py"), script.readAll()) ||
            !write(QStringLiteral("client.crt"), IdentityManager::get()->getCertificate()) ||
            !write(QStringLiteral("client.key"), IdentityManager::get()->getPrivateKey())) {
            failureCode = 4;
            error = QStringLiteral("Cannot create private QUIC session credentials.");
            return false;
        }
        local = QStringLiteral("127.90.%1.%2")
                .arg(QRandomGenerator::global()->bounded(1, 251)).arg(QRandomGenerator::global()->bounded(1, 251));
        process.start(QStringLiteral("python3"), {directory.filePath(QStringLiteral("transport.py")), QStringLiteral("client"),
            QStringLiteral("--host"), host->activeAddress.address(), QStringLiteral("--port"), QString::number(base+31),
            QStringLiteral("--base"), QString::number(base), QStringLiteral("--local"), local,
            QStringLiteral("--cert"), directory.filePath(QStringLiteral("client.crt")),
            QStringLiteral("--key"), directory.filePath(QStringLiteral("client.key")),
            QStringLiteral("--pin"), QString::fromLatin1(host->serverCert.digest(QCryptographicHash::Sha256).toHex())});
        if (!process.waitForStarted(2000)) {
            error = QStringLiteral("Cannot start Python QUIC transport.");
            return false;
        }
        QElapsedTimer timer;
        timer.start();
        QByteArray output;
        while (timer.elapsed() < 7000 && process.state() != QProcess::NotRunning) {
            process.waitForReadyRead(100);
            output += process.readAllStandardOutput();
            if (output.contains("SYZYGY_QUIC_READY")) return true;
        }
        process.waitForFinished(50);
        failureCode = process.state() == QProcess::NotRunning ? process.exitCode() : 3;
        error = QString::fromUtf8(process.readAllStandardError()).left(2000);
        if (error.isEmpty()) error = QStringLiteral("QUIC host did not become ready.");
        return false;
#endif
    }
    ~QuicTunnel() {
        if (process.state() != QProcess::NotRunning) {
            process.terminate();
            if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(1000); }
        }
    }
};
