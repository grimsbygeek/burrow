# Packaging

## Before submitting to Flathub

1. **Pick the real app ID** (reverse-DNS of a domain or GitHub account you control) and apply it everywhere:
   `packaging/set-app-id.sh org.yourdomain.Burrow`, then reconfigure from a fresh build folder
   (CMake caches the old ID).
   Flathub verifies ownership: for a domain ID you must publish
   `https://<domain>/.well-known/org.flathub.VerifiedApps.txt`; `io.github.<user>.<App>` IDs are verified through GitHub.
2. **Fill in the AppStream gaps** in `wgtray.metainfo.xml.in`: `homepage`, `bugtracker`, `vcs-browser`,
   `<developer>`, and `<screenshot>` entries with public image URLs (Flathub requires at least one).
3. **Point the manifest at a git tag** instead of the local `dir` source (see the comment in the manifest).
4. Check the checksums of the two dependency tarballs against the upstream release pages.

## Build and test locally

    flatpak-builder --user --install --force-clean build-dir packaging/flatpak/<app-id>.yml
    flatpak run <app-id>

Validate before submitting:

    desktop-file-validate <installed .desktop>
    appstreamcli validate --pedantic <metainfo.xml>
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest packaging/flatpak/<app-id>.yml

## Permissions the app asks for (and why)

| Permission | Why |
|---|---|
| `--system-talk-name=org.freedesktop.NetworkManager` | create, connect, delete connections; read their state and keys |
| `--talk-name=org.kde.StatusNotifierWatcher` | the system tray icon |
| `--talk-name=org.freedesktop.Notifications` | the "still running in the tray" notification |
| `--socket=wayland`, `--socket=fallback-x11`, `--device=dri`, `--share=ipc` | window and rendering |

File choosers (import, export) and the "Buy me a coffee" link use the desktop portals, so they need no permission.
