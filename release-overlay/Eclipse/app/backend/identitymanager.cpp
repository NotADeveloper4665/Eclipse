#include "identitymanager.h"
#include "utils.h"

#include <QDebug>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/rand.h>
#include <openssl/bn.h>

#define SER_UNIQUEID "uniqueid"
#define SER_CERT "certificate"
#define SER_KEY "key"

IdentityManager* IdentityManager::s_Im = nullptr;

IdentityManager*
IdentityManager::get()
{
    if (s_Im == nullptr) {
        s_Im = new IdentityManager();
    }
    return s_Im;
}

void IdentityManager::createCredentials(QSettings& settings)
{
    X509* cert = X509_new();
    THROW_BAD_ALLOC_IF_NULL(cert);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY* pk = EVP_RSA_gen(2048);
    THROW_BAD_ALLOC_IF_NULL(pk);
#else
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    THROW_BAD_ALLOC_IF_NULL(ctx);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
    EVP_PKEY* pk = NULL;
    EVP_PKEY_keygen(ctx, &pk);
    EVP_PKEY_CTX_free(ctx);
    THROW_BAD_ALLOC_IF_NULL(pk);
#endif

    X509_set_version(cert, 2);

    // RFC 5280 requires a positive, non-zero serial number of at most 20 octets.
    // Moonlight upstream used serial zero; cryptography now warns when parsing it.
    BIGNUM* serial = BN_new();
    THROW_BAD_ALLOC_IF_NULL(serial);
    if (BN_rand(serial, 159, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ODD) != 1) {
        BN_free(serial);
        qFatal("Unable to generate a valid X.509 certificate serial number");
    }
    ASN1_INTEGER* serialNumber = BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(cert));
    BN_free(serial);
    THROW_BAD_ALLOC_IF_NULL(serialNumber);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * 20);
#else
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 60 * 60 * 24 * 365 * 20);
#endif

    X509_set_pubkey(cert, pk);

    X509_NAME* name = X509_NAME_new();
    THROW_BAD_ALLOC_IF_NULL(name);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<unsigned char *>(const_cast<char*>("NVIDIA GameStream Client")),
                               -1, -1, 0);
    X509_set_subject_name(cert, name);
    X509_set_issuer_name(cert, name);
    X509_NAME_free(name);

    X509_sign(cert, pk, EVP_sha256());

    BIO* biokey = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biokey);
    PEM_write_bio_PrivateKey(biokey, pk, NULL, NULL, 0, NULL, NULL);

    BIO* biocert = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biocert);
    PEM_write_bio_X509(biocert, cert);

    BUF_MEM* mem;
    BIO_get_mem_ptr(biokey, &mem);
    m_CachedPrivateKey = QByteArray(mem->data, (int)mem->length);
    m_CachedSslKey = QSslKey();

    BIO_get_mem_ptr(biocert, &mem);
    m_CachedPemCert = QByteArray(mem->data, (int)mem->length);
    m_CachedSslCert = QSslCertificate();

    X509_free(cert);
    EVP_PKEY_free(pk);
    BIO_free(biokey);
    BIO_free(biocert);

    if (getSslCertificate().isNull()) qFatal("Newly generated certificate is unreadable");
    if (getSslKey().isNull()) qFatal("Newly generated private key is unreadable");

    settings.setValue(SER_CERT, m_CachedPemCert);
    settings.setValue(SER_KEY, m_CachedPrivateKey);
    qInfo() << "Wrote new identity credentials to settings";
}

IdentityManager::IdentityManager()
{
    QSettings settings;
    m_CachedPemCert = settings.value(SER_CERT).toByteArray();
    m_CachedPrivateKey = settings.value(SER_KEY).toByteArray();

    if (m_CachedPemCert.isEmpty() || m_CachedPrivateKey.isEmpty()) {
        qInfo() << "No existing credentials found";
        createCredentials(settings);
    }
    else if (getSslCertificate().isNull()) {
        qWarning() << "Certificate is unreadable";
        createCredentials(settings);
    }
    else if (getSslKey().isNull()) {
        qWarning() << "Private key is unreadable";
        createCredentials(settings);
    }

    if (getSslCertificate().isNull()) qFatal("Certificate is unreadable");
    if (getSslKey().isNull()) qFatal("Private key is unreadable");

    // Load the unique ID from settings.
    m_CachedUniqueId = settings.value(SER_UNIQUEID).toString();
    if (!m_CachedUniqueId.isEmpty()) {
        qInfo() << "Loaded unique ID from settings:" << m_CachedUniqueId;
    }
    else {
        uint64_t uid;
        RAND_bytes(reinterpret_cast<unsigned char*>(&uid), sizeof(uid));
        m_CachedUniqueId = QString::number(uid, 16);
        qInfo() << "Generated new unique ID:" << m_CachedUniqueId;
        settings.setValue(SER_UNIQUEID, m_CachedUniqueId);
    }
}

QSslCertificate
IdentityManager::getSslCertificate()
{
    if (m_CachedSslCert.isNull()) m_CachedSslCert = QSslCertificate(m_CachedPemCert);
    return m_CachedSslCert;
}

QSslKey
IdentityManager::getSslKey()
{
    if (m_CachedSslKey.isNull()) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(m_CachedPrivateKey.constData()), -1);
#else
        BIO* bio = BIO_new_mem_buf(m_CachedPrivateKey.constData(), -1);
#endif
        THROW_BAD_ALLOC_IF_NULL(bio);
        EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (pk == nullptr) return m_CachedSslKey;

        bio = BIO_new(BIO_s_mem());
        THROW_BAD_ALLOC_IF_NULL(bio);
#ifdef Q_OS_DARWIN
        PEM_write_bio_PrivateKey_traditional(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#else
        PEM_write_bio_PrivateKey(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#endif
        BUF_MEM* mem;
        BIO_get_mem_ptr(bio, &mem);
        m_CachedSslKey = QSslKey(QByteArray(mem->data, (int)mem->length), getSslCertificate().publicKey().algorithm());
        BIO_free(bio);
        EVP_PKEY_free(pk);
    }
    return m_CachedSslKey;
}

QSslConfiguration
IdentityManager::getSslConfig()
{
    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setLocalCertificate(getSslCertificate());
    sslConfig.setPrivateKey(getSslKey());
    return sslConfig;
}

QString IdentityManager::getUniqueId() { return m_CachedUniqueId; }
QByteArray IdentityManager::getCertificate() { return m_CachedPemCert; }
QByteArray IdentityManager::getPrivateKey() { return m_CachedPrivateKey; }
