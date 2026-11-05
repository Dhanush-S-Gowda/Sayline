#include "controller.h"
#include "pastekeys.h"
#include "runtime.h"
#include <QUuid>
#include <QAudioDevice>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMediaDevices>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>

namespace {
constexpr auto host = "127.0.0.1";
constexpr int port = 18080;
QString historyPath() {
    return stateDirectory() + "/transcripts";
}
void pruneHistory() { pruneSessionHistory(); }

}

Controller::Controller(QObject *parent) : QObject(parent) {
    m_punctuationEnabled = QSettings().value("dictation/punctuationEnabled", false).toBool();
    m_transcript.setPunctuationEnabled(m_punctuationEnabled);
    connect(this, &Controller::shortcutSoundRequested, this, [this] {
        if (!m_soundPlayer) {
            m_soundOutput = std::make_unique<QAudioOutput>();
            m_soundPlayer = std::make_unique<QMediaPlayer>();
            m_soundPlayer->setAudioOutput(m_soundOutput.get());
            connect(m_soundPlayer.get(), &QMediaPlayer::errorOccurred, this,
                [](QMediaPlayer::Error, const QString &error) { runtimeEvent("sound.error", {{"message", error}}); });
            m_soundPlayer->setSource(QUrl("qrc:/sound.mp3"));
        }
        m_soundPlayer->stop();
        m_soundPlayer->play();
    });
    m_bridgeToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_focusDeadline.setSingleShot(true);
    m_snapshotTimer.setSingleShot(true);
    m_snapshotTimer.setInterval(750);
    m_historyTimer.setInterval(60000);
    connect(&m_snapshotTimer, &QTimer::timeout, this, &Controller::saveHistory);
    connect(&m_historyTimer, &QTimer::timeout, this, [] { pruneHistory(); });
    m_historyTimer.start();
    connect(&m_focusDeadline, &QTimer::timeout, this, [this] {
        if (m_phase == WaitingForFocus)
            fail("KWin has not reported the focused app. See sayline.log for bridge diagnostics.");
    });
    connect(&m_clipboard, &ClipboardWriter::ready, this, [this] {
        if (m_phase == Exiting || m_phase == Failed) { m_injecting = false; return; }
        if (m_focusId != m_injectionFocus) {
            m_injecting = false;
            fail("Focus changed during clipboard publication. No paste keys were sent."); return;
        }
        runtimeEvent("clipboard.verified", {{"characters", m_pendingText.size()}});
        injectKeys(m_pendingKeys + pasteKeysForApp(m_focusClass));
    });
    connect(&m_clipboard, &ClipboardWriter::failed, this, [this](const QString &message) {
        m_injecting = false; m_insertionFailed = true;
        runtimeEvent("clipboard.failed", {{"message", message}});
        fail(message);
    });
    m_poll.setInterval(250);
    m_idle.setSingleShot(true);
    m_idle.setInterval(300000);
    m_deadline.setSingleShot(true);
    m_keyDeadline.setSingleShot(true);
    m_keyDeadline.setInterval(5000);
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(3000);
    m_insertionTimer.setInterval(90);
    connect(&m_poll, &QTimer::timeout, this, &Controller::pollReady);
    connect(&m_idle, &QTimer::timeout, this, &Controller::beginExit);
    connect(&m_insertionTimer, &QTimer::timeout, this, &Controller::pumpInsertion);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        if (m_phase == Finalizing) {
            m_lastError = "Finalization timed out; partial text may be incomplete.";
            qWarning().noquote() << m_lastError;
            finishSession();
        } else fail("Speech connection timed out. Press Ctrl+Space to close and retry.");
    });
    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_backend.state() != QProcess::NotRunning) m_backend.kill();
        else QCoreApplication::quit();
    });
    const auto parentPid = getpid();
    m_backend.setChildProcessModifier([parentPid] {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, nullptr);
        // Also release the model if the Qt process crashes or is killed.
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parentPid) _exit(1);
    });
    m_backend.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_backend, &QProcess::readyReadStandardOutput, this, [this] {
        // Consume output so a verbose backend cannot grow an unbounded buffer.
        const auto output = m_backend.readAllStandardOutput();
        m_backendLog = (m_backendLog + QString::fromUtf8(output)).right(8000);
        runtimeEvent("backend.output", {{"message", QString::fromUtf8(output).right(4000)}});
        if (m_phase == Loading) {
            m_status = QString("Loading Nemotron… %1 s").arg(m_loadTime.elapsed() / 1000);
            emit changed();
        }
        if (m_backend.state() == QProcess::NotRunning && !output.isEmpty())
            qWarning().noquote() << QString::fromUtf8(output).right(2000);
    });
    connect(&m_backend, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_phase != Exiting && error == QProcess::FailedToStart)
            fail("Cannot start nemo-speech. Check that it is in PATH.");
    });
    connect(&m_backend, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
        m_ready = false;
        runtimeEvent("backend.exit", {{"code", code}});
        if (m_phase == Exiting) { m_killTimer.stop(); QCoreApplication::quit(); }
        else { qWarning().noquote() << m_backendLog; fail(QString("nemo-speech exited (%1). Press Ctrl+Space to close and retry.").arg(code)); }
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &Controller::handleMessage);
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        if (m_phase == Listening || m_phase == Connecting) fail("Speech server disconnected.");
        else if (m_phase == Finalizing) finishSession();
    });
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_phase == Connecting || m_phase == Listening || m_phase == Finalizing)
            fail("Speech connection failed: " + m_socket.errorString());
    });
    connect(&m_keyDeadline, &QTimer::timeout, this, [this] {
        m_keys.kill();
        m_insertionFailed = true;
        fail("Typing timed out. Check ydotoold and its socket permissions.");
    });
    connect(&m_keys, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_keyDeadline.stop(); m_injecting = false; m_insertionFailed = true;
            fail("Cannot start ydotool. Install it and configure ydotoold.");
        }
    });
    connect(&m_keys, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
        m_keyDeadline.stop();
        runtimeEvent("insertion.finished", {{"code", code}, {"normal_exit", exitStatus == QProcess::NormalExit}});
        if (m_phase == Exiting) return;
        if (code || exitStatus != QProcess::NormalExit) {
            m_injecting = false; m_insertionFailed = true;
            fail("Typing failed: " + QString::fromUtf8(m_keys.readAllStandardError()).left(240));
            return;
        }
        m_inserted = m_pendingText;
        // Give the target time to consume this clipboard offer before replacing it.
        QTimer::singleShot(110, this, [this] { m_injecting = false; pumpInsertion(); });
    });
    pruneHistory();
    m_idle.start();
}
Controller::~Controller() { shutdown(); }
void Controller::setPhase(Phase phase, const QString &status) {
    runtimeEvent("state.change", {{"from", int(m_phase)}, {"to", int(phase)}, {"status", status}});
    m_phase = phase; m_status = status; emit changed();
    saveHistory();
}
void Controller::Toggle() {
    toggleForWindow(m_focusId);
}
void Controller::ToggleOnWindow(const QString &windowId) {
    toggleForWindow(windowId);
}
QString Controller::PlacementTarget() const {
    return m_placementTarget.isEmpty() ? m_focusId : m_placementTarget;
}
void Controller::toggleForWindow(const QString &windowId) {
    runtimeEvent("shortcut.toggle", {{"shown", m_shown}, {"phase", int(m_phase)}});
    if (m_phase == Exiting) return;
    emit shortcutSoundRequested();
    if (!m_shown) {
        m_placementTarget = windowId;
        runtimeEvent("overlay.target", {{"window", windowId}});
    }
    if (m_shown) stopSession();
    else if (m_phase == Finalizing || m_injecting) m_restart = !m_restart;
    else startSession();
}
void Controller::setPunctuationEnabled(bool enabled) {
    if (m_punctuationEnabled == enabled) return;
    m_punctuationEnabled = enabled;
    m_transcript.setPunctuationEnabled(enabled);
    runtimeEvent("punctuation.toggle", {{"enabled", enabled}, {"raw_characters", m_transcript.rawDisplay().size()}});
    QSettings().setValue("dictation/punctuationEnabled", enabled);
    emit changed();
    saveHistory();
    // A toggle affects only new words; previously recognized words retain their mode.
    if (!m_sessionId.isEmpty() && m_phase != Idle) pumpInsertion();
}
void Controller::Quit() { beginExit(); }
void Controller::Focus(const QString &id, const QString &resourceClass, const QString &token) {
    // Authenticate the private, per-process script rather than silently dropping
    // reports based on an additional synchronous D-Bus service-owner lookup.
    if (!calledFromDBus() || token != m_bridgeToken) {
        runtimeEvent("focus.rejected", {{"dbus", calledFromDBus()}});
        return;
    }
    acceptFocus(id, resourceClass);
}
void Controller::acceptFocus(const QString &id, const QString &resourceClass) {
    m_focusId = id; m_focusClass = resourceClass;
    runtimeEvent("focus.changed", {{"window", id}, {"class", resourceClass}});
    if (m_phase == WaitingForFocus && m_bridgeReady && !id.isEmpty() && m_shown) {
        m_focusDeadline.stop(); ensureBackend();
    }
}
void Controller::BridgeEvent(const QString &event, const QString &details, const QString &token) {
    if (!calledFromDBus() || token != m_bridgeToken) return;
    runtimeEvent("bridge." + event, {{"details", details}});
    if (event == "ready") {
        m_bridgeReady = true;
        if (m_phase == WaitingForFocus && !m_focusId.isEmpty() && m_shown) {
            m_focusDeadline.stop(); ensureBackend();
        }
    } else if (event == "error") fail("KWin integration failed: " + details);
}
void Controller::initializeFocusBridge() {
    QFile script(":/packaging/focus.js");
    if (!script.open(QIODevice::ReadOnly) || !m_focusScript.open()) {
        fail("Cannot prepare KWin focus integration."); return;
    }
    auto source = script.readAll();
    source.replace("@DBUS_OWNER@", QDBusConnection::sessionBus().baseService().toUtf8());
    source.replace("@BRIDGE_TOKEN@", m_bridgeToken.toUtf8());
    source.replace("@APP_PID@", QByteArray::number(QCoreApplication::applicationPid()));
    runtimeEvent("bridge.loading", {{"script", m_focusScript.fileName()}});
    m_focusScript.write(source); m_focusScript.flush();
    auto bus = QDBusConnection::sessionBus();
    auto unload = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript");
    unload << QString("sayline-focus");
    auto *unloadWatcher = new QDBusPendingCallWatcher(bus.asyncCall(unload), this);
    connect(unloadWatcher, &QDBusPendingCallWatcher::finished, this, [this, bus](QDBusPendingCallWatcher *done) {
        done->deleteLater();
        auto load = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript");
        load << m_focusScript.fileName() << QString("sayline-focus");
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(load), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, bus](QDBusPendingCallWatcher *result) {
            QDBusPendingReply<int> reply = *result; result->deleteLater();
            if (reply.isError() || reply.value() < 0) {
                fail("KWin focus integration failed. Check that KWin scripting is available."); return;
            }
            m_scriptId = reply.value();
            runtimeEvent("bridge.loaded", {{"script_id", m_scriptId}});
            auto run = QDBusMessage::createMethodCall("org.kde.KWin",
                QString("/Scripting/Script%1").arg(m_scriptId), "org.kde.kwin.Script", "run");
            auto *runWatcher = new QDBusPendingCallWatcher(bus.asyncCall(run), this);
            connect(runWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *result) {
                QDBusPendingReply<> reply = *result; result->deleteLater();
                if (reply.isError()) fail("Cannot run KWin focus integration: " + reply.error().message());
                else runtimeEvent("bridge.run_completed");
            });
        });
    });
}
void Controller::startSession() {
    m_idle.stop(); m_restart = false; m_shown = true;
    m_transcript.clear(); m_transcript.setPunctuationEnabled(m_punctuationEnabled); m_inserted.clear(); m_pendingText.clear();
    m_lastError.clear(); m_insertionFailed = false;
    m_started = QDateTime::currentDateTimeUtc();
    m_sessionId = m_started.toString("yyyyMMdd'T'HHmmsszzz'Z'") + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    runtimeEvent("session.start", {{"session", m_sessionId}});
    saveHistory();
    m_level = 0;
    for (const auto &command : {"ydotool", "wl-copy", "wl-paste"}) {
        if (QStandardPaths::findExecutable(command).isEmpty()) {
            fail(QString("Missing %1. Install ydotool and wl-clipboard before dictating.").arg(command)); return;
        }
    }
    if (!m_bridgeReady || m_focusId.isEmpty()) {
        setPhase(WaitingForFocus, "Connecting to the focused app…");
        m_focusDeadline.start(5000);
        return;
    }
    ensureBackend();
}
void Controller::ensureBackend() {
    if (m_ready && m_backend.state() == QProcess::Running) { openSocket(); return; }
    setPhase(Loading, "Loading Nemotron…");
    m_loadTime.start();
    if (m_backend.state() == QProcess::NotRunning) {
        // Refuse an unrelated listener: it must never be killed at idle shutdown.
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost, port)) {
            fail("Port 18080 is in use. Stop your separately started nemo-speech server first."); return;
        }
        reservation.close();
        m_backendLog.clear();
        runtimeEvent("backend.start", {{"model", "nemotron-en"}, {"device", "cpu"}, {"port", port}});
        m_backend.start("nemo-speech", {"serve", "--asr-model", "nemotron-en", "--device", "cpu",
                                       "--host", host, "--port", QString::number(port)});
    }
    m_poll.start(); pollReady();
}
void Controller::pollReady() {
    if (m_phase != Loading || m_probePending) return;
    if (m_loadTime.elapsed() > 180000) { fail("Model loading exceeded three minutes. Close and retry."); return; }
    m_status = QString("Loading Nemotron… %1 s").arg(m_loadTime.elapsed() / 1000); emit changed();
    if (m_backend.state() != QProcess::Running) return;
    m_probePending = true;
    QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/ready").arg(port)));
    request.setTransferTimeout(1000);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_probePending = false;
        const bool ready = reply->error() == QNetworkReply::NoError
            && QJsonDocument::fromJson(reply->readAll()).object().value("ready").toBool();
        reply->deleteLater();
        if (m_phase != Loading) return;
        if (ready) { runtimeEvent("backend.ready", {{"elapsed_ms", m_loadTime.elapsed()}}); m_ready = true; m_poll.stop(); openSocket(); }
    });
}
void Controller::openSocket() {
    if (!m_shown) return;
    const auto input = QMediaDevices::defaultAudioInput();
    if (input.isNull()) { fail("No microphone found. Choose an input in KDE Sound settings."); return; }
    m_format = input.preferredFormat();
    QAudioFormat preferred;
    preferred.setSampleRate(16000); preferred.setChannelCount(1); preferred.setSampleFormat(QAudioFormat::Int16);
    if (input.isFormatSupported(preferred)) m_format = preferred;
    if (m_format.sampleRate() < 8000 || m_format.sampleRate() > 96000 || m_format.bytesPerFrame() <= 0) {
        fail("The microphone format is unsupported (requires 8–96 kHz PCM)."); return;
    }
    setPhase(Connecting, "Connecting microphone…");
    m_deadline.start(10000);
    m_socket.open(QUrl(QString("ws://127.0.0.1:%1/v1/audio/transcriptions/realtime").arg(port)));
}
void Controller::handleMessage(const QString &message) {
    if (m_phase != Connecting && m_phase != Listening && m_phase != Finalizing) return;
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(message.toUtf8(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) { fail("Invalid speech server message."); return; }
    const auto event = document.object();
    const auto type = event.value("type").toString();
    runtimeEvent("asr.event", {{"type", type}});
    if (type == "error") { fail(event.value("error").toObject().value("message").toString("Speech server error.")); return; }
    if (type == "session.created" && m_phase == Connecting) {
        QJsonObject session{{"sample_rate", m_format.sampleRate()}, {"language", "en-US"}, {"automatic_punctuation", true}};
        m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"type", "session.update"}, {"session", session}}).toJson(QJsonDocument::Compact)));
    } else if (type == "session.updated" && m_phase == Connecting) {
        m_deadline.stop(); startAudio();
    } else if (type.endsWith(".delta")) {
        if (event.value("transcript").isString()) m_transcript.snapshot(event.value("transcript").toString());
        else if (event.value("delta").isString()) m_transcript.delta(event.value("delta").toString());
        if (!m_snapshotTimer.isActive()) m_snapshotTimer.start();
        emit changed();
    } else if (type.endsWith(".completed")) {
        if (!event.value("transcript").isString()) { fail("Missing completed transcript."); return; }
        m_transcript.complete(event.value("transcript").toString()); saveHistory(); emit changed();
    } else if (type == "input_audio_buffer.committed" && m_phase == Finalizing) finishSession();
}
void Controller::startAudio() {
    if (!m_shown || m_phase != Connecting) return;
    m_audio = std::make_unique<QAudioSource>(QMediaDevices::defaultAudioInput(), m_format);
    m_audio->setBufferSize(m_format.bytesForDuration(100000));
    connect(m_audio.get(), &QAudioSource::stateChanged, this, [this] {
        if (m_audio && m_audio->error() != QAudio::NoError && m_phase == Listening)
            fail("Microphone capture failed. Check KDE Sound settings.");
    });
    runtimeEvent("audio.start", {{"rate", m_format.sampleRate()}, {"channels", m_format.channelCount()}, {"sample_format", int(m_format.sampleFormat())}});
    m_audioBytes = 0; m_audioLogTime.start();
    m_audioRemainder.clear();
    m_audioDevice = m_audio->start();
    if (!m_audioDevice || m_audio->error() != QAudio::NoError) { fail("Cannot open the microphone."); return; }
    connect(m_audioDevice, &QIODevice::readyRead, this, &Controller::readAudio);
    setPhase(Listening, "Listening…");
    m_insertionTimer.start();
}
void Controller::readAudio() {
    if (!m_audioDevice || m_phase != Listening) return;
    m_audioRemainder += m_audioDevice->readAll();
    const int frameSize = m_format.bytesPerFrame(), sampleSize = m_format.bytesPerSample();
    const auto frames = m_audioRemainder.size() / frameSize;
    if (!frames) return;
    QByteArray pcm(frames * 2, Qt::Uninitialized);
    double sum = 0;
    for (qsizetype i = 0; i < frames; ++i) {
        float value = 0;
        for (int channel = 0; channel < m_format.channelCount(); ++channel)
            value += m_format.normalizedSampleValue(m_audioRemainder.constData() + i * frameSize + channel * sampleSize);
        value /= m_format.channelCount();
        if (!std::isfinite(value)) value = 0;
        sum += std::abs(value);
        const auto sample = qint16(std::clamp(value * 32767.0f, -32768.0f, 32767.0f));
        qToLittleEndian<qint16>(sample, pcm.data() + i * 2);
    }
    m_audioRemainder.remove(0, frames * frameSize);
    if (m_socket.bytesToWrite() > m_format.sampleRate() * 2 * 3) { fail("Speech server cannot keep up with microphone audio."); return; }
    m_socket.sendBinaryMessage(pcm);
    m_audioBytes += pcm.size();
    if (m_audioLogTime.elapsed() >= 5000) {
        runtimeEvent("audio.streaming", {{"bytes", m_audioBytes}, {"queued_bytes", m_socket.bytesToWrite()}});
        m_audioLogTime.restart();
    }
    m_level = std::min(1.0, sum / double(frames) * 10.0); emit changed();
}
void Controller::stopAudio() {
    if (m_audioDevice) disconnect(m_audioDevice, nullptr, this, nullptr);
    m_audioDevice = nullptr;
    if (m_audio) { m_audio->stop(); m_audio.reset(); }
    m_level = 0;
}
void Controller::stopSession() {
    m_focusDeadline.stop();
    m_shown = false; m_restart = false; m_idle.start();
    runtimeEvent("session.stop", {{"session", m_sessionId}});
    if (m_phase == Listening) {
        readAudio(); stopAudio(); setPhase(Finalizing, "Finishing…");
        m_socket.sendTextMessage("{\"type\":\"input_audio_buffer.commit\"}");
        m_deadline.start(4000);
    } else {
        m_poll.stop(); m_deadline.stop();
        setPhase(Idle, "Ready"); m_socket.abort();
        saveHistory();
    }
    emit changed();
}
void Controller::finishSession() {
    m_deadline.stop(); stopAudio();
    setPhase(Idle, "Ready"); m_socket.close();
    saveHistory();
    pumpInsertion();
}
void Controller::fail(const QString &message) {
    if (m_phase == Exiting) return;
    qWarning().noquote() << message;
    runtimeEvent("session.error", {{"message", message}, {"session", m_sessionId}});
    m_focusDeadline.stop();
    m_clipboard.cancel();
    if (m_keys.state() == QProcess::NotRunning) m_injecting = false;
    m_restart = false;
    m_lastError = message; m_poll.stop(); m_deadline.stop(); stopAudio();
    setPhase(Failed, message); m_socket.abort(); m_insertionTimer.stop();
    saveHistory();
    if (!m_shown) m_idle.start();
}
void Controller::pumpInsertion() {
    if (m_injecting || m_insertionFailed || m_phase == Failed || m_phase == Exiting) return;
    const auto desired = transcript();
    if (desired == m_inserted) {
        if (m_phase == Idle) {
            m_insertionTimer.stop();
            if (m_restart) startSession();
        }
        return;
    }
    if (m_focusId.isEmpty()) { fail("No focused application reported by KWin. Close and retry."); return; }
    const auto edit = replacementEdit(m_inserted, desired);
    // The user opted into speculative replacement: cursor/focus edits can interfere.
    // Coalesce updates while a paste is in flight, never overlap key sequences.
    QStringList keys;
    for (int i = 0; i < edit.backspaces; ++i) keys << "14:1" << "14:0";
    m_pendingText = desired; m_injectionFocus = m_focusId; m_injecting = true;
    if (edit.suffix.isEmpty()) { injectKeys(keys); return; }
    m_pendingKeys = keys;
    runtimeEvent("clipboard.publish", {{"bytes", edit.suffix.toUtf8().size()}});
    m_clipboard.publish(edit.suffix);
}
void Controller::injectKeys(const QStringList &keys) {
    runtimeEvent("insertion.start", {{"key_events", keys.size()}, {"class", m_focusClass}, {"characters", m_pendingText.size()}});
    m_keys.start("ydotool", QStringList{"key", "--key-delay", "2"} + keys);
    m_keyDeadline.start();
}
void Controller::saveHistory() {
    if (!m_started.isValid() || m_sessionId.isEmpty()) return;
    m_snapshotTimer.stop();
    QDir().mkpath(historyPath());
    QFile::setPermissions(historyPath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QSaveFile file(historyPath() + "/session-" + m_sessionId + ".json");
    if (!file.open(QIODevice::WriteOnly)) { qWarning() << "Cannot save transcript:" << file.errorString(); return; }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonArray punctuationSpans;
    for (const auto &span : m_transcript.punctuationSpans())
        punctuationSpans.append(QJsonObject{{"start", qint64(span.start)}, {"length", qint64(span.length)}, {"enabled", span.enabled}});
    const QJsonObject payload{{"started_at", m_started.toString(Qt::ISODateWithMs)},
        {"ended_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"transcript", m_transcript.rawDisplay()}, {"rendered_transcript", transcript()},
        {"punctuation_enabled", m_punctuationEnabled}, {"punctuation_rules_version", 2},
        {"punctuation_spans", punctuationSpans},
        {"committed", m_transcript.committed},
        {"partial", m_transcript.preview}, {"session", m_sessionId},
        {"status", !m_lastError.isEmpty() ? "error" : m_phase == Idle || m_phase == Exiting ? "closed" : "active"},
        {"error", m_lastError}};
    file.write(QJsonDocument(payload).toJson());
    if (!file.commit()) qWarning() << "Cannot commit transcript history:" << file.errorString();
    pruneHistory();
}
void Controller::beginExit() {
    if (m_phase == Exiting) return;
    m_focusDeadline.stop();
    m_clipboard.cancel();
    runtimeEvent("application.shutdown", {{"session", m_sessionId}});
    m_shown = false; m_idle.stop(); m_poll.stop(); m_deadline.stop(); m_insertionTimer.stop();
    stopAudio(); saveHistory(); setPhase(Exiting, "Closing…"); m_socket.abort();
    if (m_keys.state() != QProcess::NotRunning) m_keys.kill();
    if (m_backend.state() != QProcess::NotRunning) { m_backend.terminate(); m_killTimer.start(); }
    else QCoreApplication::quit();
}
void Controller::shutdown() {
    m_clipboard.cancel();
    m_phase = Exiting;
    stopAudio(); m_socket.abort();
    if (m_keys.state() != QProcess::NotRunning) { m_keys.kill(); m_keys.waitForFinished(1000); }
    if (m_backend.state() != QProcess::NotRunning) {
        m_backend.terminate();
        if (!m_backend.waitForFinished(3000)) { m_backend.kill(); m_backend.waitForFinished(1000); }
    }
    if (m_scriptId >= 0) {
        auto unload = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript");
        unload << QString("sayline-focus"); QDBusConnection::sessionBus().call(unload, QDBus::NoBlock);
        m_scriptId = -1;
    }
}
