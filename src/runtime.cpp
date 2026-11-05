#include "runtime.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <cstdio>

namespace {
QMutex logMutex;
QString logPath;
constexpr qint64 maxLogBytes = 2 * 1024 * 1024;
bool mergeStateDirectory(const QString &source, const QString &target, QString *error) {
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.exists()) return true;
    if (sourceInfo.isSymLink() || !sourceInfo.isDir()) {
        if (error) *error = "Refusing to migrate a non-directory or symlink: " + source;
        return false;
    }
    if (QFileInfo(target).isSymLink() || !QDir().mkpath(target)) {
        if (error) *error = "Cannot create state directory: " + target;
        return false;
    }
    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    for (const auto &entry : QDir(source).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
        auto name = entry.fileName();
        if (name.startsWith("nemodict.log")) name.replace(0, 8, "sayline");
        auto destination = target + '/' + name;
        if (entry.isSymLink()) {
            if (error) *error = "Refusing to migrate a symlink: " + entry.absoluteFilePath();
            return false;
        }
        if (entry.isDir()) {
            if (!mergeStateDirectory(entry.absoluteFilePath(), destination, error)) return false;
        } else {
            // Preserve both files on collision; never replace a newer session/log.
            if (QFileInfo::exists(destination)) {
                const QFileInfo info(destination);
                destination = info.absolutePath() + '/' + info.completeBaseName()
                    + "-migrated-" + QUuid::createUuid().toString(QUuid::WithoutBraces)
                    + (info.suffix().isEmpty() ? QString{} : '.' + info.suffix());
            }
            if (!QFile::rename(entry.absoluteFilePath(), destination)) {
                if (error) *error = "Cannot move state file: " + entry.absoluteFilePath();
                return false;
            }
            QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner);
        }
    }
    if (!QDir().rmdir(source)) {
        if (error) *error = "Cannot remove the empty legacy state directory: " + source;
        return false;
    }
    return true;
}
void append(const QString &event, const QJsonObject &details) {
    QMutexLocker guard(&logMutex);
    if (logPath.isEmpty()) return;
    if (QFile(logPath).size() >= maxLogBytes) {
        QFile::remove(logPath + ".1");
        QFile::rename(logPath, logPath + ".1");
    }
    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QJsonObject record = details;
    record.insert("time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    record.insert("pid", QCoreApplication::applicationPid());
    record.insert("event", event);
    file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
    file.flush();
}
void qtMessage(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    const char *severity = type == QtDebugMsg ? "debug" : type == QtInfoMsg ? "info"
        : type == QtWarningMsg ? "warning" : type == QtCriticalMsg ? "critical" : "fatal";
    append("qt.message", {{"severity", severity}, {"category", context.category}, {"message", message}});
    fprintf(stderr, "%s: %s\n", severity, qPrintable(message));
}
}
QString stateDirectory() {
    auto base = qEnvironmentVariable("XDG_STATE_HOME");
    if (base.isEmpty()) base = QDir::homePath() + "/.local/state";
    return base + "/sayline";
}
bool migrateLegacyState(QString *error) {
    const auto parent = QFileInfo(stateDirectory()).absolutePath();
    return mergeStateDirectory(parent + "/nemodict", stateDirectory(), error);
}
void pruneSessionHistory() {
    const QDir dir(stateDirectory() + "/transcripts");
    const auto cutoff = QDateTime::currentDateTimeUtc().addSecs(-86400);
    for (const auto &file : dir.entryInfoList({"session-*.json"}, QDir::Files)) {
        if (file.isSymLink()) continue;
        if (file.lastModified() < cutoff) QFile::remove(file.absoluteFilePath());
    }
}
void initializeRuntimeLog() {
    QString migrationError;
    const bool migrated = migrateLegacyState(&migrationError);
    const auto directory = stateDirectory();
    if (!QDir().mkpath(directory)) return;
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    logPath = directory + "/sayline.log";
    qInstallMessageHandler(qtMessage);
    runtimeEvent("application.start", {{"version", QCoreApplication::applicationVersion()}});
    if (!migrated) runtimeEvent("migration.error", {{"message", migrationError}});
}
void runtimeEvent(const QString &event, const QJsonObject &details) { append(event, details); }
