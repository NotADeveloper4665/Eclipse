#include "backend/devicepassthrough.h"
#include "backend/mediapassthrough.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QSignalSpy>
#include <QStandardPaths>

class PassthroughTest : public QObject {
    Q_OBJECT
    static void write(const QString& path, const QByteArray& text, bool executable = false) {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(text), qint64(text.size())); file.close();
        if (executable) QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    static QByteArray read(const QString& path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
private slots:
    void inputValidation() {
        QVERIFY(DevicePassthrough::validTarget("user@100.105.64.126"));
        QVERIFY(DevicePassthrough::validTarget("my-host"));
        QVERIFY(DevicePassthrough::validBusId("1-2.3"));
        for (const QString& value : {QString("-oProxyCommand=bad"), QString("host;id"), QString("user@host\n"), QString("$(id)"), QString("host name")}) QVERIFY(!DevicePassthrough::validTarget(value));
        for (const QString& value : {QString("usb1"), QString("1-2;id"), QString("../../x"), QString("1-2:1.0")}) QVERIFY(!DevicePassthrough::validBusId(value));
    }
    void compositeDeviceEnumerationAndHotUnplug() {
        QTemporaryDir tmp;
        write(tmp.path()+"/1-2/idVendor", "1234");
        write(tmp.path()+"/1-2/manufacturer", "Camera Corp");
        write(tmp.path()+"/1-2/product", "Camera + Microphone");
        write(tmp.path()+"/1-2:1.0/bInterfaceClass", "0e");
        write(tmp.path()+"/1-2:1.1/bInterfaceClass", "01");
        write(tmp.path()+"/1-3/bDeviceClass", "09");
        DevicePassthrough manager(nullptr, tmp.path());
        QCOMPARE(manager.devices().size(), 1);
        QVERIFY(manager.devices()[0].toMap()["audio"].toBool());
        QVERIFY(manager.devices()[0].toMap()["camera"].toBool());
        QFile::remove(tmp.path()+"/1-2/idVendor");
        manager.share("host", "1-2");
        QVERIFY(manager.status().contains("no longer connected"));
        QVERIFY(!manager.busy());
    }
    void usbLifecycle_data() {
        QTest::addColumn<bool>("sshFails");
        QTest::newRow("release on stop") << false;
        QTest::newRow("restore after failed SSH") << true;
    }
    void usbLifecycle() {
        QFETCH(bool, sshFails);
        QTemporaryDir tmp;
        QTcpServer daemon;
        if (!daemon.listen(QHostAddress::LocalHost, 3240)) QSKIP("Local port 3240 unavailable");
        const QByteArray oldPath = qgetenv("PATH");
        const QString bin = tmp.path()+"/bin";
        write(bin+"/pkexec", "#!/bin/sh\nexec \"$@\"\n", true);
        write(bin+"/usbip", "#!/bin/sh\nprintf '%s %s\\n' \"$1\" \"$2\" >> \"$ECLIPSE_TEST_LOG\"\n", true);
        write(bin+"/ssh", sshFails ? "#!/bin/sh\necho 'host unreachable' >&2\nexit 255\n" : "#!/bin/sh\nprintf 'ECLIPSE_USBIP_READY 02\\n'\ncat >/dev/null\n", true);
        write(tmp.path()+"/usb/1-2/idVendor", "1234");
        qputenv("PATH", bin.toLocal8Bit()+":"+oldPath);
        qputenv("ECLIPSE_TEST_LOG", (tmp.path()+"/log").toLocal8Bit());
        {
            DevicePassthrough manager(nullptr, tmp.path()+"/usb");
            manager.share("host", "1-2");
            if (sshFails) {
                QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
                QVERIFY(manager.status().contains("host unreachable"));
            }
            else {
                QTRY_VERIFY_WITH_TIMEOUT(manager.sharing(), 3000);
                QVERIFY(manager.canStop());
                manager.stop();
                QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
            }
            QVERIFY(!manager.sharing());
            QVERIFY(!manager.canStop());
            const QByteArray commands = read(tmp.path()+"/log");
            QVERIFY(commands.contains("bind --busid=1-2"));
            QVERIFY(commands.contains("unbind --busid=1-2"));
        }
        qputenv("PATH", oldPath);
        qunsetenv("ECLIPSE_TEST_LOG");
    }
    void nativeMediaPipe() {
        QTemporaryDir tmp;
        const QByteArray oldPath = qgetenv("PATH");
        const QString actualFfmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (actualFfmpeg.isEmpty()) QSKIP("FFmpeg unavailable");
        const QString bin = tmp.path()+"/bin";
        // Hardware-independent capture source. Exercise QProcess's real media pipe into SSH.
        write(bin+"/ffmpeg", ("#!/bin/sh\nexec '"+actualFfmpeg+"' -nostdin -hide_banner -loglevel error -re -f lavfi -i sine=frequency=440 -t 2 -ac 1 -ar 48000 -c:a libopus -application lowdelay -frame_duration 10 -flush_packets 1 -page_duration 10000 -f ogg pipe:1\n").toUtf8(), true);
        write(bin+"/ssh", "#!/bin/sh\necho ECLIPSE_MEDIA_READY >&2\ncat > \"$ECLIPSE_TEST_MEDIA\"\n", true);
        write(bin+"/pactl", "#!/bin/sh\nprintf '[]'\n", true);
        qputenv("PATH", bin.toLocal8Bit()+":"+oldPath);
        qputenv("ECLIPSE_TEST_MEDIA", (tmp.path()+"/capture.ogg").toLocal8Bit());
        {
            MediaPassthrough manager;
            manager.refresh();
            manager.start("host", "default");
            QTRY_VERIFY_WITH_TIMEOUT(manager.sharing(), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(!manager.sharing() && !manager.busy(), 6000);
            QVERIFY(read(tmp.path()+"/capture.ogg").startsWith("OggS"));
            QProcess verify;
            verify.start(actualFfmpeg, {"-v", "error", "-i", tmp.path()+"/capture.ogg", "-f", "null", "-"});
            QVERIFY(verify.waitForFinished(3000));
            QCOMPARE(verify.exitCode(), 0);
        }
        qputenv("PATH", oldPath);
        qunsetenv("ECLIPSE_TEST_MEDIA");
    }
};
QTEST_GUILESS_MAIN(PassthroughTest)
#include "passthrough_test.moc"
