#pragma once
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QStandardPaths>
#include <QProcessEnvironment>

namespace SyzygyPairing {
inline bool localTailnetPeer(const QString& host)
{
#ifdef Q_OS_LINUX
    const QFileInfo socket("/run/tailscale/tailscaled.sock");
    const QFileInfo directory("/run/tailscale");
    const QFileInfo executable("/usr/bin/tailscale");
    const auto writable = QFileDevice::WriteGroup | QFileDevice::WriteOther;
    if (!socket.exists() || socket.isSymLink() || socket.ownerId() != 0 ||
        directory.ownerId() != 0 || (directory.permissions() & writable) ||
        !executable.isExecutable() || executable.ownerId() != 0 || (executable.permissions() & writable)) return false;
    const QString program = QStringLiteral("/usr/bin/tailscale");
    const QStringList socketArgs{QStringLiteral("--socket=/run/tailscale/tailscaled.sock")};
#elif defined(Q_OS_MACOS)
    // Use the official, signed Tailscale.app CLI. Its local API connection
    // performs peer identity queries through the running macOS Tailscale daemon.
    const QString app = QStringLiteral("/Applications/Tailscale.app");
    const QString program = app + QStringLiteral("/Contents/MacOS/Tailscale");
    if (!QFileInfo(program).isExecutable()) return false;
    QProcess signature;
    signature.start(QStringLiteral("/usr/bin/codesign"),
                    {QStringLiteral("--verify"), QStringLiteral("--deep"),
                     QStringLiteral("--strict"), app});
    if (!signature.waitForFinished(3000) || signature.exitStatus() != QProcess::NormalExit ||
        signature.exitCode() != 0) return false;
    const QStringList socketArgs;
#else
    Q_UNUSED(host);
    return false;
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    const auto query = [&](QStringList arguments) {
        QProcess process;
        arguments = socketArgs + arguments;
#ifdef Q_OS_MACOS
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("TAILSCALE_BE_CLI"), QStringLiteral("1"));
        process.setProcessEnvironment(environment);
#endif
        process.start(program, arguments);
        if (!process.waitForFinished(2500)) {
            process.kill();
            process.waitForFinished();
            return QJsonObject();
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            return QJsonObject();
        return QJsonDocument::fromJson(process.readAllStandardOutput()).object();
    };

    const auto status = query({QStringLiteral("status"), QStringLiteral("--json")});
    if (status.value(QStringLiteral("BackendState")).toString() != QStringLiteral("Running")) return false;
#ifdef Q_OS_LINUX
    // Linux's system-daemon integration requires the kernel TUN interface.
    if (!status.value(QStringLiteral("TUN")).toBool()) return false;
#endif
    // macOS uses its managed Network Extension / packet-tunnel integration.
    // Do not use the Linux TUN flag as a proxy for macOS peer trust; the
    // whois StableID and network-map checks below remain mandatory.
    const auto suffix = QStringLiteral(".") +
        status.value(QStringLiteral("CurrentTailnet")).toObject()
              .value(QStringLiteral("MagicDNSSuffix")).toString() + QStringLiteral(".");
    if (suffix.size() < 4) return false;
    const QHostAddress address(host);
    if (address.isNull()) return false;
    const auto peers = status.value(QStringLiteral("Peer")).toObject();
    for (auto it = peers.begin(); it != peers.end(); ++it) {
        const auto peer = it.value().toObject();
        if (!peer.value(QStringLiteral("TailscaleIPs")).toArray()
                 .contains(address.toString())) continue;
        const auto dns = peer.value(QStringLiteral("DNSName")).toString();
        if (!peer.value(QStringLiteral("InNetworkMap")).toBool() ||
            peer.value(QStringLiteral("Expired")).toBool() ||
            peer.value(QStringLiteral("ShareeNode")).toBool() ||
            peer.value(QStringLiteral("AltSharerUserID")).toDouble() != 0 ||
            !dns.endsWith(suffix)) return false;
        const auto node = query({QStringLiteral("whois"), QStringLiteral("--json"),
                                 address.toString()}).value(QStringLiteral("Node")).toObject();
        return node.value(QStringLiteral("MachineAuthorized")).toBool() &&
            node.value(QStringLiteral("StableID")).toString() == peer.value(QStringLiteral("ID")).toString() &&
            !node.value(QStringLiteral("StableID")).toString().isEmpty() &&
            node.value(QStringLiteral("Name")).toString() == dns;
    }
#endif
    return false;
}
}
