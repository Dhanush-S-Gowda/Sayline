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
        QTest::newRow("joining") << "well hyphen known it apostrophe s path forward slash file backslash name underscore end"
            << "well-known it's path/file\\name_end";
        QTest::newRow("symbols") << "mail at sign example percent sign next hash sign topic ampersand more"
            << "mail@example% next #topic & more";
        QTest::newRow("dash-ellipsis") << "Wait ellipsis really em dash yes" << QString::fromUtf8("Wait… really — yes");
        QTest::newRow("pairs") << "Say open quote hello close quote open parenthesis aside close parenthesis open bracket note close bracket full stop"
            << "Say \"hello\" (aside) [note].";
        QTest::newRow("lines") << "First new line second next line third new paragraph fourth"
            << "First\nsecond\nthird\n\nfourth";
        QTest::newRow("trailing-line") << "First new line." << "First\n";
        QTest::newRow("leading-paragraph") << "new paragraph Hello" << "\n\nHello";
        QTest::newRow("consecutive-lines") << "First new line next line last" << "First\n\nlast";
        QTest::newRow("line-punctuation") << "First full stop new line open quote next close quote" << "First.\n\"next\"";
        QTest::newRow("asr-pairs") << "open bracket, note close bracket. next line. end" << "[note]\nend";
        QTest::newRow("new-escapes") << "literal hyphen literal new line literal open quote literal at sign"
            << "hyphen new line open quote at sign";
        QTest::newRow("whitespace") << "OPEN\tPARENTHESIS hello CLOSE   PARENTHESIS" << "(hello)";
        QTest::newRow("combining-boundary") << QString::fromUtf8("commá hyphenated someone") << QString::fromUtf8("commá hyphenated someone");
        QTest::newRow("digits") << "zero one two three four five six seven eight nine" << "0 1 2 3 4 5 6 7 8 9";
        QTest::newRow("digits-sentence") << "I have TWO apples, one orange and zero pears." << "I have 2 apples, 1 orange and 0 pears.";
        QTest::newRow("digits-punctuation") << "one, two. three?! four: five; six..." << "1, 2. 3?! 4: 5; 6...";
        QTest::newRow("digits-commands") << "open bracket one close bracket next line two percent sign" << "[1]\n2%";
        QTest::newRow("digits-escape") << "literal one and literal zero" << "one and zero";
        QTest::newRow("digits-boundaries") << QString::fromUtf8("someone twosome stone one_two oné éone 123")
            << QString::fromUtf8("someone twosome stone one_two oné éone 123");
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

    void liveFormattingAndToggleBoundaries() {
        Transcript t; t.setPunctuationEnabled(true);
        t.snapshot("well hy"); t.delta("phen known new");
        QCOMPARE(t.display(), QString("well-known new"));
        t.delta(" line next");
        QCOMPARE(t.display(), QString("well-known\nnext"));
        t.complete("Well hyphen known new line. Next");
        QCOMPARE(t.display(), QString("Well-known\nNext"));
        t.snapshot("new"); t.setPunctuationEnabled(false); t.delta(" paragraph plain hyphen text");
        QCOMPARE(t.display(), QString("Well-known\nNext new paragraph plain hyphen text"));
        t.complete("new paragraph plain hyphen text");
        t.setPunctuationEnabled(true); t.snapshot("new paragraph open bracket done close bracket");
        QCOMPARE(t.display(), QString("Well-known\nNext new paragraph plain hyphen text\n\n[done]"));
        t.snapshot("literal new line");
        QCOMPARE(t.display(), QString("Well-known\nNext new paragraph plain hyphen text new line"));
    }

    void liveDigitsFollowPunctuationMode() {
        Transcript t;
        t.snapshot("one two");
        QCOMPARE(t.display(), QString("one two"));
        t.setPunctuationEnabled(true);
        t.delta(" three fo"); t.delta("ur");
        QCOMPARE(t.display(), QString("one two 3 4"));
        t.setPunctuationEnabled(false);
        t.delta(" five six");
        QCOMPARE(t.display(), QString("one two 3 4 five six"));
        t.complete("One two three four five six.");
        QCOMPARE(t.display(), QString("One two 3 4 five six."));
        t.setPunctuationEnabled(true);
        t.snapshot("literal seven eight");
        QCOMPARE(t.display(), QString("One two 3 4 five six. seven 8"));
        t.complete("literal seven eight.");
        QCOMPARE(t.display(), QString("One two 3 4 five six. seven 8."));
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
