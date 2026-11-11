<h1 align="center">
  <a href="https://dhanush-s-gowda.github.io/Sayline/">
    <img src="packaging/icons/org.sayline.Sayline.png" alt="Sayline logo" width="64" height="64" align="absmiddle">
  </a>
  Sayline
</h1>

<p align="center">
  <strong>Give your keyboard a breather.</strong><br>
  Live, local voice typing for KDE Plasma on Wayland.
</p>

<p align="center">
  <a href="https://dhanush-s-gowda.github.io/Sayline/">
    <img src="https://img.shields.io/badge/Website-Try%20the%20demo%20%E2%86%97-2563EB?style=for-the-badge" alt="Website: try the interactive demo" height="28">
  </a>
  &nbsp;
  <a href="#quick-start">
    <img src="https://img.shields.io/badge/Get%20started-Install%20Sayline-1F2937?style=for-the-badge" alt="Get started: install Sayline" height="28">
  </a>
</p>

<p align="center">
  Press <kbd>Ctrl</kbd> + <kbd>Space</kbd>, speak, and your words appear in the app you're using.<br>
  Powered by NVIDIA Nemotron through NeMo-Speech.cpp, running locally on your computer.
</p>

<p align="center">
  <img src="docs/Screenshot.png" alt="Sayline's compact dictation overlay showing a blue waveform and live transcript" width="360"><br>
  <sub>A small, movable overlay that keeps your app in focus.</sub>
</p>

---

## Features

- **Live transcription**: text updates as you speak, with corrections as
  recognition results are refined.
- **Local recognition**: CPU inference with the English `nemotron-en` model.
- **An unobtrusive overlay**: move it where you like while your app keeps
  keyboard focus.
- **Spoken punctuation**: optionally dictate comma, full stop, question mark,
  exclamation mark, colon, and semicolon.
- **Desktop integration**: start and stop with a KDE shortcut and revisit
  previous dictation sessions in your history.

## Quick start

Sayline is tested on **Fedora KDE Plasma 6 / Wayland**. Other distributions can
work with Plasma 6 / KWin on Wayland when they provide Qt 6.7 or newer and the
dependencies below. Their installation paths have not been verified yet.

### Fedora KDE (tested)

```sh
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qtmultimedia-devel qt6-qtwebsockets-devel ydotool wl-clipboard
```

### Debian / Ubuntu KDE

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev \
  qt6-multimedia-dev qt6-websockets-dev qdbus-qt6 plasma-workspace \
  ydotool wl-clipboard
```

Your package sources must provide **Qt 6.7 or newer**. If they provide an older
Qt version, this source will not build without a newer Qt installation.

### Arch Linux KDE

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative \
  qt6-multimedia qt6-websockets qt6-tools plasma-workspace ydotool wl-clipboard
```

### Install speech recognition and run

Install NVIDIA's NeMo-Speech.cpp runtime and fetch the model Sayline uses:

```sh
curl -fsSL https://github.com/NVIDIA/NeMo-Speech.cpp/raw/main/scripts/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
nemo-speech pull nemotron-en
```

From the Sayline repository, build and install the KDE shortcut:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./scripts/install-kde.sh
```

The installer configures Sayline under `~/.local`. Press **Ctrl+Space** to
start or stop dictation. It starts and manages its own speech process on port
`18080`; keep that port free and do not start a separate `nemo-speech serve`.
The installer requires an active Plasma session, D-Bus, systemd user services,
`qdbus`, and `kwriteconfig6`. It does not require `sudo`.

Before installing, configure and start `ydotoold` for your distribution and
ensure it can access `/dev/uinput`. Do not run Sayline as root.

## Testing on another KDE distribution

Confirm that the session is Plasma 6 on Wayland and that the installed Qt is
6.7 or newer. Install the packages above using your distribution's package
manager, then build and run the visual preview:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/sayline --preview
```

The preview checks that the overlay launches; it does not test the microphone,
speech model, shortcut, or text insertion. To exercise those, install the
NeMo-Speech.cpp runtime and model, configure `ydotoold`, then run
`./scripts/install-kde.sh` in the Plasma session. Try dictating into a text
editor and verify that **Ctrl+Space** opens and closes the overlay, live text
arrives, and the intended application keeps focus. Please report the distro and
version, Plasma and Qt versions, and any installer or runtime error when sharing
test results.

## Windows and macOS

Windows and macOS are **not supported**. Sayline currently depends on Linux
Wayland, KWin scripting, D-Bus, `ydotool`, and Linux-specific process handling.
Porting would require replacing those integrations; installing Qt and building
the current source is not sufficient. A future port would also need Qt 6.7+,
CMake 3.21+, and a C++20 compiler, plus platform-specific equivalents for the
desktop integration and input injection.

## Speech recognition credits

The recognition engine and `nemotron-en` model come from NVIDIA's
[NeMo-Speech.cpp project](https://github.com/NVIDIA/NeMo-Speech.cpp); Sayline
provides the KDE interface and integration. Check NVIDIA's project for its
runtime license, notices, and model terms.
