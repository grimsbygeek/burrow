# Burrow

A simple way to use WireGuard on Linux. Import the file from your VPN provider (or enter the details yourself), then connect and disconnect with one button.

Burrow (binary: `wgtray`) is a Qt 6 tray app. Connections are stored and managed by **NetworkManager**, so they behave like any other network connection on your system. No root helper and no extra daemon.

## Features

- Import a `.conf` file, or create a connection by typing in the details (with key generation)
- One-click connect and disconnect, with live data sent and received
- Export a connection as a file or a QR code for your phone
- Rename and delete connections
- System tray icon with quick connect and disconnect
- Built-in troubleshooting and diagnostics
- Light and dark themes

<!-- TODO: add screenshots, e.g. ![Main window](docs/screenshot-main.png) -->

## Requirements

- Linux with NetworkManager (1.16+, which has native WireGuard support)
- A desktop with a system tray. GNOME needs the AppIndicator/KStatusNotifierItem extension for the tray icon.

## Install

### Flatpak

Not yet on Flathub. To build and install it locally, see [packaging/README.md](packaging/README.md):

    flatpak-builder --user --install --force-clean build-dir packaging/flatpak/<app-id>.yml
    flatpak run <app-id>

### From source

Build dependencies: CMake 3.21+, a C++20 compiler, and Qt 6.5+ (Core, DBus, Gui, Network, Widgets, and Test for the tests).

Fedora:

    sudo dnf install cmake gcc-c++ ninja-build qt6-qtbase-devel

Debian/Ubuntu:

    sudo apt install cmake g++ ninja-build qt6-base-dev

Build and run:

    cmake -S . -B build -G Ninja
    cmake --build build
    ./build/src/wgtray

## Tests

    ctest --test-dir build --output-on-failure

`tests/run_integration.sh` runs the NetworkManager integration test against `tests/fake_nm.py`, a fake NetworkManager, so it never touches your real connections.

## Project layout

| Path | Contents |
|---|---|
| `src/core` | Config parsing and writing, key generation, QR codes, diagnostics, preferences |
| `src/nm` | NetworkManager D-Bus client |
| `src/ui` | Windows, dialogs, tray, theme |
| `tests` | Qt Test suites and the fake NetworkManager |
| `packaging` | Desktop file, AppStream metadata, Flatpak manifest |

## Flatpak permissions

| Permission | Why |
|---|---|
| `--system-talks-name=org.freedesktop.NetworkManager` | create, connect and delete connections; read their state and keys |
| `--talk-name=org.kde.StatusNotifierWatcher` | the system tray icon |
| `--talk-name=org.freedesktop.Notifications` | the "still running in the tray" notification |
| Wayland/X11, DRI, IPC | window and rendering |

## Contributing

Issues and pull requests are welcome. Please run the tests before submitting.

## Support

If you find it useful, you can [buy me a coffee](https://buymeacoffee.com/grimsbygeek).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Disclaimer

This is an independent project. It is not affiliated with or endorsed by the WireGuard project. WireGuard is a registered trademark of Jason A. Donenfeld.
