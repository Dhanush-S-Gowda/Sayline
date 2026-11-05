#include <QtTest>
#include "transcript.h"
#include "pastekeys.h"
class ProtocolTest : public QObject {
    Q_OBJECT
private slots:
    void spokenPunctuation_data() {
        QTest::addColumn<QString>("raw"); QTest::addColumn<QString>("rendered");
        QTest::newRow("commands") << "Hello comma world full stop" << "Hello, world.";
        QTest::newRow("remaining") << "Ready question mark Yes exclamation mark time colon now semicolon later" << "Ready? Yes! time: now; later";
        QTest::newRow("asr") << "Are you ready question mark." << "Are you ready?";
        QTest::newRow("escape") << "Write literal question mark and literal comma" << "Write question mark and comma";
        QTest::newRow("boundary") << "commander semicolons time period literal word" << "commander semicolons time period literal word";
        QTest::newRow("unicode") << QString::fromUtf8("Café COMMA 世界 FULL STOP") << QString::fromUtf8("Café, 世界.");
        QTest::newRow("unicode-boundary") << QString::fromUtf8("écomma commaé") << QString::fromUtf8("écomma commaé");
        QTest::newRow("unrelated") << "Wait... really?!" << "Wait... really?!";
    }
    void spokenPunctuation() {
        QFETCH(QString, raw); QFETCH(QString, rendered);
        QCOMPARE(formatPunctuation(raw), rendered);
        Transcript t; t.complete(raw);
        QCOMPARE(t.display(), raw);
        Transcript enabled; enabled.setPunctuationEnabled(true); enabled.complete(raw);
        QCOMPARE(enabled.display(), rendered);
        QCOMPARE(t.committed, raw);
    }
    void punctuationAcrossCompletionsAndPartials() {
        Transcript t; t.setPunctuationEnabled(true); t.complete("Ready question"); t.delta("mark");
        QCOMPARE(t.display(), QString("Ready?"));
        t.complete("mark.");
        QCOMPARE(t.display(), QString("Ready?"));
        t.snapshot("Hello comma");
        QCOMPARE(t.display(), QString("Ready? Hello,"));
        t.complete("Hello comma world full stop");
        QCOMPARE(t.display(), QString("Ready? Hello, world."));
        const auto edit = replacementEdit("Ready question mark", "Ready?");
        QCOMPARE(edit.backspaces, 14); QCOMPARE(edit.suffix, QString("?"));
    }

    void livePunctuationKeepsModesAfterToggleAndFinalization() {
        Transcript t;
        t.setPunctuationEnabled(true);
        t.delta("I am speaking com");
        t.delta("ma");
        QCOMPARE(t.display(), QString("I am speaking,"));
        t.setPunctuationEnabled(false);
        QCOMPARE(t.display(), QString("I am speaking,"));
        t.delta(" and now comma stays literal");
        QCOMPARE(t.display(), QString("I am speaking, and now comma stays literal"));
        t.complete("I am speaking comma and now comma stays literal.");
        QCOMPARE(t.display(), QString("I am speaking, and now comma stays literal."));
        QCOMPARE(t.rawDisplay(), QString("I am speaking comma and now comma stays literal."));
        t.setPunctuationEnabled(true);
        QCOMPARE(t.display(), QString("I am speaking, and now comma stays literal."));
        t.delta("Ready question"); t.delta(" mark");
        QCOMPARE(t.display(), QString("I am speaking, and now comma stays literal. Ready?"));
    }
    void fullRevisionKeepsHistoricalWordModes() {
        Transcript t; t.setPunctuationEnabled(true);
        t.snapshot("helo comma world");
        t.setPunctuationEnabled(false);
        t.snapshot("Hello comma world comma end");
        QCOMPARE(t.display(), QString("Hello, world comma end"));
        t.complete("Hello comma brave world comma end.");
        QCOMPARE(t.display(), QString("Hello, brave world comma end."));
    }
    void toggleDoesNotJoinCommandsAcrossModes() {
        Transcript t; t.setPunctuationEnabled(true); t.delta("Ready question");
        t.setPunctuationEnabled(false); t.delta(" mark");
        QCOMPARE(t.display(), QString("Ready question mark"));
        t.complete("Ready question mark");
        QCOMPARE(t.display(), QString("Ready question mark"));
        t.clear(); t.delta("comma"); QCOMPARE(t.display(), QString("comma"));
    }
    void liveEscapesAndRecognitionCorrections() {
        Transcript t; t.setPunctuationEnabled(true);
        t.delta("literal question"); t.delta(" mark");
        QCOMPARE(t.display(), QString("question mark"));
        t.snapshot("literal comma"); QCOMPARE(t.display(), QString("comma"));
        t.complete("literal comma"); QCOMPARE(t.display(), QString("comma"));
        t.delta("Hello comma"); QCOMPARE(t.display(), QString("comma Hello,"));
        t.snapshot("Hello commander"); QCOMPARE(t.display(), QString("comma Hello commander"));
    }

    void zedEditorAndEmbeddedTerminalSharePaste() {
        const QStringList shiftInsert{"42:1", "110:1", "110:0", "42:0"};
        QCOMPARE(pasteKeysForApp("dev.zed.Zed"), shiftInsert);
        QCOMPARE(pasteKeysForApp("zed"), shiftInsert);
        QCOMPARE(pasteKeysForApp("dev.zed.Zed-Preview"), shiftInsert);
    }
    void terminalAndBrowserPaste() {
        QCOMPARE(pasteKeysForApp("org.kde.konsole"),
                 (QStringList{"29:1", "42:1", "47:1", "47:0", "42:0", "29:0"}));
        QCOMPARE(pasteKeysForApp("firefox"),
                 (QStringList{"29:1", "47:1", "47:0", "29:0"}));
    }
    void partialCorrection() {
        const auto edit = replacementEdit("hello wrold", "hello world");
        QCOMPARE(edit.backspaces, 4);
        QCOMPARE(edit.suffix, QString("orld"));
    }
    void unicode() {
        const auto edit = replacementEdit(QString::fromUtf8("Hi 😀"), "Hi there");
        QCOMPARE(edit.backspaces, 1);
        QCOMPARE(edit.suffix, QString("there"));
    }
    void completionReplacesPreview() {
        Transcript t;
        t.delta("i am "); t.delta("speeking");
        QCOMPARE(t.display(), QString("i am speeking"));
        const auto before = t.display();
        t.complete("I am speaking.");
        auto edit = replacementEdit(before, t.display());
        QCOMPARE(edit.backspaces, before.size());
        QCOMPARE(edit.suffix, QString("I am speaking."));
        t.delta("Next");
        QCOMPARE(t.display(), QString("I am speaking. Next"));
        t.complete("Next.");
        QCOMPARE(t.display(), QString("I am speaking. Next."));
    }
    void deletionAndNoop() {
        QCOMPARE(replacementEdit("abc", "ab").backspaces, 1);
        QCOMPARE(replacementEdit("abc", "abc").backspaces, 0);
        QVERIFY(replacementEdit("abc", "abc").suffix.isEmpty());
    }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol_test.moc"
