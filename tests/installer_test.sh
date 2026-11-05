#!/usr/bin/env bash
# Isolated installer fixtures; never contact the desktop or write installed files.
set -euo pipefail
source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
fixture="$(mktemp -d /tmp/sayline-installer-test.XXXXXX)"
trap '/usr/bin/rm -rf -- "$fixture"' EXIT
mkdir -p "$fixture/scripts" "$fixture/bin" "$fixture/build"
cp "$source_dir/scripts/install-kde.sh" "$fixture/scripts/install-kde.sh"
cat > "$fixture/bin/qdbus6" <<'MOCK'
#!/usr/bin/env bash
set -eu
printf '%s\n' "$3" >> "$SAYLINE_TEST_LOG"
case "$3" in
    org.freedesktop.DBus.NameHasOwner)
        if [[ "$SAYLINE_TEST_MODE" == migrate && "$4" == org.nemodict.NeMoDict && ! -f "$SAYLINE_TEST_LOG.stopped" ]]; then echo true; else echo false; fi ;;
    org.nemodict.NeMoDict.Quit) touch "$SAYLINE_TEST_LOG.stopped" ;;
    org.freedesktop.DBus.ListActivatableNames)
        echo org.freedesktop.DBus
        if [[ "$SAYLINE_TEST_MODE" != missing ]]; then echo org.sayline.Sayline; fi ;;
    org.kde.kwin.Scripting.isScriptLoaded) echo true ;;
esac
MOCK
chmod +x "$fixture/bin/qdbus6"
for command_name in cmake kwriteconfig6 nemo-speech ydotool wl-copy wl-paste systemctl; do
    printf '#!/bin/sh\nexit 0\n' > "$fixture/bin/$command_name"
    chmod +x "$fixture/bin/$command_name"
done
cat > "$fixture/build/sayline" <<'MOCK'
#!/usr/bin/env bash
[[ "$1" == --migrate-state ]]
printf '%s\n' state-migration >> "$SAYLINE_TEST_LOG"
MOCK
chmod +x "$fixture/build/sayline"
cat > "$fixture/bin/cmake" <<'MOCK'
#!/usr/bin/env bash
printf 'cmake %s\n' "$*" >> "$SAYLINE_TEST_LOG"
MOCK
chmod +x "$fixture/bin/cmake"
cat > "$fixture/bin/unlink" <<'MOCK'
#!/usr/bin/env bash
# Never remove a real installed binary from this fixture.
printf '%s\n' remove-old-executable >> "$SAYLINE_TEST_LOG"
MOCK
chmod +x "$fixture/bin/unlink"
export PATH="$fixture/bin:$PATH"
export SAYLINE_TEST_LOG="$fixture/calls"
unset SAYLINE_INSTALL_PREFIX

export SAYLINE_TEST_MODE=registered
bash "$fixture/scripts/install-kde.sh" > "$fixture/output" 2> "$fixture/error"
reload_line=0
list_line=0
line_number=0
while IFS= read -r method; do
    ((line_number+=1))
    case "$method" in
        org.freedesktop.DBus.ReloadConfig) if [[ "$reload_line" == 0 ]]; then reload_line="$line_number"; fi ;;
        org.freedesktop.DBus.ListActivatableNames) list_line="$line_number" ;;
    esac
done < "$SAYLINE_TEST_LOG"
[[ "$reload_line" -gt 0 && "$reload_line" -lt "$list_line" ]]
output="$(cat "$fixture/output")"
[[ "$output" == *'Installed: Sayline activation'* ]]
calls="$(cat "$SAYLINE_TEST_LOG")"
[[ "$calls" == *'org.kde.kwin.Scripting.isScriptLoaded'* ]]
[[ "$calls" == *'remove-old-executable'* ]]
printf '%s\n' 'PASS: reload before activation discovery; verify KWin script.'
# Matching cache and existing binary must still rebuild changed sources/assets.
printf 'CMAKE_HOME_DIRECTORY:INTERNAL=%s\n' "$fixture" > "$fixture/build/CMakeCache.txt"
: > "$SAYLINE_TEST_LOG"
bash "$fixture/scripts/install-kde.sh" > "$fixture/output" 2> "$fixture/error"
calls="$(cat "$SAYLINE_TEST_LOG")"
[[ "$calls" == *'cmake --build '* ]]
printf '%s\n' 'PASS: existing cached binary still triggers a build.'


: > "$SAYLINE_TEST_LOG"
export SAYLINE_TEST_MODE=missing
if bash "$fixture/scripts/install-kde.sh" > "$fixture/output" 2> "$fixture/error"; then
    printf '%s\n' 'FAIL: installer accepted missing activation.' >&2
    exit 1
fi
error="$(cat "$fixture/error")"
output="$(cat "$fixture/output")"
calls="$(cat "$SAYLINE_TEST_LOG")"
[[ "$error" == *'has not registered Sayline'* ]]
[[ "$output" != *'Installed:'* ]]
[[ "$calls" != *'org.kde.KWin.reconfigure'* ]]
[[ "$calls" != *'remove-old-executable'* ]]
printf '%s\n' 'PASS: missing activation fails before claiming success.'

: > "$SAYLINE_TEST_LOG"
export SAYLINE_TEST_MODE=migrate
bash "$fixture/scripts/install-kde.sh" > "$fixture/output" 2> "$fixture/error"
calls="$(cat "$SAYLINE_TEST_LOG")"
[[ "$calls" == *'org.nemodict.NeMoDict.Quit'* ]]
[[ "$calls" == *'state-migration'* ]]
[[ "$calls" == *'cmake -S '* ]]
[[ "$calls" == *'remove-old-executable'* ]]
printf '%s\n' 'PASS: stop the previous app through its stable bus name before migration.'
