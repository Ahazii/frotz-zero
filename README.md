# Frotz Zero

An offline, keyboard-operated Z-machine player for the **Linux M5Stack Cardputer Zero**, with a Windows/WSL desktop preview. It integrates the maintained Frotz interpreter and its graphical frontend; the VM and game parser are upstream Frotz.

**Current version: 0.8.1-1.** The installed build has been verified on the Zero. See the [changelog](CHANGELOG.md) for the feature history and [verification report](docs/TEST_RESULTS.md) for the limits of the tests.

The home screen shows Continue with the last played title, Library, Downloads, Settings, Help and Exit. Library previews recognised titles and spoiler-free summaries. Each story opens one page with artwork and Continue, New, Saves and Delete. Deletion asks for confirmation, offers a file list and preserves saved games, checkpoints and shared media. All four arrows navigate menus; the displayed number opens its action immediately. Dark and light themes, clear focus, volume and mute are available.

**Save management:** the story page's **3 Saves** lists named saves by date with their type. Enter loads; **M** opens Load, Rename and Delete. Renames never overwrite another save, deletion is confirmed, and automatic Continue stays protected.

An original book-and-lantern logo and labelled menu icons bring an adventure theme to the launcher and app. **Help → About Frotz Zero** shows the version and author; **Help → Credits & licences** contains project, Frotz and library acknowledgements, the full GPL licence and manual links. Credit text is built in and works offline. Dark mode is the default; Settings offers a matching light theme.

**Help → Controls** is a topic menu: Reading, Game controls, Menus, Saves and Keyboard. Larger action text and separate shortcut badges replace dense paragraphs. Up/Down or Fn+L/M pages a topic; Enter/Esc returns to the topic list. Fn+H opens the same guide during play without losing your unfinished command.

The app supports v3–v8 execution, Blorb PNG/JPEG graphics and supported audio, split windows, styles, command history, scrollback and portable Quetzal saves. **Ctrl+V** opens Reader, Fit Screen, Fill Screen, Fit Width, Actual Pixels, Reading Zoom, Frame + Text Zoom and Automatic. Every supported story version (V3-V8) now defaults to Automatic: prose wraps at your chosen reading size, while Tab shows the fitted original layout for artwork, maps and status information. This includes Trinity's opening and Arthur's timed intro. Ctrl+U/D sizes text without changing the game's layout. Existing stories switch to Automatic once; later manual choices persist, including when loading older saves. Frame + Text Zoom fills the screen with the original V6 border while magnifying only its main text window at untimed command prompts. **Ctrl+U/D** changes reading size from 12–24 px; in fitted V6 views it selects framed text zoom. **Ctrl+P/N** recalls commands. **Ctrl+F/X/Z/C** pans up/down/left/right. Each shortcut uses Ctrl with one physical key.

**Current limits:** automatic Continue supports ordinary untimed v3–v5/v7/v8 line prompts without active audio, interrupts or transcript/replay streams. V6 and timed/character input retain story-provided SAVE/RESTORE. Reader is a plain-text alternative, including non-status upper-window text panels; use the original layout for styles and maps. Frame + Text Zoom preserves the original text layout, so wide lines can need horizontal panning. Automatic uses plain wrapped text, so use Tab or a manual view for original styles, spatial maps and combined layouts. Timed prompts retain their timers and story keys; Fn+L/M pages their text. Pointer mode uses the complete V6 layout. Full commercial-game playability requires separate verification. See [verification evidence](docs/TEST_RESULTS.md).

No commercial stories or media are distributed. The original CC0 test stories and generated media can be rebuilt for demonstrations.

## Start on Windows

Clone [Ahazii/frotz-zero](https://github.com/Ahazii/frotz-zero), then follow [the Windows guide](docs/WINDOWS.md) for dependencies, builds and SSH authentication. The repository has no playable stories or media packs. Add your own files to a local `stories` folder, or generate the original CC0 test fixtures as described in the guide. With the documented WSL dependencies installed, run these **PowerShell** commands from this folder:

```powershell
.\scripts\Build-Preview.ps1 -Run
.\scripts\Build-Device.ps1
.\scripts\Deploy.ps1
.\scripts\Copy-Stories.ps1
```

The first command opens the actual renderer and engine at 3× the device's logical 320×170 resolution. The ARM64 package is `dist/frotz-zero_0.8.1-1_arm64.deb`. It targets the inspected Debian 13 device image; older firmware may need a different build.

On the Zero, open **Library**, choose a story, then **New Game**. **Ctrl+Esc** opens the game menu and **Fn+H** opens help. Home **Downloads** offers the Infocom catalogue and IF Archive Z-code collection, including supported ZIP bundles. Catalogue notes are cached offline; manual and website links show QR codes for a phone or PC. Ordinary play needs no network. See [Online Library](docs/ONLINE_LIBRARY.md).

## Personal data

| Content | Default location on the Zero |
|---|---|
| Stories and matching media | `~/Games/FrotzZero/stories/` |
| Preferences | `~/.config/frotz-zero/settings.conf` |
| Cached catalogues and downloaded story metadata | `~/.local/share/frotz-zero/online/` |
| Per-story display choice | `~/.local/share/frotz-zero/stories/<identity>/display.conf` |
| Saves, checkpoints, private screenshots | `~/.local/share/frotz-zero/` |

`XDG_CONFIG_HOME` and `XDG_DATA_HOME` are respected. Saves are separated by SHA-256 story content identity, so renames retain them. The package installs no files into a home directory, no background service, and no data-removal script.

## Guides

- [Windows build, deployment and preview](docs/WINDOWS.md)
- [Controls and save behaviour](docs/CONTROLS.md)
- [Adventure artwork, themes and credits](docs/DESIGN.md)
- [Online Library, downloads and manual links](docs/ONLINE_LIBRARY.md)
- [Story identification and summaries](docs/CATALOGUE.md)
- [Graphics/audio resource association](docs/RESOURCES.md)
- [Architecture and pinned sources](docs/ARCHITECTURE.md)
- [Troubleshooting, backup and removal](docs/TROUBLESHOOTING.md)
- [Verification results and physical checks](docs/TEST_RESULTS.md)
- [Store review and submission](store/SUBMISSION.md)
- [Contributing and checking public source](CONTRIBUTING.md)
- [Version history](CHANGELOG.md)

## Source and licensing

Copyright © 2026 Ahazi and the respective upstream authors. The combined application is distributed under **GPL-3.0-or-later**; see [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). Frotz's GPL-2.0-or-later notices remain intact. Every package build also creates a corresponding-source archive containing the pinned Frotz source, adaptations and build scripts. Publish that archive alongside the binary.

The source repository is [Ahazii/frotz-zero on GitHub](https://github.com/Ahazii/frotz-zero). It contains the application, documentation, original artwork and original CC0 test-story source code only. Playable stories, media packs, saves, story archives, credentials and private test evidence are excluded. `scripts/check_public_tree.py` checks the staged or committed Git contents before a push. Store submission and binary-release publication are separate from publishing this source repository.
