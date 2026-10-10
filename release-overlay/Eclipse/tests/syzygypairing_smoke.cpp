#include "../app/backend/syzygypairing.h"
#include <QMessageAuthenticationCode>
#include <iostream>
#include <stdexcept>

void require(bool valid, const char* message)
{
    if (!valid) throw std::runtime_error(message);
}

int main()
{
    try {
        const QString phrase("abacus abacus abacus abacus abacus abacus");
        require(SyzygyPairing::normalizePasskey(" ABACUS-abacus ABACUS abacus abacus abacus ") == phrase, "normalize phrase");
        require(SyzygyPairing::normalizePasskey(QString(48, 'a')).isEmpty(), "reject old hexadecimal key");
        require(SyzygyPairing::phraseKey(phrase) == "9099aa9169cd7480309dc3cb217b62371c0486755779f78e", "match independent scrypt vector");
        const QByteArray message = SyzygyPairing::challengeMessage(QByteArray(32, 'n'), "desktop-1", "client-certificate");
        require(QCryptographicHash::hash(message, QCryptographicHash::Sha256).toHex() ==
                "ad6cba19e5df7f3387606cca01ab009591bf25c9a452ee6f26016bd40ca2ce8a", "match server transcript format");
        require(QMessageAuthenticationCode::hash(message, QByteArray(48, 'a'), QCryptographicHash::Sha256).toHex() ==
                "af4e3810806f0ca196fb292997b38d1a6d68fdbeeaba1be853ba1303725604a2", "match server proof vector");
        require(SyzygyPairing::challengeMessage(QByteArray(31, 'n'), "desktop-1", "client-certificate").isEmpty(), "reject malformed nonce");
        require(SyzygyPairing::challengeMessage(QByteArray(32, 'n'), "desktop-2", "client-certificate") != message, "bind client identity");
        require(SyzygyPairing::challengeMessage(QByteArray(32, 'n'), "desktop-1", "other-certificate") != message, "bind certificate");
        std::cout << "PASS: passkey validation and independent server-compatible challenge/proof vectors\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
