# Windows build and device quick-start

The verified development route is **Windows PowerShell → Ubuntu 24.04 under WSL2**. Windows OpenSSH performs device transfers; WSL handles Linux compilation and preview. Windows and WSL networking are separate. The device inspected for this build is Debian 13 ARM64, glibc 2.41, 320×170 RGB565 with a tca8418c keyboard and ES8389Audio.

## 1. Check tools

In **PowerShell**:

```powershell
wsl --list --verbose
ssh -V
git --version
```

For a new checkout, choose a parent directory and clone the public source:

```powershell
git clone https://github.com/Ahazii/frotz-zero.git 'C:\Coding\Frotz Zero'
Set-Location 'C:\Coding\Frotz Zero'
```

Use a different destination if that folder already exists. Do not clone over an existing development workspace. Playable stories and media are absent from Git; create a local `stories` folder for files you supply. The original CC0 `.inf` fixture sources are included under `tests/fixtures`.

If Ubuntu is absent, install WSL/Ubuntu using Microsoft's Windows setup instructions, restart if required, and complete its local Linux-account setup. WSLg/X11 is used for the preview. Native MSVC builds are not currently provided because the application uses Linux framebuffer/input and POSIX save semantics.

In an **Ubuntu/WSL shell**, install the build prerequisites:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git curl python3 python3-pil \
  libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev libfreetype6-dev libpng-dev libjpeg-dev \
  zlib1g-dev libxkbcommon-dev libssl-dev libcurl4-openssl-dev libarchive-dev libxml2-dev nlohmann-json3-dev fonts-dejavu-core xkb-data \
  crossbuild-essential-arm64 inform6-compiler openssl zbar-tools xvfb xauth
