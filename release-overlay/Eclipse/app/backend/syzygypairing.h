#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>

namespace SyzygyPairing {
inline QString normalizePasskey(QString key)
{
    key = key.trimmed().toLower();
    if (key.size() != 48) return {};
    for (const QChar character : key) {
        if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
              (character >= QLatin1Char('a') && character <= QLatin1Char('f')))) return {};
    }
    return key;
}

inline QByteArray challengeMessage(const QByteArray& nonce, const QByteArray& uniqueId,
                                  const QByteArray& certificate)
{
    if (nonce.size() != 32 || uniqueId.isEmpty() || uniqueId.size() > 256 || certificate.isEmpty()) return {};
    QByteArray message("Syzygy pairing v1");
    const auto appendField = [&message](const QByteArray& field) {
        const quint32 size = static_cast<quint32>(field.size());
        for (int shift = 24; shift >= 0; shift -= 8) message.append(static_cast<char>(size >> shift));
        message.append(field);
    };
    appendField(nonce);
    appendField(uniqueId);
    appendField(QCryptographicHash::hash(certificate, QCryptographicHash::Sha256));
    return message;
}
}
