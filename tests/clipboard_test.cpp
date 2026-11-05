#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "clipboardwriter.h"
class ClipboardWriterTest : public QObject {
    Q_OBJECT
    QTemporaryDir tools;
    QByteArray oldPath;
    QString selection;
    void executable(const QString &name, const QByteArray &body) {
        QFile file(tools.path() + '/' + name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\n" + body);
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
private slots:
    void initTestCase() {
        QVERIFY(tools.isValid());
        oldPath = qgetenv("PATH");
        selection = tools.path() + "/selection";
        qputenv("PATH", tools.path().toUtf8() + ':' + oldPath);
        qputenv("SAYLINE_TEST_SELECTION", selection.toUtf8());
        executable("wl-copy", "[ \"$SAYLINE_TEST_COPY_FAIL\" = 1 ] && exit 1\nprintf '%s\\n' \"$@\" > \"$SAYLINE_TEST_SELECTION.args\"\ncat > \"$SAYLINE_TEST_SELECTION\"\n");
        executable("wl-paste", "if [ \"$SAYLINE_TEST_STALE\" = 1 ]; then printf 'old clipboard'; exit 0; fi\nif [ \"$SAYLINE_TEST_FIRST_STALE\" = 1 ] && [ ! -f \"$SAYLINE_TEST_SELECTION.attempt\" ]; then touch \"$SAYLINE_TEST_SELECTION.attempt\"; printf 'old clipboard'; else cat \"$SAYLINE_TEST_SELECTION\"; fi\n");
    }
    void init() {
        qunsetenv("SAYLINE_TEST_COPY_FAIL"); qunsetenv("SAYLINE_TEST_STALE");
        qunsetenv("SAYLINE_TEST_FIRST_STALE"); QFile::remove(selection + ".attempt");
    }
    void publishesUnicodeThroughStdin() {
        ClipboardWriter writer;
        QSignalSpy ready(&writer, &ClipboardWriter::ready), failed(&writer, &ClipboardWriter::failed);
        const auto text = QString::fromUtf8("Dictated café 😀\nsecond line");
        writer.publish(text);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 1000);
        QCOMPARE(failed.size(), 0);
        QFile file(selection); QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), text.toUtf8());
        QFile args(selection + ".args"); QVERIFY(args.open(QIODevice::ReadOnly));
        QCOMPARE(args.readAll(), QByteArray("--type\ntext/plain;charset=utf-8\n"));
    }
    void waitsForStaleSelectionToBeReplaced() {
        qputenv("SAYLINE_TEST_FIRST_STALE", "1");
        ClipboardWriter writer;
        QSignalSpy ready(&writer, &ClipboardWriter::ready), failed(&writer, &ClipboardWriter::failed);
        writer.publish("new dictated text");
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 1000);
        QCOMPARE(failed.size(), 0);
        QVERIFY(QFile::exists(selection + ".attempt"));
    }
    void permanentlyStaleSelectionNeverBecomesReady() {
        qputenv("SAYLINE_TEST_STALE", "1");
        ClipboardWriter writer;
        writer.m_deadline.setInterval(150);
        QSignalSpy ready(&writer, &ClipboardWriter::ready), failed(&writer, &ClipboardWriter::failed);
        writer.publish("new dictated text");
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 1000);
        QCOMPARE(ready.size(), 0);
        QVERIFY(failed.first().first().toString().contains("timed out"));
    }
    void failedCopyNeverBecomesReady() {
        qputenv("SAYLINE_TEST_COPY_FAIL", "1");
        ClipboardWriter writer;
        QSignalSpy ready(&writer, &ClipboardWriter::ready), failed(&writer, &ClipboardWriter::failed);
        writer.publish("new dictated text");
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 1000);
        QCOMPARE(ready.size(), 0);
    }
    void cleanupTestCase() {
        qputenv("PATH", oldPath);
        qunsetenv("SAYLINE_TEST_SELECTION"); qunsetenv("SAYLINE_TEST_COPY_FAIL");
        qunsetenv("SAYLINE_TEST_STALE"); qunsetenv("SAYLINE_TEST_FIRST_STALE");
    }
};
QTEST_GUILESS_MAIN(ClipboardWriterTest)
#include "clipboard_test.moc"
