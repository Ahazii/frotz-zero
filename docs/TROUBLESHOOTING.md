# Troubleshooting, backup and removal

**Empty library:** use Settings to check the story folder, copy files there, then open Library and press R to refresh. Change the directory in Settings using an existing readable path. Scanning supports subfolders to depth six and deduplicates identical content. An extension alone does not establish story validity.

**Cannot open framebuffer or keyboard:** run `Collect-Diagnostics.ps1`. The inspected `pi` account already has video/input/audio/render access. Use the ordinary launcher, not `sudo frotz-zero`. Do not change all device nodes to world-writable. After firmware changes, re-run `Inspect-Zero.ps1`; device-node names and ABI may differ. Expert overrides are `FROTZ_ZERO_FB` and `FROTZ_ZERO_INPUT`.

**Black screen:** first try the firmware's wake sequence, **Tab then Enter**. Use the normal launcher for interactive testing. A remote test that stops the launcher can bypass its wake handling; a black-screen incident on that path was not conclusively diagnosed, and the interactive speaker check now uses the normal launcher. The automated device test deliberately restarts the launcher afterward, so its startup screen/beep does not itself mean the Zero rebooted.

**No sound:** check Mute and Volume in Settings and the device's speaker mixer. The app lets SDL use the firmware's managed PulseAudio/PipeWire output. The inspected image rejected direct ALSA output but accepted PulseAudio; no global audio configuration is changed. Expert `SDL_AUDIODRIVER` overrides are respected. A failed sink uses a silent timed mixer and writes a local `audio-status`. No sound can be reconstructed from missing media.

Use `Test-Speaker.ps1` to prepare a key-triggered CC0 sound check, then open `000-speaker-check.blb` through the normal app. Restore your preferred Mute/Volume settings afterward; they persist by design.

**Missing graphics:** read Story details and [resource association](RESOURCES.md). Matching release media is required. Original Infocom graphics extensions cannot be renamed to Blorb. Missing media is not evidence of a corrupt story.

**Continue unavailable:** its supported mode is an ordinary untimed v3–v5/v7/v8 line prompt without active audio/streams/interrupts. Use the story's SAVE command in other modes. On corruption, Continue tries the previous complete generation and tells you when it recovered. If none remain, it offers New game or named saves and preserves the failed files. An unsupported current prompt does not silently resume an earlier turn.

**Save or settings write failed:** check free space (`df -h`) and ownership of the user data/config directories. The error dialog is scrollable. Existing named saves and completed generations are preserved. Do not delete a source save to make a failed load succeed. `local` diagnostics in this development workspace may include private filenames; review them before sharing.

**Game appears too small or clipped:** Automatic is the default for every supported story and wraps prose at your preferred size. Ctrl+U/D changes that size; Tab reveals the original map, status or artwork. Ctrl+V opens the manual display choices. Fit Screen shows the whole original layout without cropping; Fill Screen stretches it to fill the display. In Automatic or Reader, Space/Enter pages untimed text; typing reaches the prompt. Ctrl+F/X/Z/C pans enlarged original layouts. These controls use Ctrl plus one plain key. Changing views leaves the VM geometry and story bytes unchanged.

**Trinity's title/intro seems missing:** choose Automatic or Reader in Ctrl+V and start New. The opening copyright and key pause are readable before the first story paragraph. Ctrl+U/D adjusts size without dismissing the pause. Page through it with Space/Enter, then press again to begin. Continue resumes an existing command prompt and does not replay the introduction. Scrollback retains previously captured text; an old checkpoint cannot reconstruct text that an earlier app version did not capture.

**Make any story readable:** Automatic wraps prose at the selected reading size in V3-V8, including Beyond Zork. Ctrl+U/D changes size, Tab shows the original map/status/artwork, and Ctrl+0 toggles Automatic/Fit. Older checkpoints keep their game state and draft while adopting the new view; later manual choices are retained.

**Arthur alternates cropped graphics and tiny text:** Automatic is now the default for all supported versions, including earlier display preferences. It fits artwork and wraps text at the chosen size. Tab shows the fitted original artwork and returns to text without advancing the game. Ctrl+U/D sizes text; Ctrl+V retains all manual modes. Arthur uses timed character input during its intro: Fn+L/M pages readable text while the game retains its timers and keys.

