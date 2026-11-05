#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include "runtime.h"
class RuntimeTest : public QObject {
    Q_OBJECT
    QTemporaryDir state;
private slots:
    void initTestCase() {
        QVERIFY(state.isValid());
        qputenv("XDG_STATE_HOME", state.path().toUtf8());
        const auto legacy = state.path() + "/nemodict";
        QVERIFY(QDir().mkpath(legacy + "/transcripts"));
        QFile oldLog(legacy + "/nemodict.log");
        QVERIFY(oldLog.open(QIODevice::WriteOnly)); oldLog.write("previous log\n"); oldLog.close();
        QFile oldSession(legacy + "/transcripts/session-previous.json");
        QVERIFY(oldSession.open(QIODevice::WriteOnly)); oldSession.write("{}"); oldSession.close();
        QString error;
        QVERIFY2(migrateLegacyState(&error), qPrintable(error));
        QVERIFY(!QDir(legacy).exists());
        QVERIFY(QFile::exists(stateDirectory() + "/sayline.log"));
        QVERIFY(QFile::exists(stateDirectory() + "/transcripts/session-previous.json"));
        QVERIFY2(migrateLegacyState(&error), qPrintable(error)); // Idempotent retry.
        initializeRuntimeLog();
    }
    void migrationPreservesExistingDestinationFiles() {
        const auto legacy = state.path() + "/nemodict/transcripts";
        QVERIFY(QDir().mkpath(legacy));
        QFile collision(legacy + "/session-previous.json");
        QVERIFY(collision.open(QIODevice::WriteOnly)); collision.write("different session"); collision.close();
        QString error;
        QVERIFY2(migrateLegacyState(&error), qPrintable(error));
        QCOMPARE(QDir(stateDirectory() + "/transcripts").entryList({"session-previous*.json"}, QDir::Files).size(), 2);
        QFile original(stateDirectory() + "/transcripts/session-previous.json");
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("{}"));
    }
    void structuredPrivateLog() {
        runtimeEvent("test.event", {{"count", 7}});
        QFile file(stateDirectory() + "/sayline.log");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto lines = file.readAll().trimmed().split('\n');
        const auto record = QJsonDocument::fromJson(lines.last()).object();
        QCOMPARE(record.value("event").toString(), QString("test.event"));
        QCOMPARE(record.value("count").toInt(), 7);
        QVERIFY(!record.value("time").toString().isEmpty());
        QCOMPARE(file.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), QFile::Permissions{});
    }
    void rotatesBoundedLog() {
        QFile file(stateDirectory() + "/sayline.log");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(QByteArray(2 * 1024 * 1024, 'x')), 2 * 1024 * 1024);
        file.close();
        runtimeEvent("test.after_rotation");
        QVERIFY(QFile::exists(file.fileName() + ".1"));
        QVERIFY(file.size() < 1024);
    }
    void retainsRecentAndDeletesExpiredSessions() {
        const auto directory = stateDirectory() + "/transcripts";
        QVERIFY(QDir().mkpath(directory));
        const auto now = QDateTime::currentDateTimeUtc();
        for (const auto &name : {QString("session-old.json"), QString("session-recent.json")}) {
            QFile file(directory + '/' + name);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("{}");
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(name.contains("old") ? now.addSecs(-90000) : now,
                                     QFileDevice::FileModificationTime));
        }
        pruneSessionHistory();
        QVERIFY(!QFile::exists(directory + "/session-old.json"));
        QVERIFY(QFile::exists(directory + "/session-recent.json"));
    }
    void cleanupTestCase() { qInstallMessageHandler(nullptr); }
};
QTEST_GUILESS_MAIN(RuntimeTest)
#include "runtime_test.moc"
