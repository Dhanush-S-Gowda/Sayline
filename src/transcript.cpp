#include "transcript.h"
#include <QList>
#include <QRegularExpression>

namespace {
struct Word {
    QString text;
    PunctuationSpan span;
};
QList<Word> words(const QString &text, bool enabled) {
    static const QRegularExpression word(QStringLiteral(R"([\p{L}\p{M}\p{N}_]+)"));
    QList<Word> result;
    auto matches = word.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result.append({match.captured().toCaseFolded(), {match.capturedStart(), match.capturedLength(), enabled}});
    }
    return result;
}
// Align raw words, ignoring casing/ASR punctuation, to carry toggle history into
// revised hypotheses and the authoritative completion. Inserted trailing speech
// uses the current mode; corrections inherit the words they replace.
QList<PunctuationSpan> reconcileModes(const QString &previous, const QList<PunctuationSpan> &previousSpans,
                                     const QString &next, bool enabled) {
    auto oldWords = words(previous, enabled);
    auto newWords = words(next, enabled);
    for (qsizetype i = 0; i < oldWords.size() && i < previousSpans.size(); ++i)
        oldWords[i].span.enabled = previousSpans[i].enabled;
    qsizetype prefix = 0;
    while (prefix < oldWords.size() && prefix < newWords.size()
           && oldWords[prefix].text == newWords[prefix].text) {
        newWords[prefix].span.enabled = oldWords[prefix].span.enabled;
        ++prefix;
    }
    qsizetype oldEnd = oldWords.size(), newEnd = newWords.size();
    while (oldEnd > prefix && newEnd > prefix && oldWords[oldEnd - 1].text == newWords[newEnd - 1].text) {
        newWords[--newEnd].span.enabled = oldWords[--oldEnd].span.enabled;
    }
    const auto oldCount = oldEnd - prefix, newCount = newEnd - prefix;
    // Bound memory/work for pathological full revisions of very long speech.
    constexpr qsizetype maxCells = 1000000;
    if (oldCount && newCount && oldCount <= maxCells / newCount) {
        const auto columns = newCount + 1;
        QList<int> lcs((oldCount + 1) * columns, 0);
        for (qsizetype i = oldCount; i-- > 0;)
            for (qsizetype j = newCount; j-- > 0;)
                lcs[i * columns + j] = oldWords[prefix + i].text == newWords[prefix + j].text
                    ? 1 + lcs[(i + 1) * columns + j + 1]
                    : qMax(lcs[(i + 1) * columns + j], lcs[i * columns + j + 1]);
        qsizetype i = 0, j = 0;
        while (j < newCount) {
            if (i < oldCount && oldWords[prefix + i].text == newWords[prefix + j].text) {
                newWords[prefix + j++].span.enabled = oldWords[prefix + i++].span.enabled;
            } else if (i < oldCount && lcs[(i + 1) * columns + j] > lcs[i * columns + j + 1]) {
                ++i;
            } else {
                if (i < oldCount) newWords[prefix + j].span.enabled = oldWords[prefix + i].span.enabled;
                ++j;
            }
        }
    } else {
        for (qsizetype j = 0; j < qMin(oldCount, newCount); ++j)
            newWords[prefix + j].span.enabled = oldWords[prefix + j].span.enabled;
    }
    QList<PunctuationSpan> result;
    result.reserve(newWords.size());
    for (const auto &word : newWords) result.append(word.span);
    return result;
}
QString renderPunctuation(const QString &raw, const QList<PunctuationSpan> *spans);
}

QString Transcript::rawDisplay() const {
    return committed + ((!committed.isEmpty() && !preview.isEmpty()) ? " " : "") + preview;
}
QList<PunctuationSpan> Transcript::punctuationSpans() const {
    auto spans = m_committedSpans;
    const auto offset = committed.size() + ((!committed.isEmpty() && !preview.isEmpty()) ? 1 : 0);
    for (auto span : m_previewSpans) { span.start += offset; spans.append(span); }
    return spans;
}
QString Transcript::display() const {
    const auto spans = punctuationSpans();
    return renderPunctuation(rawDisplay(), &spans);
}
void Transcript::delta(const QString &fragment) { snapshot(preview + fragment); }
void Transcript::snapshot(const QString &text) {
    m_previewSpans = reconcileModes(preview, m_previewSpans, text, m_punctuationEnabled);
    preview = text;
}
void Transcript::complete(const QString &text) {
    const auto phrase = text.trimmed();
    const auto spans = reconcileModes(preview, m_previewSpans, phrase, m_punctuationEnabled);
    if (!phrase.isEmpty()) {
        if (!committed.isEmpty()) committed += ' ';
        for (auto span : spans) { span.start += committed.size(); m_committedSpans.append(span); }
        committed += phrase;
    }
    preview.clear(); m_previewSpans.clear();
}
void Transcript::clear() {
    committed.clear(); preview.clear(); m_committedSpans.clear(); m_previewSpans.clear();
}
TextEdit replacementEdit(const QString &previous, const QString &next) {
    const auto oldPoints = previous.toUcs4();
    const auto newPoints = next.toUcs4();
    qsizetype common = 0;
    while (common < oldPoints.size() && common < newPoints.size()
           && oldPoints[common] == newPoints[common]) ++common;
    qsizetype utf16Prefix = 0;
    for (qsizetype i = 0; i < common; ++i) utf16Prefix += newPoints[i] > 0xffff ? 2 : 1;
    return {int(oldPoints.size() - common), next.mid(utf16Prefix)};
}

namespace {
QString renderPunctuation(const QString &raw, const QList<PunctuationSpan> *spans) {
    static const QRegularExpression command(
        QStringLiteral(R"((?<![\p{L}\p{N}_])(?:(literal)\s+)?(question\s+mark|exclamation\s+mark|full\s+stop|semicolon|comma|colon)(?![\p{L}\p{N}_]))"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    QString output;
    qsizetype cursor = 0;
    qsizetype spanIndex = 0;
    auto matches = command.globalMatch(raw);
    while (matches.hasNext()) {
        const auto match = matches.next();
        output += raw.mid(cursor, match.capturedStart() - cursor);
        cursor = match.capturedEnd();
        bool enabled = true;
        if (spans) {
            while (spanIndex < spans->size() && spans->at(spanIndex).start < match.capturedStart()) ++spanIndex;
            auto index = spanIndex;
            while (index < spans->size() && spans->at(index).start < match.capturedEnd())
                enabled = spans->at(index++).enabled && enabled;
        }
        if (!enabled) { output += match.captured(); continue; }
        if (!match.captured(1).isEmpty()) {
            output += match.captured(2); // Escaped output is never matched again.
            continue;
        }
        const auto phrase = match.captured(2).toLower();
        const QChar symbol = phrase.startsWith("question") ? '?' : phrase.startsWith("exclamation") ? '!'
            : phrase.startsWith("full") ? '.' : phrase == "comma" ? ',' : phrase == "colon" ? ':' : ';';
        while (!output.isEmpty() && output.back().isSpace()) output.chop(1);
        output += symbol;
        // NeMo may add punctuation immediately after a spoken command.
        while (cursor < raw.size() && QStringLiteral(".,?!:;").contains(raw[cursor])) ++cursor;
        const auto spaceStart = cursor;
        while (cursor < raw.size() && raw[cursor].isSpace()) ++cursor;
        if (cursor > spaceStart && cursor < raw.size()) output += ' ';
    }
    output += raw.mid(cursor);
    return output;
}

}
QString formatPunctuation(const QString &raw) { return renderPunctuation(raw, nullptr); }
