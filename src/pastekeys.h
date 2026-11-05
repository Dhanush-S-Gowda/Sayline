#pragma once
#include <QStringList>

// Window-class policy: embedded terminals share a class with their editor.
QStringList pasteKeysForApp(const QString &resourceClass);
