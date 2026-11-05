# Sayline — C++ / Qt Quick

A small, rounded, movable dictation overlay for Fedora KDE Plasma on Wayland.
The application is C++20 with a Qt Quick/QML interface. Speech recognition remains
local using the existing `nemo-speech` executable and `nemotron-en` on CPU.

## Behavior

- The installed KDE **Ctrl+Space** shortcut sends a D-Bus toggle. D-Bus starts
  `sayline --background` when needed, including after idle shutdown.
- The first press starts the Qt app and its own `nemo-speech` process on port 18080.
  The overlay shows loading activity and elapsed time until `/ready` succeeds.
  The backend does not expose a loading percentage, so progress is indeterminate.
- Recording waits for a successful KWin focus-bridge handshake and a target
  window. Once ready, the microphone streams to NeMo. Audio waves and live text appear
  side by side. Drag anywhere on the overlay to move it through KWin's native
  window move operation. It does not request keyboard focus. The compact 460 × 48 px
  window uses system colors, a subtle shaded rim, and a small shadow. Its default
  is bottom-center, 8% of the usable screen height above the bottom edge (64–120
  logical pixels), on the monitor containing the application focused when you
  press Ctrl+Space. The shortcut captures that window before starting Sayline;
  placement uses that monitor's usable bounds, rather than the combined desktop.
  KWin handles placement on Wayland;
  switching focus does not undo dragging. KWin keeps it
  above other apps and off the taskbar/Alt+Tab list, across virtual desktops.
- Partial text is pasted into the focused application as it arrives. Revised text
  is applied with backspaces followed by a paste. KWin reports the focused app;
  Clipboard publication uses `wl-copy` followed by an exact `wl-paste` readback
  before any correction/paste keys are sent; an unverified clipboard stops insertion.
  Known terminals receive **Ctrl+Shift+V**, Zed receives **Shift+Insert** (supported
  in both its editor and embedded terminal), and other apps receive **Ctrl+V**.
- A second press immediately hides the overlay and stops recording. The last
  audio is committed with a four-second finalization limit. Its final correction
  can still arrive after the overlay closes.
- The app and model remain available for **five minutes after closing the UI**.
  Reopening cancels that timer and reuses the loaded model. After five idle
  minutes, the owned speech process is terminated (forcibly after three seconds
  if necessary), then the Qt app exits. No Sayline background process remains.
  The next shortcut invocation starts both again.
- Closing during model loading also starts the five-minute idle timer. The
  already-started model can finish loading in the background during that interval.
- Normal termination cleans up the speech process. Linux parent-death handling
  also kills the owned backend if the Qt app crashes or is forcibly killed.

**Do not start `nemo-speech serve` separately.** Sayline runs exactly this command:

```sh
nemo-speech serve --asr-model nemotron-en --device cpu --host 127.0.0.1 --port 18080
```

If port 18080 is occupied, Sayline reports an error. It does not attach to or
terminate a process it did not start. Stop your previously launched server before
using the C++ app.

## Build on Fedora

