# SpeedCrunch — unofficial up-to-date builds

Native **macOS (Apple Silicon)** and **Windows (x64)** builds of [SpeedCrunch](https://speedcrunch.org), the fast, keyboard-driven, high-precision scientific calculator. The source here is the actively maintained [official repository](https://bitbucket.org/heldercorreia/speedcrunch) with two small additions, listed below.

**[⬇ Download the latest release](../../releases/latest)**

## Why this exists

One morning in 2026 I opened my MacBook to a notice that Rosetta 2 is being retired — and with it, the ten-year-old Intel build of SpeedCrunch I'd been running every day. I work in construction, and my team and I live in this calculator: earthwork volumes, tonnage conversions, quick engineering math. Losing it wasn't an option, and there were no current binaries to download anywhere.

So, in the spirit of full honesty: I didn't port anything. I asked Claude (Anthropic's Claude Fable 5, via Claude Code) to figure it out. It confirmed the official source builds cleanly with Qt 6, produced a native Apple Silicon app, and set up the automated Windows and macOS builds published here. My contribution was loving this calculator enough to ask.

## Credit where credit is due

SpeedCrunch is the work of many people over more than two decades, and all credit for the calculator itself belongs to them:

- **Ariya Hidayat** — original author
- **Helder Correia** — main author, maintainer, and logo
- **Wolf Lammen** — math engine
- **Felix Krull, Hadrien Theveneau, Pol Welter, Teyut** — core developers
- …and the many contributors and translators thanked in the app's About box (Help → About)

Thank you, all of you. This program is a small masterpiece of focused software — it starts instantly, does exactly what it should, and gets out of your way. If you find these builds useful, the people above are who to thank.

## Downloads

Each [release](../../releases) includes:

- **macOS (Apple Silicon)** — `SpeedCrunch-macos-arm64.zip`. The app is not notarized, so the first launch needs right-click → Open (or `xattr -d com.apple.quarantine SpeedCrunch.app`).
- **Windows (x64) installer** — `SpeedCrunch-windows-x64-setup.exe`. Standard installer with Start-menu entry and uninstaller.
- **Windows (x64) portable** — `SpeedCrunch-windows-x64-portable.zip`. Unzip anywhere and run `SpeedCrunch.exe`; nothing touches the registry. Either way, SmartScreen may warn because the binaries are unsigned — "More info → Run anyway".
- **Linux** — no binaries here; most distributions already package SpeedCrunch, and building from source (below) is straightforward.

## What's different from the official source

1. A GitHub Actions workflow that produces the release builds.
2. One cosmetic tweak: the input-bar outline is slightly softened.
3. A `NOMINMAX` define so the Windows build compiles under MSVC.
4. The Inno Setup installer script updated to package the Qt 6 runtime.

Everything else is the official source, unmodified. Bugs in the calculator itself belong in the [upstream issue tracker](https://bitbucket.org/heldercorreia/speedcrunch/issues); problems with these *builds* belong [here](../../issues).

## Building from source

To build SpeedCrunch, you need:

- A C++17-capable compiler
- [Qt](http://qt.io) 6.x (Core, Widgets, Help, Network)
- [CMake](http://cmake.org) 3.16 or later

To build SpeedCrunch in a dedicated build directory and install it, run the following
commands from the root of the source directory:

    mkdir build
    cd build
    cmake ../src
    make install

When building against a Qt version that is not the system default Qt installation,
point CMake towards the Qt installation to use by setting `CMAKE_PREFIX_PATH` or
`Qt6_DIR` when running CMake.

Example (Homebrew on macOS):

    brew install qt
    mkdir build
    cd build
    cmake ../src -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
    make

You can customize the build using the following variables. These are specified when
running CMake, in the form `cmake ../src -Dvariable=value`.

- **PORTABLE_SPEEDCRUNCH**: Set this to `on` to have the application settings stored
  in the same location as the executable, e.g. for running from a USB drive without
  requiring installation.
- **CMAKE_INSTALL_PREFIX**: Change the installation prefix for SpeedCrunch.
- **HTML_DOCS_DIR**: Change the path to the HTML manual that's embedded in the binary
  by the build. By default, a bundled prebuilt copy is used to minimize dependencies.

## Building the manual
Building the HTML manual is normally not necessary because a prebuilt copy is included
with the SpeedCrunch source. For more information, see the [manual's README](doc/src/README.md).

## Contributing
- Report bugs or request features in the upstream
  [issue tracker](https://bitbucket.org/heldercorreia/speedcrunch/issues).
- Add or improve a [translation](https://www.transifex.com/projects/p/speedcrunch/).
- Send a message to the [forum](https://groups.google.com/group/speedcrunch).
- Follow the news on the [blog](http://speedcrunch.blogspot.com).

## License
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
