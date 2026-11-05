#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
install_prefix="${SAYLINE_INSTALL_PREFIX:-$HOME/.local}"
if [[ "$install_prefix" != "$HOME/.local" ]]; then
    printf '%s\n' 'KDE integration currently requires the default ~/.local installation prefix.' >&2
    exit 1
fi
qdbus_bin=""
for candidate in qdbus6 qdbus /usr/lib64/qt6/bin/qdbus /usr/lib/qt6/bin/qdbus; do
    if command -v "$candidate" >/dev/null 2>&1; then qdbus_bin="$candidate"; break; fi
done
if [[ -z "$qdbus_bin" ]] || ! command -v kwriteconfig6 >/dev/null 2>&1; then
    printf '%s\n' 'This installer requires Plasma 6, Qt qdbus, and kwriteconfig6.' >&2
    exit 1
fi
for dependency in cmake nemo-speech ydotool wl-copy wl-paste systemctl; do
    if ! command -v "$dependency" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$dependency" >&2
        exit 1
    fi
done
if ! ydotool key --help >/dev/null 2>&1; then
    printf '%s\n' 'ydotool cannot connect to its daemon. Configure ydotoold and its socket permissions first.' >&2
    exit 1
fi
# CMake caches embed the absolute source path. Rebuild after a folder move.
cache_home=""
if [[ -f "$project_dir/build/CMakeCache.txt" ]]; then
    while IFS= read -r line; do
        if [[ "$line" == CMAKE_HOME_DIRECTORY:INTERNAL=* ]]; then cache_home="${line#*=}"; break; fi
    done < "$project_dir/build/CMakeCache.txt"
fi
if [[ "$cache_home" != "$project_dir" || ! -x "$project_dir/build/sayline" ]]; then
    if [[ -d "$project_dir/build" && "$cache_home" != "$project_dir" ]]; then
        cmake -E remove_directory "$project_dir/build"
    fi
fi
# Always check source/resource changes, even when a cached binary exists.
cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_dir/build" -j2
stop_instance() {
    local service="$1" object_path="$2" method="$3"
    if [[ "$("$qdbus_bin" org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner "$service")" != true ]]; then return; fi
    "$qdbus_bin" "$service" "$object_path" "$method"
    for ((attempt=0; attempt<60; attempt++)); do
        if [[ "$("$qdbus_bin" org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner "$service")" == false ]]; then return; fi
        sleep 0.1
    done
    printf 'The previous instance (%s) has not stopped. Close it before reinstalling.\n' "$service" >&2
    exit 1
}
# Compatibility is confined to this upgrade block; active integration uses Sayline.
stop_instance org.nemodict.NeMoDict /NeMoDict org.nemodict.NeMoDict.Quit
stop_instance org.sayline.Sayline /Sayline org.sayline.Sayline.Quit
"$project_dir/build/sayline" --migrate-state
cmake --install "$project_dir/build" --prefix "$install_prefix" --strip
"$qdbus_bin" org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ReloadConfig
activatable_names="$("$qdbus_bin" org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListActivatableNames)"
activation_found=false
while IFS= read -r service_name; do
    if [[ "$service_name" == org.sayline.Sayline ]]; then activation_found=true; break; fi
done <<< "$activatable_names"
if [[ "$activation_found" != true ]]; then
    printf '%s\n' 'Installation copied the files, but D-Bus has not registered Sayline for activation.' \
        'Ctrl+Space cannot start the app yet. Share this output and your XDG_DATA_HOME setting.' >&2
    exit 1
fi
# Release the previous shortcut before registering Sayline's action.
"$qdbus_bin" org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript nemodict-shortcut >/dev/null
"$qdbus_bin" org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript nemodict-focus >/dev/null
kwriteconfig6 --file kwinrc --group Plugins --key nemodict-shortcutEnabled --delete
kwriteconfig6 --file kglobalshortcutsrc --group kwin --key NeMoDictToggle --delete
kwriteconfig6 --file kwinrc --group Plugins --key sayline-shortcutEnabled true
"$qdbus_bin" org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript sayline-shortcut >/dev/null
"$qdbus_bin" org.kde.KWin /KWin org.kde.KWin.reconfigure
script_loaded=false
for ((attempt=0; attempt<20; attempt++)); do
    if [[ "$("$qdbus_bin" org.kde.KWin /Scripting org.kde.kwin.Scripting.isScriptLoaded sayline-shortcut)" == true ]]; then
        script_loaded=true; break
    fi
    sleep 0.1
done
if [[ "$script_loaded" != true ]]; then
    printf '%s\n' 'D-Bus activation is registered, but KWin did not load the Sayline shortcut script.' >&2
    exit 1
fi
systemctl --user disable --now nemodict-history-cleanup.timer >/dev/null 2>&1 || true
# Delete only known installation artifacts after the new integration verifies.
for obsolete in bin/nemodict share/applications/org.nemodict.NeMoDict.desktop \
    share/dbus-1/services/org.nemodict.NeMoDict.service \
    share/systemd/user/nemodict-history-cleanup.service share/systemd/user/nemodict-history-cleanup.timer; do
    unlink -- "$install_prefix/$obsolete" 2>/dev/null \
        || [[ ! -e "$install_prefix/$obsolete" && ! -L "$install_prefix/$obsolete" ]]
done
cmake -E remove_directory "$install_prefix/share/kwin/scripts/nemodict-shortcut"
"$qdbus_bin" org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ReloadConfig
systemctl --user daemon-reload
systemctl --user enable --now sayline-history-cleanup.timer
printf '%s\n' 'Installed: Sayline activation, shortcut, history migration, and cleanup timer are configured.' \
    'If Ctrl+Space is assigned to another app, assign it to KWin → Sayline: toggle dictation.'
