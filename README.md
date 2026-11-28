![minipro.jpg](res%2FAppIcon.ico)
# MINIPRO-GUI
### *GUI wrapper for the [minipro CLI tool](https://gitlab.com/DavidGriffith/minipro) - a free and open TL866 series programmer*

[![Unit Tests](https://github.com/twelve-chairs/minipro-gui/actions/workflows/tests.yaml/badge.svg)](https://github.com/twelve-chairs/minipro-gui/actions/workflows/tests.yaml)
[![Build Ubuntu](https://github.com/twelve-chairs/minipro-gui/actions/workflows/ubuntu.yaml/badge.svg)](https://github.com/twelve-chairs/minipro-gui/actions/workflows/ubuntu.yaml)
[![Build MacOS](https://github.com/twelve-chairs/minipro-gui/actions/workflows/macos.yaml/badge.svg)](https://github.com/twelve-chairs/minipro-gui/actions/workflows/macos.yaml)
[![Build Windows](https://github.com/twelve-chairs/minipro-gui/actions/workflows/windows.yaml/badge.svg)](https://github.com/twelve-chairs/minipro-gui/actions/workflows/windows.yaml)
[![CodeQL](https://github.com/twelve-chairs/minipro-gui/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/twelve-chairs/minipro-gui/actions/workflows/github-code-scanning/codeql) 
[![Dependabot](https://img.shields.io/badge/Dependabot-enabled-025E8C?logo=dependabot)](https://github.com/twelve-chairs/minipro-gui/security)
[![Sonar Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=twelve-chairs_minipro-gui&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=twelve-chairs_minipro-gui)
[![Sonar Coverage](https://sonarcloud.io/api/project_badges/measure?project=twelve-chairs_minipro-gui&metric=coverage)](https://sonarcloud.io/component_measures?id=twelve-chairs_minipro-gui&metric=coverage)

![screenshot.png](res%2Fscreenshot.png)
## Installation
Download a ready-to-run package from [Releases](https://github.com/twelve-chairs/minipro-gui/releases): a `.zip` for Windows, a `.dmg` for macOS (Apple silicon and Intel) and an AppImage for Linux. The release notes say how to open each one. You still need the [minipro CLI](https://gitlab.com/DavidGriffith/minipro) installed (see Dependencies below). Or build from source.
## Dependencies
#### MacOS

```
brew install cmake ninja qt6 minipro
```

#### Ubuntu / Debian
Install minipro CLI (required): https://gitlab.com/DavidGriffith/minipro#install-build-dependencies

```
sudo apt-get install build-essential ninja-build cmake libgl1-mesa-dev qt6-base-dev -y
```

  (*If your OS uses Wayland (e.g. Raspberry PI 5), please also install `qt6-wayland` via apt-get*)

#### Fedora
Install minipro CLI (required): https://gitlab.com/DavidGriffith/minipro#install-build-dependencies

```
sudo dnf groupinstall "C Development Tools and Libraries"
sudo dnf install ninja-build cmake mesa-libGL-devel qt6-qtbase-devel -y
```

#### openSUSE
Install minipro CLI (required): https://gitlab.com/DavidGriffith/minipro#install-build-dependencies
```
sudo zypper install --type pattern devel_basis
sudo zypper install cmake Mesa-libGL-devel qt6-base-devel -y
```

#### Windows
- **Build tools:** [Visual Studio](https://visualstudio.microsoft.com/) 2022 or later, or its Build Tools, with the *Desktop development with C++* workload, which includes CMake.
- **Qt:** Qt 6 for *MSVC 2022 64-bit*, from the [Qt Online Installer](https://www.qt.io/download-qt-installer-oss). CI builds with Qt 6.10.3.
- **minipro CLI (required):** there is no Windows package. Build it from source in [MSYS2](https://www.msys2.org/): its Makefile has a Windows branch that uses WinUSB and needs zlib. Put `minipro.exe` on your `PATH`. The programmer must use the WinUSB driver (you can install it with [Zadig](https://zadig.akeo.ie/)).

Each run of the [Build Windows](https://github.com/twelve-chairs/minipro-gui/actions/workflows/windows.yaml) workflow also uploads a ready-to-run build (`minipro-gui-windows`), which you can download from the run's page while signed in to GitHub. It needs the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist).

## Build and run
macOS and Linux:
```
git clone https://github.com/twelve-chairs/minipro-gui.git
cd minipro-gui
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S . -B "./cmake-build-minsizerel"
cmake --build ./cmake-build-minsizerel --target minipro-gui -- -j 2
./cmake-build-minsizerel/minipro-gui                            # Linux
open ./cmake-build-minsizerel/minipro-gui.app                   # macOS
```

On Linux, install it to get a menu entry and the app icon in the panel. This puts the program in `/usr/local/bin`, `minipro-gui.desktop` in `/usr/local/share/applications` and the icon in `/usr/local/share/pixmaps`:
```
sudo cmake --install ./cmake-build-minsizerel
```

Windows, in PowerShell (adjust the Qt path to your install):
```
git clone https://github.com/twelve-chairs/minipro-gui.git
cd minipro-gui
cmake -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\msvc2022_64 -S . -B build
cmake --build build --config MinSizeRel --target minipro-gui
C:\Qt\6.10.3\msvc2022_64\bin\windeployqt.exe --release --no-translations --no-network --no-opengl-sw build\MinSizeRel\minipro-gui.exe
.\build\MinSizeRel\minipro-gui.exe
```