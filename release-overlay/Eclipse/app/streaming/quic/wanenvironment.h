#pragma once
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QDebug>
#include <thread>
#include <atomic>
#include <future>

// Optional, reversible Wi-Fi power change. One privileged helper owns restoration
// and restores on stdin EOF, normal stop, or SIGTERM. No saved connection changes.
class WanEnvironment {
    std::thread worker;
    std::atomic<bool> stopping{false};
    QByteArray oldPulse;
    bool hadPulse;
public:
    explicit WanEnvironment(const QString& host, bool wifi) {
        hadPulse = qEnvironmentVariableIsSet("PULSE_LATENCY_MSEC");
        oldPulse = qgetenv("PULSE_LATENCY_MSEC");
        qputenv("PULSE_LATENCY_MSEC", "10");
#ifdef Q_OS_LINUX
        if (!wifi) return;
        worker = std::thread([this, host]() {
            auto iw = QStandardPaths::findExecutable("iw");
            auto ip = QStandardPaths::findExecutable("ip");
            auto pkexec = QStandardPaths::findExecutable("pkexec");
            if (iw.isEmpty() || ip.isEmpty() || pkexec.isEmpty()) {
                qWarning() << "WAN Wi-Fi optimization unavailable; normal interface settings retained";
                return;
            }
            QProcess route;
            route.start(ip, {"route", "get", host});
            if (!route.waitForFinished(2000) || route.exitCode()) return;
            auto match = QRegularExpression("\\bdev ([A-Za-z0-9_.:-]{1,15})\\b").match(QString::fromUtf8(route.readAllStandardOutput()));
            if (!match.hasMatch()) return;
            QString iface = match.captured(1);
            QProcess state;
            state.start(iw, {"dev", iface, "get", "power_save"});
            if (!state.waitForFinished(2000) || state.exitCode()) return;
            auto saved = state.readAllStandardOutput();
            if (!saved.contains("Power save: on")) return;
            QString script = QStringLiteral(
                "restore() { \"$1\" dev \"$2\" set power_save on; }; "
                "iw_path=$1; iface=$2; "
                "trap '\"$iw_path\" dev \"$iface\" set power_save on' EXIT; "
                "trap 'exit 0' TERM INT HUP; "
                "\"$iw_path\" dev \"$iface\" set power_save off || exit 1; "
                "echo WAN_WIFI_READY; read -r stop");
            QProcess helper;
            helper.start(pkexec, {"/bin/sh", "-c", script, "wan-wifi", iw, iface});
            if (!helper.waitForStarted(2000)) return;
            QElapsedTimer timer;
            timer.start();
            QByteArray output;
            while (!stopping.load() && timer.elapsed() < 30000 && helper.state() != QProcess::NotRunning) {
                helper.waitForReadyRead(100);
                output += helper.readAllStandardOutput();
                if (output.contains("WAN_WIFI_READY")) break;
            }
            if (output.contains("WAN_WIFI_READY")) {
                qInfo() << "WAN Wi-Fi power saving disabled temporarily on" << iface;
                while (!stopping.load() && helper.state() != QProcess::NotRunning) helper.waitForFinished(100);
            } else qWarning() << "WAN Wi-Fi change unavailable or declined; normal fallback";
            if (helper.state() != QProcess::NotRunning) {
                helper.write("stop\n"); helper.closeWriteChannel();
                if (!helper.waitForFinished(2000)) {
                    helper.terminate();
                    if (!helper.waitForFinished(2000)) {
                        qWarning() << "Wi-Fi restoration helper did not exit; check power saving on" << iface;
                        helper.kill(); helper.waitForFinished(1000);
                    }
                }
            }
        });
#else
        Q_UNUSED(host); Q_UNUSED(wifi);
#endif
    }
    ~WanEnvironment() {
        stopping.store(true);
        if (worker.joinable()) worker.join();
        if (hadPulse) qputenv("PULSE_LATENCY_MSEC", oldPulse);
        else qunsetenv("PULSE_LATENCY_MSEC");
    }
};