```sh
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qtmultimedia-devel qt6-qtwebsockets-devel ydotool wl-clipboard
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Keep `nemo-speech` on PATH with `nemotron-en` already available. Set up `ydotoold`
for your user and ensure `YDOTOOL_SOCKET` points to its socket if it is not the
user default. The daemon needs `/dev/uinput` access. Do not run Sayline as root.
Qt Multimedia uses the system's default microphone; choose it in KDE Sound settings.
Sayline prefers 16 kHz mono PCM16 and can convert the microphone's native PCM
format and downmix channels; NeMo resamples supported 8–96 kHz input.

Preview the UI without microphone capture, typing, or loading the model:

```sh
./build/sayline --preview
```

Run the application:

```sh
./build/sayline --toggle
```

From another terminal, the same command toggles it off. To stop both processes
immediately:

```sh
./build/sayline --quit
```

## Install or update the KDE shortcut

After building and testing, run from this project directory:

```sh
./scripts/install-kde.sh
```

No sudo is needed. This stops the old instance, replaces the executable in
`~/.local/bin/sayline`, installs the D-Bus activation service and a small KWin
shortcut script, enables it, checks that ydotool can reach its daemon, installs an hourly transcript
cleanup timer, reloads D-Bus activation files and KWin, and verifies that D-Bus
recognizes Sayline and KWin has loaded the script. Press **Ctrl+Space** to
start dictating. The script remains in KWin when the app exits; it consumes no
separate process and holds no speech model in memory.

Remove any old manual Sayline command shortcut for Ctrl+Space to avoid a
conflict. If needed, open **KDE System Settings → Keyboard → Shortcuts → KWin**
and assign Ctrl+Space to **Sayline: toggle dictation**. The script is also listed
under **Window Management → KWin Scripts → Sayline shortcut**.

The executable still supports `--toggle` for manual use. To stop an installed
instance immediately, use `~/.local/bin/sayline --quit`.

Rebuilding uses the existing build directory and replaces its previous binary;
it does not keep a second version. Re-running the installer also replaces the
installed copy. You do not need to uninstall before updating.

Sayline uses its own names throughout: `org.sayline.Sayline`, `/Sayline`,
`sayline-shortcut`, `sayline-history-cleanup.timer`, and the state paths below.
The installer stops previous versions, moves their logs/transcripts without
overwriting existing files, and removes their old desktop integration after
verifying the replacement. Old names occur only in explicit upgrade code/tests.
If the source folder was moved, the installer detects the outdated CMake cache
and rebuilds automatically before installation.

After installation, these directories are optional disk cleanup (run from this
project root):

```sh
rm -rf build-core
```

The separate `build-core` directory is only needed for the smaller test build.
You may also remove `build` after installing
if you want to remove the compiler outputs; rebuild before your next update.
Keep the source files and your nemo-speech executable/model cache.

## Supported platforms

The complete app currently targets **Linux with KDE Plasma 6 / KWin** and Qt 6.7+.
Other distributions running Plasma 6 can build it with their matching Qt packages,
`nemo-speech`, and `ydotoold`; they have not been tested here. Distribution-specific
package names and daemon setup differ.

GNOME, Sway, Hyprland and other desktops need replacement shortcut/focus/stacking
integrations. This version relies on KWin and is not a general Linux desktop build.

**macOS and Windows are not supported by this code as written.** Qt/QML and the
transcript logic are reusable, but D-Bus/KWin, ydotool, Linux signal handling, and
parent-death cleanup need platform-specific replacements. Supporting those systems
requires additional implementation; a different build command is insufficient.

## Live insertion limits

You explicitly selected speculative live insertion. The installed NeMo protocol
sometimes sends a suffix and sometimes an entire replacement hypothesis in the
same `delta` field, with no discriminator. This client accumulates those deltas,
then corrects the entire phrase against the authoritative `completed` event.
Intermediate words can therefore temporarily repeat or be wrong. Exact partial
revision requires a backend protocol extension; a full `transcript` field in a
partial event is already supported by the client when supplied.

Keep the caret in the intended field. Typing manually, selecting text, or moving
the caret while dictating can cause corrections to erase unrelated text. Focus
can also change where the next update is inserted. A focus change during an
in-flight clipboard offer stops dictation with an error. The current implementation
cannot identify caret moves or distinguish individual panes inside an editor.
Zed is handled using Shift+Insert because its default Linux keymap binds this to
paste in both the editor and terminal. If you customized those bindings, restore
the paste action for Shift+Insert in both contexts. Other editors' embedded
terminals may need an app-specific binding that works in both contexts; simply
classifying the whole editor as a terminal can change its text-pane behavior.

Paste shortcuts must retain their usual bindings in the target app. For terminal
window classes not in the built-in list, start the app with a comma-separated
list (class matching is case-insensitive):

```sh
SAYLINE_TERMINAL_CLASSES=my-terminal,another-terminal ./build/sayline --toggle
```

Clipboard insertion replaces the clipboard contents, and clipboard history may
record dictated fragments. Key delivery and clipboard consumption are best-effort
across arbitrary Wayland applications; real-desktop validation is required for
your editors, browser, terminal, keyboard layout, and clipboard manager.

## Runtime log and session transcripts

The app automatically writes runtime events to:

```text
~/.local/state/sayline/sayline.log
```

This is a timestamped JSON-lines `.log` file covering startup, shortcut toggles,
state transitions, KWin bridge startup/handshake and focus reports, overlay
visibility, model diagnostics, microphone streaming, ASR event types, paste
attempts, errors, and shutdown. App events record text lengths rather than the
spoken text; backend diagnostics are also included. The log rotates at 2 MB,
keeping one `.1` backup. Files are private (mode 0600).

Each dictation session gets one file under:

```text
~/.local/state/sayline/transcripts/session-<timestamp>-<id>.json
```

It is updated atomically during recording, on each finalized utterance, and on
stop/error. It includes the complete displayed transcript, committed text,
unfinished partial text, session ID, timestamps, status, and any error. An early
failure therefore leaves a session record even if no finalized sentence arrived.
Partial snapshots are debounced to 750 ms rather than writing every audio frame.

Files older than 24 hours are removed at app startup, on save, once a minute while
the app runs, and hourly by the installed user timer while the app is closed.
The hourly schedule means a file can remain until the next cleanup after its
24-hour age limit. The cleanup command does not initialize a GUI or speech model,
and no Sayline process stays running between checks. When your user session is
off, cleanup resumes when the user timer next runs.

Both locations honor `$XDG_STATE_HOME`. To watch diagnostics:

```sh
tail -f ~/.local/state/sayline/sayline.log
```

No additional shortcut or window-rule setup should be needed after the installer
succeeds. Fedora's ydotoold/uinput access is a system prerequisite; the installer
reports missing daemon access rather than silently installing a non-working app.
It does not change privileged device permissions. If dictation fails, share the
recent runtime log so the bridge, ASR, and paste failures can be distinguished.

## Verification and migration

The pure C++ transcript tests can run without the missing desktop development
packages:

```sh
cmake -S . -B build-core -DSAYLINE_CORE_ONLY=ON
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure
```

They cover partial/final reconciliation, correction boundaries, deletion-only
updates, no-op updates, supplementary Unicode characters, and Zed/terminal/browser
paste key selection. The full test suite also checks process lifecycle, QML
startup and exit, and KWin script policy (when Node.js is available for testing). `--smoke-test`
loads the real overlay for 700 ms and exits without recording or typing; for a
headless environment use `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`.

The repository contains the native C++/QML app and C++/Bash/JavaScript checks.

Before daily use, verify on the actual desktop:

1. Ctrl+Space opens without moving keyboard focus; dragging works.
2. Partial text and final corrections reach a text editor and browser input.
3. Konsole receives Ctrl+Shift+V; other app shortcuts remain appropriate.
4. Close and reopen within five minutes: the same model process is reused.
5. Close and wait five minutes: both `sayline` and its `nemo-speech` child exit.
6. Press Ctrl+Space again: loading feedback appears and both processes restart.

The build, automated checks and staged installer have been verified. This execution
environment cannot connect to the real KDE session bus, so actual global shortcut
delivery, compositor stacking, native dragging, and typing into Zed must be checked
on your desktop after installation.

References: [NeMo API](https://github.com/NVIDIA/NeMo-Speech.cpp/blob/main/docs/api.md),
[NeMo server](https://github.com/NVIDIA/NeMo-Speech.cpp/blob/main/docs/server.md),
[KWin scripting](https://develop.kde.org/docs/plasma/kwin/api/),
[Qt audio capture](https://doc.qt.io/qt-6/qaudiosource.html).

## Agent handoff

Compact code map/invariants: [docs/AGENT_CONTEXT.md](docs/AGENT_CONTEXT.md).
Current version/evidence/remaining validation: [docs/SESSION_STATE.md](docs/SESSION_STATE.md).

Spoken punctuation is opt-in: click the **text-and-punctuation icon** at the right
of the overlay. A highlighted background means on; a muted background means off.
The button starts off and remembers your choice. Commands convert live: “comma”,
“full stop”, “question mark”, “exclamation mark”, “colon”, and “semicolon”.
For example, “Hello comma world full stop” becomes “Hello, world.” Say “literal
comma” to keep the word “comma”. Switching off preserves earlier punctuation and
leaves new command words literal, including when recording is stopped and finalized.
A word already in progress retains its original mode. The hover hint appears beside
the button so it cannot cover the click target. Existing ASR automatic punctuation
continues independently. See [docs/PUNCTUATION_PLAN.md](docs/PUNCTUATION_PLAN.md)
for recognition revision limits.

Ctrl+Space plays the bundled `sound.mp3` on both start and stop. Playback uses Qt's
[media player](https://doc.qt.io/qt-6/qmediaplayer.html); playback errors are logged
without interrupting dictation. The desktop launcher and application window use
`sayline-icon.ico` (installed as a PNG for the desktop icon theme).

After updating this checkout, run `./scripts/install-kde.sh` in your desktop session
to update the installed executable, icon, and KDE integration. The installer now
always rebuilds changed sources and resources before installing.
