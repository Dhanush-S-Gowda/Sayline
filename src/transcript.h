#pragma once
#include <QString>
#include <QList>

struct PunctuationSpan {
    qsizetype start;
    qsizetype length;
    bool enabled;
};

// The server's delta may be a suffix OR a replacement hypothesis. There is no
// flag to distinguish these. Explicit full text, when present, is authoritative.
class Transcript {
public:
    QString committed;
    QString preview;
    QString display() const;
    QString rawDisplay() const;
    void setPunctuationEnabled(bool enabled) { m_punctuationEnabled = enabled; }
    QList<PunctuationSpan> punctuationSpans() const;
    void delta(const QString &fragment);
    void snapshot(const QString &text);
    void complete(const QString &text);
    void clear();
private:
    bool m_punctuationEnabled = false;
    QList<PunctuationSpan> m_committedSpans, m_previewSpans;
};
struct TextEdit {
    int backspaces = 0;
    QString suffix;
};
// Count Unicode scalar values, rather than UTF-16 code units, for key presses.
TextEdit replacementEdit(const QString &previous, const QString &next);

// Pure formatter for command-mode text. Transcript also tracks per-word modes.
QString formatPunctuation(const QString &raw);
