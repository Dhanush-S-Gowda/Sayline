#pragma once
#include <QString>
#include <QJsonObject>

QString stateDirectory();
bool migrateLegacyState(QString *error = nullptr);
void initializeRuntimeLog();
void pruneSessionHistory();
void runtimeEvent(const QString &event, const QJsonObject &details = {});
