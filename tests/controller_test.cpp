#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QScopeGuard>
#include <QSettings>
#include "controller.h"

// Exercise actual controller timers and QProcess ownership using a harmless
// sleeping child, without model inference, microphone access, or input injection.
class ControllerTest : public QObject {
    Q_OBJECT
    QTemporaryDir state;
private slots:
    void initTestCase() {
        QVERIFY(state.isValid());
        qputenv("XDG_STATE_HOME", state.path().toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, state.path());
    }
    void shortcutRequestsSoundForStartAndStop() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        QSignalSpy sound(&controller, &Controller::shortcutSoundRequested);
        controller.Toggle(); controller.Toggle();
        QCOMPARE(sound.count(), 2);
    }
    void punctuationPreferenceAndHistory() {
        Controller controller;
        QVERIFY(!controller.punctuationEnabled());
        controller.setPunctuationEnabled(true);
        Controller restored;
        QVERIFY(restored.punctuationEnabled());
        controller.m_transcript.complete("Hello comma world full stop");
        QCOMPARE(controller.transcript(), QString("Hello, world."));
        controller.m_started = QDateTime::currentDateTimeUtc();
        controller.m_sessionId = "punctuation";
        controller.saveHistory();
        QFile file(state.path() + "/sayline/transcripts/session-punctuation.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto record = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(record["transcript"].toString(), QString("Hello comma world full stop"));
        QCOMPARE(record["rendered_transcript"].toString(), controller.transcript());
        QVERIFY(record["punctuation_enabled"].toBool());
        controller.setPunctuationEnabled(false);
        QCOMPARE(controller.transcript(), QString("Hello, world."));
    }
    void failureReleasesClipboardBusyState() {
        Controller controller;
        controller.m_injecting = true;
        controller.m_phase = Controller::Listening;
        controller.m_shown = true;
        controller.fail("Focus changed during clipboard publication");
        QVERIFY(!controller.m_injecting);
        controller.stopSession();
        controller.startSession();
        QVERIFY(controller.shown());
        QVERIFY(!controller.m_restart);
    }
    void idleShutdownStopsBoth() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        QCOMPARE(controller.m_idle.interval(), 300000);
        controller.m_backend.start("/usr/bin/sleep", {"60"});
        QVERIFY(controller.m_backend.waitForStarted());
        controller.m_phase = Controller::Loading;
        controller.m_shown = true;
        controller.m_idle.setInterval(40);
        controller.Toggle();
        QVERIFY(!controller.shown());
        QVERIFY(controller.m_idle.isActive());
        QTRY_COMPARE(controller.m_phase, Controller::Exiting);
        QTRY_COMPARE(controller.m_backend.state(), QProcess::NotRunning);
    }
    void reopeningCancelsIdleShutdown() {
        if (QStandardPaths::findExecutable("ydotool").isEmpty()) QSKIP("Requires ydotool preflight executable");
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.m_backend.start("/usr/bin/sleep", {"60"});
        QVERIFY(controller.m_backend.waitForStarted());
        const auto pid = controller.m_backend.processId();
        controller.m_phase = Controller::Loading;
        controller.m_shown = true;
        controller.m_idle.setInterval(50);
        controller.Toggle();
        QVERIFY(controller.m_idle.isActive());
        controller.m_bridgeReady = true;
        controller.m_focusId = "test-window";
        controller.Toggle();
        QVERIFY(controller.shown());
        QVERIFY(!controller.m_idle.isActive());
        QTest::qWait(100);
        QCOMPARE(controller.m_backend.state(), QProcess::Running);
        QCOMPARE(controller.m_backend.processId(), pid);
        QCOMPARE(controller.m_phase, Controller::Loading);
    }
    void missingFocusWaitsBeforeStartingModel() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.Toggle();
        QCOMPARE(controller.m_phase, Controller::WaitingForFocus);
        QCOMPARE(controller.m_backend.state(), QProcess::NotRunning);
        QVERIFY(controller.m_focusDeadline.isActive());
        controller.Toggle();
        QVERIFY(!controller.m_focusDeadline.isActive());
    }
    void shortcutTargetSurvivesLaterFocusChanges() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.ToggleOnWindow("pressed-window");
        QCOMPARE(controller.PlacementTarget(), QString("pressed-window"));
        controller.acceptFocus("later-focused-window", "dev.zed.Zed");
        QCOMPARE(controller.PlacementTarget(), QString("pressed-window"));
        controller.Toggle(); // close
        controller.ToggleOnWindow("other-monitor-window");
        QCOMPARE(controller.PlacementTarget(), QString("other-monitor-window"));
    }
    void partialTranscriptSurvivesFailure() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.m_started = QDateTime::currentDateTimeUtc();
        controller.m_sessionId = "test-partial";
        controller.m_transcript.delta("Unfinished sentence");
        controller.fail("Test insertion failure");
        QFile file(state.path() + "/sayline/transcripts/session-test-partial.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto record = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(record.value("transcript").toString(), QString("Unfinished sentence"));
        QCOMPARE(record.value("partial").toString(), QString("Unfinished sentence"));
        QCOMPARE(record.value("status").toString(), QString("error"));
        controller.m_transcript.complete("Finished sentence.");
        controller.saveHistory();
        QCOMPARE(QDir(state.path() + "/sayline/transcripts").entryList({"session-test-partial*.json"}, QDir::Files).size(), 1);
        QCOMPARE(file.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), QFile::Permissions{});
    }
    void renderedPunctuationUsesVerifiedClipboard() {
        QTemporaryDir helpers;
        QVERIFY(helpers.isValid());
        const auto previousPath = qgetenv("PATH");
        const auto restore = qScopeGuard([previousPath] { qputenv("PATH", previousPath); qunsetenv("SAYLINE_TEST_OFFER"); });
        qputenv("PATH", helpers.path().toUtf8() + ':' + previousPath);
        const auto selection = helpers.path() + "/selection";
        qputenv("SAYLINE_TEST_OFFER", selection.toUtf8());
        const QList<QPair<QString, QByteArray>> scripts{
            {"wl-copy", "cat > \"$SAYLINE_TEST_OFFER\"\n"},
            {"wl-paste", "cat \"$SAYLINE_TEST_OFFER\"\n"},
            {"ydotool", "exit 0\n"}};
        for (const auto &[name, body] : scripts) {
            QFile file(helpers.path() + '/' + name);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("#!/bin/sh\n" + body); file.close();
            QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
        Controller controller;
        controller.m_punctuationEnabled = true;
        controller.m_transcript.setPunctuationEnabled(true);
        controller.m_phase = Controller::Listening;
        controller.m_focusId = "test-editor";
        controller.m_focusClass = "dev.zed.Zed";
        controller.m_transcript.delta("Hello comma world full stop");
        controller.pumpInsertion();
        QTRY_COMPARE(controller.m_inserted, QString("Hello, world."));
        QCOMPARE(controller.transcript(), controller.m_inserted);
        QFile offer(selection); QVERIFY(offer.open(QIODevice::ReadOnly));
        QCOMPARE(offer.readAll(), controller.transcript().toUtf8());
        controller.setPunctuationEnabled(false);
        QCOMPARE(controller.transcript(), QString("Hello, world."));
        controller.handleMessage(R"({"type":"conversation.item.input_audio_transcription.delta","delta":" Now comma is literal"})");
        controller.pumpInsertion();
        QTRY_COMPARE(controller.m_inserted, QString("Hello, world. Now comma is literal"));
        controller.handleMessage(R"({"type":"conversation.item.input_audio_transcription.completed","transcript":"Hello comma world full stop Now comma is literal."})");
        controller.pumpInsertion();
        QTRY_COMPARE(controller.m_inserted, QString("Hello, world. Now comma is literal."));
    }
    void clipboardFailureSendsNoCorrectionKeys() {
        QTemporaryDir helpers;
        QVERIFY(helpers.isValid());
        const auto previousPath = qgetenv("PATH");
        const auto restore = qScopeGuard([previousPath] { qputenv("PATH", previousPath); });
        qputenv("PATH", helpers.path().toUtf8() + ':' + previousPath);
        QFile copy(helpers.path() + "/wl-copy");
        QVERIFY(copy.open(QIODevice::WriteOnly));
        copy.write("#!/bin/sh\nexit 1\n"); copy.close();
        copy.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const auto marker = helpers.path() + "/keys-sent";
        QFile keys(helpers.path() + "/ydotool");
        QVERIFY(keys.open(QIODevice::WriteOnly));
        keys.write(("#!/bin/sh\ntouch '" + marker + "'\n").toUtf8()); keys.close();
        keys.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.m_phase = Controller::Listening;
        controller.m_focusId = "test-editor";
        controller.m_focusClass = "dev.zed.Zed";
        controller.m_inserted = "wrong text";
        controller.m_punctuationEnabled = true;
        controller.m_transcript.setPunctuationEnabled(true);
        controller.m_transcript.complete("correct comma text");
        controller.pumpInsertion();
        QTRY_COMPARE_WITH_TIMEOUT(controller.m_phase, Controller::Failed, 1000);
        QVERIFY(!QFile::exists(marker));
        QCOMPARE(controller.m_inserted, QString("wrong text"));
    }
    void explicitQuitStopsOwnedBackend() {
        Controller controller;
        disconnect(&controller, &Controller::shortcutSoundRequested, &controller, nullptr);
        controller.m_backend.start("/usr/bin/sleep", {"60"});
        QVERIFY(controller.m_backend.waitForStarted());
        controller.Quit();
        QCOMPARE(controller.m_phase, Controller::Exiting);
        QTRY_COMPARE(controller.m_backend.state(), QProcess::NotRunning);
        QVERIFY(!controller.m_idle.isActive());
    }
};
QTEST_GUILESS_MAIN(ControllerTest)
#include "controller_test.moc"
