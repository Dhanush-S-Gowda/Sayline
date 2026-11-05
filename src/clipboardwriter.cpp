#include "clipboardwriter.h"

namespace {
const QString mimeType = QStringLiteral("text/plain;charset=utf-8");
}
ClipboardWriter::ClipboardWriter(QObject *parent) : QObject(parent) {
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(3000);
    m_retry.setSingleShot(true);
    m_retry.setInterval(50);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        reject("Wayland clipboard publication timed out. No paste keys were sent.");
    });
    connect(&m_retry, &QTimer::timeout, this, &ClipboardWriter::verify);
    connect(&m_process, &QProcess::started, this, [this] {
        if (m_stage == Copying) {
            m_process.write(m_expected);
            m_process.closeWriteChannel();
        }
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        const auto bytes = m_process.readAllStandardOutput();
        if (m_stage != Verifying) return;
        // A foreign clipboard can be arbitrarily large. Retain only enough bytes
        // to establish a mismatch; keep draining the helper until its deadline.
        const auto room = m_expected.size() + 1 - m_observed.size();
        if (room > 0) m_observed += bytes.left(room);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_stage != Idle && error == QProcess::FailedToStart)
            reject("Cannot start " + m_process.program() + ". Install wl-clipboard.");
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        if (m_stage == Idle) return;
        if (code != 0 || status != QProcess::NormalExit) {
            reject(m_process.program() + " failed: " + QString::fromUtf8(m_process.readAllStandardError()).left(250));
            return;
        }
        if (m_stage == Copying) {
            m_stage = Verifying;
            verify();
        } else {
            const auto remaining = m_process.readAllStandardOutput();
            const auto room = m_expected.size() + 1 - m_observed.size();
            if (room > 0) m_observed += remaining.left(room);
            if (m_observed == m_expected) {
                m_stage = Idle; m_deadline.stop();
                m_expected.clear(); m_observed.clear();
                emit ready();
            } else {
                m_retry.start();
            }
        }
    });
}
ClipboardWriter::~ClipboardWriter() {
    cancel();
    if (m_process.state() != QProcess::NotRunning) m_process.waitForFinished(1000);
}
void ClipboardWriter::publish(const QString &text) {
    if (m_stage != Idle || m_process.state() != QProcess::NotRunning) {
        reject("Clipboard publisher is busy. No paste keys were sent."); return;
    }
    m_expected = text.toUtf8(); m_observed.clear();
    m_stage = Copying;
    m_deadline.start();
    // wl-copy forks its clipboard owner only after the offer is established.
    // Read text on stdin: do not expose dictated content in process arguments.
    m_process.start("wl-copy", {"--type", mimeType});
}
void ClipboardWriter::verify() {
    if (m_stage != Verifying || m_process.state() != QProcess::NotRunning) return;
    m_observed.clear();
    m_process.start("wl-paste", {"--no-newline", "--type", mimeType});
}
void ClipboardWriter::cancel() {
    m_stage = Idle; m_deadline.stop(); m_retry.stop();
    m_expected.clear(); m_observed.clear();
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
}
void ClipboardWriter::reject(const QString &message) {
    cancel(); emit failed(message);
}
