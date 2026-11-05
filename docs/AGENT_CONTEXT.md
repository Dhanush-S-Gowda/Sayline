# AGENT_CONTEXT v1
scope=orientation; not policy. User/session/AGENTS instructions prevail. Read SESSION_STATE.md next.

product=Sayline(local live dictation), binary=sayline; target=Fedora44,KDE/Plasma6,Wayland,x86_64. impl=C++20/Qt>=6.7/QML; native-only repo; checks=C++/Bash/optional Node.
entry=src/main.cpp; flags=--toggle,--background(DBus activation; no auto-toggle),--quit,--preview,--smoke-test,--prune-history,--migrate-state(last two headless/no-model).
flow=KWin Ctrl+Space captures workspace.activeWindow.internalId -> DBus org.sayline.Sayline /Sayline ToggleOnWindow(id) -> Controller -> owned nemo-speech -> QAudioSource/WS -> Transcript -> ClipboardWriter -> ydotool. Manual Toggle uses last focus ID.

map:
 src/controller.{h,cpp}=lifecycle/audio/WS/focus/insertion/history. Phase ordinals: Idle0,WaitingForFocus1,Loading2,Connecting3,Listening4,Finalizing5,Failed6,Exiting7.
 src/transcript.*=partial/final state + Unicode-scalar prefix diff(backspaces,suffix).
 src/clipboardwriter.*=async wl-copy stdin -> wl-paste exact UTF8 readback -> ready; mismatch retry50ms,total deadline3s; failed/cancel => no keys.
 src/pastekeys.*=Zed Shift+Insert(both panes); terminal Ctrl+Shift+V; default Ctrl+V; SAYLINE_TERMINAL_CLASSES overrides terminal class list.
 src/runtime.*=JSONL logging+rotation/state paths+24h pruning.
 qml/Main.qml=460x48 translucent frameless nonfocusable drag via startSystemMove; initial KWin placement=bottom-center on captured target monitor, gap=clamp(8% usable height,64,120), applied once/surface so focus does not undo dragging. Bridge reads PlacementTarget over unique DBus owner; clientArea(KWin.MaximizeArea,target). NEVER use WorkArea: it spans all outputs.
 packaging/focus.js=temporary KWin script; placeholder owner/token/pid substituted at runtime; PID-first overlay identity; stacking/focus+BridgeEvent handshake; signals reassert keepAbove/unminimized; skipTaskbar/Pager/Switcher; all desktops.
 packaging/kwin-shortcut/=persistent tiny KWin shortcut script, DBus-activates app after idle exit.
 scripts/install-kde.sh=~/.local install; stops old+new bus owners; --migrate-state preserves history/logs; deps+ydotool preflight; invalid relocated CMake cache triggers rebuild; DBus reload+activation verification; KWin enable/reload/check; Sayline retention timer; obsolete integration removed after verification.
 CMakeLists.txt=targets/resources/install; install-time prefix generates DBus service+systemd cleanup units.

contracts:
 brand=Sayline everywhere: org.sayline.Sayline,/Sayline,sayline-shortcut,SaylineToggle,sayline-history-cleanup,SAYLINE_* options/env. Source-folder target=sayline; use pwd for actual checkout path. Previous names allowed only in explicit upgrade code/fixtures. Never overwrite colliding legacy sessions; migration preserves both.
 backend="nemo-speech serve --asr-model nemotron-en --device cpu --host 127.0.0.1 --port 18080". Refuse occupied port; NEVER kill unrelated server. Child SIGTERM then SIGKILL3s; PDEATHSIG; unmask child signals.
 Ctrl+Space closes UI immediately, stops audio, commits/finalizes<=4s; model+Qt stay warm5min from UI close; reopen cancels idle; expiry exits BOTH. GUI close handler must allow process exit.
 require authenticated KWin handshake+focused target BEFORE model/audio startup; Focus token is per-process. Never autoactivate via focus reports: use unique bus owner, not well-known name.
 WS=/v1/audio/transcriptions/realtime; /ready.ready; session.created->session.update->session.updated->PCM16 binary->delta/completed; stop=input_audio_buffer.commit->committed.
 backend delta ambiguous(suffix OR full revision, no flag). Current accumulate; authoritative completed corrects phrase. Full transcript field in delta supported. Do not invent exact revisions.
 m_injecting serializes/coalesces. NO deletion/paste before verified publication. Check focus after publication. Never restore QClipboard::setText for unfocused Wayland overlay: may leave old selection.
 no spoken text in process args; wl-copy stdin. Runtime app events use counts; backend diagnostics may contain text.
 state=$XDG_STATE_HOME/sayline else ~/.local/state/sayline; log=sayline.log,2MiB+one .1; transcripts/session-<UTC>-<UUID>.json atomic0600, partial+committed+status/error; debounced750ms; prune startup/save/minutely; installed hourly timer handles closed app(no model).
 unsupported=non-KWin desktop,macOS,Windows; Qt portability != OS integration portability. Cursor edits/focus changes during speculation remain best-effort.

commands:
 cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
 cmake --build build -j2
 ctest --test-dir build --output-on-failure
 bash tests/installer_test.sh # also included in CTest
 ./scripts/install-kde.sh # USER desktop; shared workspace build already replaces old binary
 core-only=-DSAYLINE_CORE_ONLY=ON

proof=Qt tests(protocol,storage,clipboard,lifecycle,real QJSEngine bridge,QML startup/exit)+optional Node script mock; installer Bash isolated command fixtures. No microphone/inference/input injection in tests. Fake clipboard commands isolate OS clipboard.
punctuation=implemented per user approval 2026-11-05. Overlay text/punctuation icon defaults off, persists QSettings dictation/punctuationEnabled. Six spoken commands and literal escapes rendered live. Transcript tracks per-word mode spans through ordinary partial/final revisions. Toggles affect only new words; earlier rendered punctuation survives mode-off and finalization. History includes UTF-16 punctuation_spans and rules version2. Ambiguous large ASR rewrites remain best effort. Controller transcript() is common overlay/insertion source. History retains raw plus rendered text, option, rules version. Inline hover hint replaces popup; icon has highlighted on/muted off states. See docs/PUNCTUATION_PLAN.md.
assets=sound.mp3 + sayline-icon.ico embedded; shortcut toggles request QMediaPlayer playback. Desktop theme PNG derived from ICO largest size. Installer always rebuilds even with matching cache.
bugfix=fail() releases canceled clipboard busy state when no key process is running; prevents restart getting stuck.
