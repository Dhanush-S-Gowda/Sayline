#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

// Serialized asynchronous publication; ready means an independent Wayland reader
// returned exactly the bytes supplied. No keyboard events belong in this class.
class ClipboardWriter : public QObject {
    Q_OBJECT
public:
    explicit ClipboardWriter(QObject *parent = nullptr);
    void publish(const QString &text);
    void cancel();
    ~ClipboardWriter() override;
signals:
    void ready();
    void failed(const QString &message);
private:
    friend class ClipboardWriterTest;
    enum Stage { Idle, Copying, Verifying };
    void verify();
    void reject(const QString &message);
    Stage m_stage = Idle;
    QProcess m_process;
    QTimer m_deadline, m_retry;
    QByteArray m_expected, m_observed;
};