**Keep a graphics border while zooming:** choose Frame + Text Zoom in Ctrl+V, or press Ctrl+U/D from V6 Fit/Fill. It fills the screen with the border and magnifies only the main text area at untimed command prompts. It preserves original lines, so Ctrl+F/X/Z/C may still be needed for wide text. Timed/character input, pointer mode and unsupported windows show the complete original layout.

**Remove a title:** Library → story → 4 Delete. Cancel is selected first; Review file list shows the exact files. Confirming permanently removes those story copies and exclusive media/art, keeping saves and shared files. Reinstall the same release to access retained saves. Changed files or an open session stop deletion; refresh and close that session before retrying.

**Manage saves:** Library → story → 3 Saves, then select a save and press M. Rename keeps the format and refuses an occupied name. Delete asks for confirmation and preserves other saves and automatic Continue. A save marked `!` cannot currently be loaded; Load explains the issue. If a save changed while its list was open, reopen the list before trying a mutation. Checkpoints containing links or unexpected directory structures are preserved for manual inspection.

**Ctrl shortcut needs another modifier:** use the physical-key mappings in [Controls](CONTROLS.md), including Ctrl+U/D for size and Ctrl+P/N for previous/next command. Earlier desktop combinations used symbols/arrows that sit behind the Zero's Sym/Fn layers; they are no longer the advertised device controls. Fn navigation, Help, Mute and PrintScreen continue to use their single Fn combinations.

**Finding a control:** open Help → Controls, or Fn+H during play. Choose Reading, Game controls, Menus, Saves or Keyboard. Up/Down and Fn+L/M page the shortcut rows; Enter/Esc returns to the topic list. Bksp is the short label for Backspace.

**SSH succeeds in your terminal but not in a script:** specify the same host/user and allow OpenSSH to use the existing key. Use `Check-DeviceKey.ps1` for public fingerprints and file permissions. If an encrypted key needs unlocking, run `ssh-add` locally. Never paste passwords, private keys or tokens into a chat or configuration file.

## Backup

Exit the app first so a complete generation is committed. In **PowerShell**, replace `YOUR_DEVICE_IP` with the actual address from your ignored local deployment configuration:

```powershell
ssh pi@YOUR_DEVICE_IP 'tar -czf "$HOME/frotz-zero-backup.tar.gz" -C "$HOME" .config/frotz-zero .local/share/frotz-zero Games/FrotzZero/stories'
scp pi@YOUR_DEVICE_IP:frotz-zero-backup.tar.gz .\local\
```

If a directory has not been created yet, omit it from the tar command. Keep the archive private. To restore, unpack it into the same user's home with the app stopped, checking that the intended directories will be replaced. Checkpoints may depend on the frontend version; `.qzl` files are the portable interchange format for matching story releases.

## Upgrade and uninstall

`Deploy.ps1` installs/upgrades using apt. Application data lives in the user's home and is not owned by the package. In **PowerShell**, remove the program with:

```powershell
ssh -t pi@YOUR_DEVICE_IP 'sudo apt-get remove frotz-zero'
```

Removal or purge preserves stories, settings and saves because no cleanup hook targets them. Personal-data deletion is a separate, optional manual action after backup; no cleanup command is run automatically. The app does not add a background service. If an interrupted SSH test leaves the launcher stopped, run `ssh pi@YOUR_DEVICE_IP 'systemctl --user start APPLaunch.service'`.

## Online catalogue or download fails

Local stories remain playable offline. Check Wi-Fi and use Online Library > Refresh catalogue. A failed refresh keeps the previous cache. TLS failures can indicate an incorrect device clock or missing CA certificates; correct those instead of disabling certificate checks. A conflicting filename is never overwritten: move or rename that file yourself if you want another release at that name. Read [Online Library](ONLINE_LIBRARY.md) for limits and cache paths.

If a new Blorb does not attach, check the story's full details and compare its release/serial/checksum with the pack's identity. A similar title is not enough. See [Resources](RESOURCES.md), including the mismatched or unidentified packs in the updated development collection.
