#include <QtTest>
#include "transcript.h"
#include "pastekeys.h"
class ProtocolTest : public QObject {
    Q_OBJECT
private slots:
    void spokenPunctuation_data() {
        QTest::addColumn<QString>("raw"); QTest::addColumn<QString>("rendered");
        QTest::newRow("commands") << "Hello comma world full stop" << "Hello, world.";
        QTest::newRow("remaining") << "Ready question mark Yes exclamation mark time colon now semicolon later" << "Ready? Yes! Time: now; later";
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
            << "First\nSecond\nThird\n\nFourth";
        QTest::newRow("trailing-line") << "First new line." << "First\n";
        QTest::newRow("leading-paragraph") << "new paragraph Hello" << "\n\nHello";
        QTest::newRow("consecutive-lines") << "First new line next line last" << "First\n\nLast";
        QTest::newRow("line-punctuation") << "First full stop new line open quote next close quote" << "First.\n\"Next\"";
        QTest::newRow("asr-pairs") << "open bracket, note close bracket. next line. end" << "[note]\nEnd";
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
        QTest::newRow("space") << "hello space world" << "hello world";
        QTest::newRow("spaces") << "space hello space space world space" << " hello  world ";
        QTest::newRow("paragraph-alias") << "hello next paragraph world" << "hello\n\nWorld";
        QTest::newRow("decimal") << "zero point 2 and one point nine" << "0.2 and 1.9";
        QTest::newRow("point-word") << "the point is next point point 12 point twofold" << "the point is next point point 12 point twofold";
        QTest::newRow("decimal-escape") << "zero literal point two" << "0 point 2";
        QTest::newRow("caps") << "before caps lock on hello comma world caps lock off after" << "before HELLO, WORLD after";
        QTest::newRow("caps-of") << "caps lock on hello caps lock of After" << "HELLO After";
        QTest::newRow("caps-escape") << "literal caps lock on Hello" << "caps lock on Hello";
        QTest::newRow("keypoints") << "intro start keypoints first next point second stop keypoint end" << "intro\n- First\n- Second\nEnd";
        QTest::newRow("list-caps") << "start keypoints caps lock on first next point second caps lock off stop keypoint end" << "- FIRST\n- SECOND\nEnd";
        QTest::newRow("list-asr") << "Start keypoints. First next point. Second stop keypoint. End" << "- First\n- Second\nEnd";
        QTest::newRow("spaced-list") << "start key point first next point second stop key point End" << "- First\n- Second\nEnd";
        QTest::newRow("caps-spaced-list") << "CAPS LOCK ON START KEY POINT FIRST NEXT POINT SECOND STOP KEY POINT CAPS LOCK OFF End" << "- FIRST\n- SECOND\nEnd";
        QTest::newRow("plural-list") << "start key points first stop key points end" << "- First\nEnd";
        QTest::newRow("joined-list") << "start keypoints first stop keypoints end" << "- First\nEnd";
        QTest::newRow("bare-keypoints") << "This is my first key point and keypoints keypoint key points next point" << "This is my first key point and keypoints keypoint key points next point";
        QTest::newRow("bare-keypoints-active") << "start key point This is my first key point next point And now the second point stop key point keypoint next point key points" << "- This is my first key point\n- And now the second point\nKeypoint next point key points";
        QTest::newRow("list-restart") << "start key point first stop key point next point start keypoint second stop keypoint end" << "- First\nNext point\n- Second\nEnd";
        QTest::newRow("list-escape") << "literal start key point first" << "start key point first";
        QTest::newRow("backspace") << "hello backspace" << "hell";
        QTest::newRow("repeat-backspace") << "hello backspace backspace world" << "hel world";
        QTest::newRow("empty-backspace") << "backspace backspace hello" << "hello";
        QTest::newRow("backspace-space") << "hello space backspace" << "hello";
        QTest::newRow("backspace-symbol") << "hello comma backspace" << "hello";
        QTest::newRow("backspace-unicode") << QString::fromUtf8("Hi 😀 backspace") << "Hi ";
        QTest::newRow("backspace-combining") << QString::fromUtf8("café backspace") << "caf";
        QTest::newRow("backspace-caps") << "caps lock on HELLO BACKSPACE caps lock off end" << "HELL end";
        QTest::newRow("backspace-escape") << "hello literal backspace" << "hello backspace";
        QTest::newRow("sentence-capitalization") << "hello full stop next sentence question mark another sentence exclamation mark done" << "hello. Next sentence? Another sentence! Done";
        QTest::newRow("preserve-word-case") << "new line iPhone next line NASA new paragraph mixedCASE" << "\nIPhone\nNASA\n\nMixedCASE";
        QTest::newRow("quote-sentence") << "open quote hello close quote full stop open quote another close quote" << "\"Hello\". \"Another\"";
        QTest::newRow("quote-mid-sentence") << "say open quote hello close quote now" << "say \"hello\" now";
        QTest::newRow("capital-after-symbols") << "new line open bracket open quote hello close quote close bracket" << "\n[\"Hello\"]";
        QTest::newRow("capital-after-escape") << "full stop literal comma next" << ". Comma next";
        QTest::newRow("numeric-start") << "full stop two apples new line 42 cats" << ". 2 apples\n42 cats";
        QTest::newRow("decimal-no-sentence") << "zero point two apples" << "0.2 apples";
        QTest::newRow("deleted-sentence-boundary") << "hello full stop backspace world" << "hello world";
        QTest::newRow("literal-boundary") << "literal full stop hello literal new line world" << "full stop hello new line world";
        QTest::newRow("unicode-capital") << QString::fromUtf8("full stop éclair new line 𐐨ello") << QString::fromUtf8(". Éclair\n𐐀ello");
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
        QCOMPARE(t.display(), QString("well-known\nNext"));
        t.complete("Well hyphen known new line. Next");
        QCOMPARE(t.display(), QString("Well-known\nNext"));
        t.snapshot("new"); t.setPunctuationEnabled(false); t.delta(" paragraph plain hyphen text");
        QCOMPARE(t.display(), QString("Well-known\nNext new paragraph plain hyphen text"));
        t.complete("new paragraph plain hyphen text");
        t.setPunctuationEnabled(true); t.snapshot("new paragraph open bracket done close bracket");
        QCOMPARE(t.display(), QString("Well-known\nNext new paragraph plain hyphen text\n\n[Done]"));
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

    void liveFormattingModes() {
        Transcript t; t.setPunctuationEnabled(true);
        t.complete("caps lock on start keypoints first");
        t.snapshot("next point zero point");
        QCOMPARE(t.display(), QString("- FIRST\n- 0 POINT"));
        t.delta(" two");
        QCOMPARE(t.display(), QString("- FIRST\n- 0.2"));
        t.complete("next point zero point two");
        t.snapshot("stop keypoint caps lock off End");
        QCOMPARE(t.display(), QString("- FIRST\n- 0.2\nEnd"));
        t.snapshot("next point revised");
        QCOMPARE(t.display(), QString("- FIRST\n- 0.2\n- REVISED"));
        t.setPunctuationEnabled(false); t.delta(" lowercase comma");
        QCOMPARE(t.display(), QString("- FIRST\n- 0.2\n- REVISED lowercase comma"));
        t.clear(); t.setPunctuationEnabled(true); t.snapshot("ordinary next point");
        QCOMPARE(t.display(), QString("ordinary next point"));
    }
    void keypointModeRequiresExplicitStart() {
        Transcript t; t.setPunctuationEnabled(true);
        t.complete("my first key point next point");
        QCOMPARE(t.display(), QString("my first key point next point"));
        t.complete("start key point This is my first key point");
        t.snapshot("next point second");
        QCOMPARE(t.display(), QString("my first key point next point\n- This is my first key point\n- Second"));
        t.complete("next point second stop key point");
        t.snapshot("keypoint next point");
        QCOMPARE(t.display(), QString("my first key point next point\n- This is my first key point\n- Second\nKeypoint next point"));
        t.complete("keypoint next point");
        t.snapshot("start keypoint again");
        QCOMPARE(t.display(), QString("my first key point next point\n- This is my first key point\n- Second\nKeypoint next point\n- Again"));
    }
    void editingAcrossUpdates() {
        Transcript t; t.setPunctuationEnabled(true);
        t.complete("hello"); t.snapshot("back"); t.delta("space");
        QCOMPARE(t.display(), QString("hell"));
        t.complete("backspace");
        QCOMPARE(t.display(), QString("hell"));
        t.snapshot("backspace");
        QCOMPARE(t.display(), QString("hel"));
        t.snapshot("world");
        QCOMPARE(t.display(), QString("hell world"));
        t.snapshot("start key"); t.delta(" point first");
        QCOMPARE(t.display(), QString("hell\n- First"));
        t.setPunctuationEnabled(false); t.delta(" backspace");
        QCOMPARE(t.display(), QString("hell\n- First backspace"));
        t.clear(); t.setPunctuationEnabled(true); t.snapshot("backspace");
        QVERIFY(t.display().isEmpty());
    }
    void capitalizationAcrossUpdatesAndModes() {
        Transcript t; t.setPunctuationEnabled(true);
        t.complete("hello full stop"); t.snapshot("n");
        QCOMPARE(t.display(), QString("hello. N"));
        t.delta("ext word"); QCOMPARE(t.display(), QString("hello. Next word"));
        t.complete("next word");
        t.snapshot("new line open quote lower");
        QCOMPARE(t.display(), QString("hello. Next word\n\"Lower"));
        t.snapshot("new line open quote revised");
        QCOMPARE(t.display(), QString("hello. Next word\n\"Revised"));
        t.complete("new line");
        t.setPunctuationEnabled(false); t.snapshot("lowercase comma");
        QCOMPARE(t.display(), QString("hello. Next word\nlowercase comma"));
        t.setPunctuationEnabled(true); t.delta(" another");
        QCOMPARE(t.display(), QString("hello. Next word\nlowercase comma another"));
        t.clear(); t.snapshot("lowercase");
        QCOMPARE(t.display(), QString("lowercase"));
    }
    void decimalAcrossToggle() {
        Transcript t; t.setPunctuationEnabled(true); t.snapshot("zero point");
        t.setPunctuationEnabled(false); t.delta(" two");
        QCOMPARE(t.display(), QString("0 point two"));
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
