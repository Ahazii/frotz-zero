# Store submission review

This is a prepared store listing, not a submitted store release. Source is maintained at [Ahazii/frotz-zero](https://github.com/Ahazii/frotz-zero). Publishing that repository does not upload a binary or submit it to the device store. The author is Ahazi (`Ahazii`), maintainer email `jbanner@btinternet.com`, and the user-selected share code is **FRTZ**. The current application/package version is 0.8.1 / 0.8.1-1.

The inspected registry revision in `dependencies.lock.json` contained no Frotz Zero / frotz-zero / FRTZ match. This is a snapshot check, not a reservation. Recheck name and share-code availability against the current registry immediately before publication.

## Review gates

- [ ] Approve the concrete binary, corresponding source archive, icon and listing in `app-builder.json`.
- [ ] Confirm physical launcher launch/exit, keyboard modifiers, readable display and audible/muted sound using `docs/TEST_RESULTS.md`.
- [ ] Resolve any failing software tests; keep resource-dependent commercial v6 tests explicitly pending in the compatibility statement.
- [ ] Verify that the source repository and source release are public and correspond to the binary.
- [ ] Recheck current official policy and registry uniqueness.
- [ ] Record the physical-device demo below; desktop captures do not satisfy the required video.
- [ ] Obtain Ahazi's explicit approval before upload or submission PR creation.

The permission declaration enables **network** for optional public HTTPS catalogue/story downloads. The remaining six booleans are false: microphone, camera, IMU, extra hardware, background services and external displays are not used. Normal play remains offline. The desktop development preview is not a claim of external-display support on the device.

Screenshots in `store/screenshots` are exact 320×170 outputs of the real preview renderer, using only original CC0 content. They are desktop evidence; do not label them hardware captures. The 256×256 icon is original artwork, with no baked-in rounded corners or border.

## Validate without publishing

From **WSL**, in the project directory:

```bash
python3 scripts/validate_release.py
```

This invokes the pinned official package-path and store-metadata validators locally, including the checked-out registry for uniqueness, and uses AppBuilder's read-only package checks. It does not call `publish`, authenticate or upload anything.

## Publish only after approval

Refresh and review the official AppBuilder and registry checkouts first. From **WSL**, in the project root:

```bash
.cache/AppBuilder/czdev login
.cache/AppBuilder/czdev publish --deb dist/frotz-zero_0.8.1-1_arm64.deb
```

`publish` uploads and creates a package submission PR; it is not a dry-run command. Complete the local GitHub device-authentication flow without exposing tokens. Attach the approved physical demo video to the resulting PR as requested by maintainers. Publish the corresponding-source archive alongside the binary release. See the [official submission guide](https://github.com/CardputerZero/cardputerzero.github.io/blob/main/docs/en/CP0_submit.md).

## Physical demo script (about 90 seconds)

Build the CC0 fixtures with `python3 scripts/make_fixtures.py` in WSL. `Test-Device.ps1` stages the Lantern Room and graphics fixtures in the device user's `~/.cache/frotz-zero-tests/fixtures` folder. Before recording, open Frotz Zero normally and change Settings' story folder to that absolute path (for example, `/home/pi/.cache/frotz-zero-tests/fixtures`). This keeps private games out of the filmed library. Restore your usual story folder after recording. Wake the launcher with Tab then Enter if needed.

1. Show the actual Zero and its normal launcher. Open Frotz Zero.
2. Show all six home actions, open Library and highlight the original Lantern Room fixture to show its synopsis. Select it and start New Game. Type LOOK and NORTH with the physical keyboard.
3. SAVE as `demo`, move again, and RESTORE `demo` to demonstrate portable saves.
4. Open Ctrl+Esc, return to the library, choose Continue, and demonstrate the retained prompt/draft.
5. Change reading size with Ctrl+U/D, then browse command history with Ctrl+P/N. Demonstrate Automatic paging and Tab's original-layout view. Open Fn+H, choose a Controls topic, page it and return with Esc. Open Settings, change light/dark and Mute, then return to play. Ctrl+V retains manual display choices; pan enlarged originals with Ctrl+F/X/Z/C. Use Ctrl with the plain key, with no Fn, Sym or Shift.
6. Start the original v6 resource fixture. Show its checker image, play its tone, and demonstrate mute. Do not film commercial story content for redistribution.
7. Exit through the app menu and show the normal device launcher restored.

Record what actually happens. If a check fails, fix it and repeat before claiming completion. No video has been generated or fabricated by the desktop tests.
