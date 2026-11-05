#include "pastekeys.h"

QStringList pasteKeysForApp(const QString &resourceClass) {
    const auto name = resourceClass.toLower();
    // Zed binds Shift+Insert to editor::Paste AND terminal::Paste. Ctrl+Shift+V
    // opens previews in some editor contexts, so do not use it for all of Zed.
    if (name == "zed" || name == "dev.zed.zed" || name == "dev.zed.zed-preview")
        return {"42:1", "110:1", "110:0", "42:0"};
    const QStringList terminals = {"konsole", "org.kde.konsole", "kitty", "alacritty", "foot",
        "footclient", "wezterm", "org.wezfurlong.wezterm", "gnome-terminal-server",
        "org.gnome.terminal", "org.gnome.ptyxis", "com.mitchellh.ghostty", "ghostty",
        "org.gnome.console", "tilix", "terminator", "xterm", "urxvt"};
    const auto overrides = qEnvironmentVariable("SAYLINE_TERMINAL_CLASSES").split(',', Qt::SkipEmptyParts);
    if (terminals.contains(name) || overrides.contains(name, Qt::CaseInsensitive))
        return {"29:1", "42:1", "47:1", "47:0", "42:0", "29:0"};
    return {"29:1", "47:1", "47:0", "29:0"};
}