cd '/mnt/c/Coding/Frotz Zero'
python3 scripts/fetch_dependencies.py
```

The source release already contains Frotz; skip fetching when building it. Python 3.12 or newer is required for the guarded SDK extractor.

## 2. Preview

In **PowerShell**, from `C:\Coding\Frotz Zero`:

```powershell
.\scripts\Build-Preview.ps1 -Run
```

It reads the local `stories` folder by default. To preview only redistributable fixtures, first run `python3 scripts/make_fixtures.py` in WSL, then:

```powershell
.\scripts\Build-Preview.ps1 -Run -StoryFolder '.\build\fixtures'
```

The same C++ menus, upstream engine, fonts and pixel buffer run on both platforms. Preview dimensions are logically 320×170, enlarged 3×. If WSL reports a missing Wayland socket, the script explicitly selects `SDL_VIDEODRIVER=x11`. Do not redefine `HOME` or system runtime variables to work around it.

## 3. Build the device package

In **PowerShell**:

```powershell
.\scripts\Build-Device.ps1
```

The first build downloads the official v0.0.4 template SDK (about 414 MB), checks its SHA-256, extracts only compile/link files inside `.cache/sdk`, and adds checksum-pinned Debian SDL and libarchive development packages matching the inspected device runtime. It uses GCC 13's ARM64 cross compiler and the target sysroot. Subsequent builds reuse verified downloads.

Outputs:

- `dist/frotz-zero_0.8.1-1_arm64.deb`
- `dist/frotz-zero_0.8.1-1_source.tar.gz`
- `dist/SHA256SUMS`

The package script uses a fixed file allowlist. Neither personal games nor local transcripts are build inputs. `SOURCE_DATE_EPOCH` controls archive timestamps. Toolchain versions and target package checksums are pinned/documented; byte-for-byte reproducibility across different compiler installations is not claimed without comparison.

## 4. Authenticate and install

Configure `deployment.local.json` (ignored by Git):

```json
{"host":"YOUR_DEVICE_IP","user":"pi"}
```

Replace `YOUR_DEVICE_IP` with the address shown on your Zero. Alternatively pass `-HostName`/`-UserName`, or use an SSH alias named `frotz-zero`. Verify the host fingerprint locally on first connection; all scripts retain host-key checking. Passwords are entered only into local OpenSSH prompts. This workspace's actual address remains only in the ignored local configuration.

To add an existing Windows public key after authorizing that change:

```powershell
.\scripts\Authorize-DeviceKey.ps1
.\scripts\Deploy.ps1
.\scripts\Copy-Stories.ps1
```

The authorization script transmits only `~\.ssh\id_ed25519.pub`, appends it if absent, and preserves existing keys. If the private key has a passphrase, unlock it locally with `ssh-add`. Deployment uses `sudo apt-get install --reinstall` for package management, so rebuilding the same development version installs the selected artifact; the application runs as the normal user. `-NonInteractive` uses existing key authentication and cached or passwordless sudo, and fails rather than asking for a secret.

Open **Frotz Zero** in the normal launcher. Open **Library** and press **R** after copying stories. Highlight a title to read its summary, then open its page for artwork and **Continue, New, Saves and Delete**. Adding games does not require rebuilding. `Copy-Stories.ps1 -StoryFolder ...` recursively copies that chosen folder; review it before copying. It does not delete originals or mirror deletions. `Deploy.ps1` selects the version in `app-builder.json` by default.

## 5. Testing and diagnostics

In **WSL**:

```bash
python3 scripts/make_fixtures.py
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/desktop -j4
ctest --test-dir build/desktop --output-on-failure
make -C third_party/frotz dumb -j4
python3 scripts/test_integration.py
python3 scripts/test_extended.py
python3 scripts/test_ui_reading.py
python3 scripts/test_shortcuts.py
python3 scripts/test_online.py
python3 scripts/test_credits.py
python3 scripts/test_views.py
python3 scripts/test_saves.py
python3 scripts/test_automatic.py
```

The online test uses `openssl` for its local TLS fixture. Installing `zbar-tools` enables the independent QR decoding check. No commercial story is required. `test_online.py --live` additionally fetches public catalogue metadata only.

After building the ARM64 package, check the installer and corresponding source:

```bash
python3 scripts/validate_release.py
python3 scripts/test_package_lifecycle.py
python3 scripts/test_source_release.py
cd dist
sha256sum -c SHA256SUMS
```

Package lifecycle checks use an isolated filesystem and package database; they do not uninstall the app on the host or Zero. The source check extracts the exact archive and rebuilds it independently. In a Git checkout, also use `python3 scripts/check_public_tree.py --ref HEAD` to check committed public contents; see [Contributing](../CONTRIBUTING.md).

In **PowerShell**:

```powershell
.\scripts\Test-Device.ps1
.\scripts\Test-Device.ps1 -UseBuild -Quiet
.\scripts\Test-Speaker.ps1
.\scripts\Collect-Diagnostics.ps1
```

Device smoke tests temporarily stop the user launcher and restore it on exit. Run them when the device is idle; they refuse an already-running Frotz Zero session. A short original tone is played unless `-Quiet` is set. The current suite has 34 checks covering menus and Controls help in both themes, V3–V8 display/input, supported Continue, save management, graphics/audio and online flows, using isolated fixtures and data. `-UseBuild` stages the compiled binary without changing the installed package. Physical keyboard, audible output and launcher-selection checks still need the hands-on checklist. Use the normal launcher for interactive testing so the firmware retains its sleep/wake handling.

`Test-Speaker.ps1` copies an original CC0 fixture into `~/Games/FrotzZero/stories/FrotzZero-Demos/000-speaker-check.blb`. It leaves the launcher running. Wake the Zero with **Tab, then Enter** if needed; open Frotz Zero normally, Library, R to refresh, select **Speaker Check** (filename `000-speaker-check.blb`), then New Game. **P** plays three separated tones, **Ctrl+Esc** opens the menu to toggle Mute, and **Q** returns to Library. App volume and system volume both affect audibility. Restore your preferred Mute/Volume settings afterward. The launcher's startup screen and beep are not proof of game audio.
