#pragma once
#include <QObject>
#include <QAudioSource>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QAudioFormat>
#include <QDBusContext>
#include <QElapsedTimer>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <QWebSocket>
#include <memory>
#include "transcript.h"
#include "clipboardwriter.h"

class Controller : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.sayline.Sayline")
    Q_PROPERTY(bool shown READ shown NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString transcript READ transcript NOTIFY changed)
    Q_PROPERTY(bool punctuationEnabled READ punctuationEnabled WRITE setPunctuationEnabled NOTIFY changed)
    Q_PROPERTY(double level READ level NOTIFY changed)
public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller() override;
    bool shown() const { return m_shown; }
    bool busy() const { return m_phase == WaitingForFocus || m_phase == Loading || m_phase == Connecting || m_phase == Finalizing; }
    QString status() const { return m_status; }
    QString transcript() const { return m_transcript.display(); }
    bool punctuationEnabled() const { return m_punctuationEnabled; }
    Q_INVOKABLE void setPunctuationEnabled(bool enabled);
    double level() const { return m_level; }
    void initializeFocusBridge();
    void shutdown();
public slots:
    void Toggle();
    void ToggleOnWindow(const QString &windowId);
    QString PlacementTarget() const;
    void Quit();
    void Focus(const QString &id, const QString &resourceClass, const QString &token);
    void BridgeEvent(const QString &event, const QString &details, const QString &token);
signals:
    void changed();
    void shortcutSoundRequested();
private:
    friend class ControllerTest;
    enum Phase { Idle, WaitingForFocus, Loading, Connecting, Listening, Finalizing, Failed, Exiting };
    void startSession();
    void toggleForWindow(const QString &windowId);
    void acceptFocus(const QString &id, const QString &resourceClass);
    void stopSession();
    void ensureBackend();
    void pollReady();
    void openSocket();
    void handleMessage(const QString &message);
    void startAudio();
    void readAudio();
    void stopAudio();
    void finishSession();
    void fail(const QString &message);
    void pumpInsertion();
    void injectKeys(const QStringList &keys);
    void saveHistory();
    void beginExit();
    void setPhase(Phase phase, const QString &status);

    std::unique_ptr<QAudioOutput> m_soundOutput;
    std::unique_ptr<QMediaPlayer> m_soundPlayer;
    bool m_punctuationEnabled = false;
    Phase m_phase = Idle;
    bool m_shown = false, m_ready = false, m_probePending = false;
    bool m_restart = false, m_injecting = false, m_insertionFailed = false;
    QString m_status = "Ready", m_focusId, m_focusClass, m_injectionFocus;
    QString m_inserted, m_pendingText, m_lastError, m_backendLog;
    Transcript m_transcript;
    double m_level = 0;
    QDateTime m_started;
    QString m_sessionId, m_bridgeToken;
    QString m_placementTarget;
    bool m_bridgeReady = false;
    qint64 m_audioBytes = 0;
    QElapsedTimer m_audioLogTime;
    QProcess m_backend, m_keys;
    ClipboardWriter m_clipboard;
    QStringList m_pendingKeys;
    QWebSocket m_socket;
    QNetworkAccessManager m_network;
    QTimer m_poll, m_deadline, m_idle, m_insertionTimer, m_keyDeadline, m_killTimer, m_focusDeadline, m_snapshotTimer, m_historyTimer;
    QElapsedTimer m_loadTime;
    std::unique_ptr<QAudioSource> m_audio;
    QIODevice *m_audioDevice = nullptr;
    QAudioFormat m_format;
    QByteArray m_audioRemainder;
    QTemporaryFile m_focusScript;
    int m_scriptId = -1;
};
