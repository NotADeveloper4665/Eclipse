#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>
#include <QRegularExpression>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <stdexcept>

namespace SyzygyPairing {
inline QString normalizePasskey(QString key)
{
    if (key.size() > 128) return {};
    key = key.toLower().replace('-', ' ').simplified();
    if (!QRegularExpression("^[a-z]{1,9}( [a-z]{1,9}){5}$").match(key).hasMatch()) return {};
    return key;
}

inline QByteArray phraseKey(const QString& phrase)
{
    const QByteArray normalized = normalizePasskey(phrase).toLatin1();
    if (normalized.isEmpty()) return {};
    const QByteArray salt("Syzygy pairing phrase v2");
    QByteArray key(24, 0);
    if (EVP_PBE_scrypt(normalized.constData(), normalized.size(),
            reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
            32768, 8, 1, 64 * 1024 * 1024,
            reinterpret_cast<unsigned char*>(key.data()), key.size()) != 1)
        throw std::runtime_error("Pairing phrase derivation failed");
    const auto hex = key.toHex();
    OPENSSL_cleanse(key.data(), key.size());
    return hex;
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
