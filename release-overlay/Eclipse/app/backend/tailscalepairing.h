#pragma once
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostInfo>
#include <QHostAddress>

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
    const auto query = [](QStringList arguments) {
        QProcess process;
        arguments.prepend("--socket=/run/tailscale/tailscaled.sock");
        process.start("/usr/bin/tailscale", arguments);
        if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(); return QJsonObject(); }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return QJsonObject();
        return QJsonDocument::fromJson(process.readAllStandardOutput()).object();
    };
    const auto status = query({"status", "--json"});
    if (status.value("BackendState").toString() != "Running" || !status.value("TUN").toBool()) return false;
    const auto suffix = "." + status.value("CurrentTailnet").toObject().value("MagicDNSSuffix").toString() + ".";
    if (suffix.size() < 4) return false;
    const auto peers = status.value("Peer").toObject();
    const auto addresses = QHostInfo::fromName(host).addresses();
    for (const auto& address : addresses) {
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            const auto peer = it.value().toObject();
            if (!peer.value("TailscaleIPs").toArray().contains(address.toString())) continue;
            const auto dns = peer.value("DNSName").toString();
            if (!peer.value("InNetworkMap").toBool() || peer.value("Expired").toBool() ||
                peer.value("ShareeNode").toBool() || peer.value("AltSharerUserID").toDouble() != 0 ||
                !dns.endsWith(suffix)) return false;
            const auto node = query({"whois", "--json", address.toString()}).value("Node").toObject();
            return node.value("MachineAuthorized").toBool() && node.value("StableID").toString() == peer.value("ID").toString() &&
                !node.value("StableID").toString().isEmpty() && node.value("Name").toString() == dns;
        }
    }
#else
    Q_UNUSED(host);
#endif
    return false;
}
}
