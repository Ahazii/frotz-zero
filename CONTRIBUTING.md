# Contributing to Frotz Zero

Source lives at [Ahazii/frotz-zero](https://github.com/Ahazii/frotz-zero). Author/maintainer: **Ahazi**, `jbanner@btinternet.com`. Follow the [Windows/WSL build guide](docs/WINDOWS.md), [architecture notes](docs/ARCHITECTURE.md) and [controls reference](docs/CONTROLS.md).

## Repository contents

Commit application source, scripts, documentation, dependency locks, original artwork and the original CC0 Inform 6 sources under `tests/fixtures`. Generate their playable test stories and media locally with `python3 scripts/make_fixtures.py`; outputs stay in ignored `build/fixtures`.

Do not commit playable stories, commercial story source, media packs, covers from games, save files, private screenshots/transcripts, story archives, device configuration or credentials. This includes the local `stories` folder and `stories.zip`, and also copies placed elsewhere. Only the original CC0 fixture source files are approved story-source inputs.

`third_party/qrcodegen` is the small reviewed vendored dependency. The upstream Frotz checkout is ignored and fetched at the commit pinned in `dependencies.lock.json`. The corresponding-source release archive includes the complete pinned Frotz source and original notices; Git users fetch it using `scripts/fetch_dependencies.py`.

## Before committing and pushing

Review and stage explicit source paths. From **WSL**, in the project directory:

```bash
git status --short
git diff
git add README.md docs
git diff --cached --stat
python3 scripts/check_public_tree.py
```

The `git add` line is an example for documentation edits; add other intended source files explicitly. The checker reads the actual Git index, including files added with `git add -f`. It rejects private paths, compiled stories/media/saves/archives, unexpected binary files, unapproved source paths and common credential signatures. It permits the original CC0 `.inf` fixture sources and the reviewed PNG artwork/screenshots. Its output suppresses any detected credential value. Review file contents too; a pattern scan is not a substitute for provenance review.

After committing, check the exact commit before pushing:

```bash
python3 scripts/check_public_tree.py --ref HEAD
git show --stat --oneline HEAD
git push origin main
```

The initial publication has a single source-only commit with no imported private history. For subsequent work, review every outgoing commit as well as the final tree; removing a file in a later commit does not remove it from earlier history. Never force-add a forbidden file or bypass a failed check.

## Verification

Run the checks relevant to the change, using the full command list in [Windows setup](docs/WINDOWS.md#5-testing-and-diagnostics). Core tests keep assertions enabled. UI checks use the real 320×170 renderer and original CC0 fixtures; inspect dark/light captures when changing presentation. In-game changes must preserve input, drafts, timers and supported save boundaries.

Run device checks only when Frotz Zero is closed and the device is awake. Use `Test-Device.ps1 -UseBuild -Quiet` to avoid installing a test binary or playing a tone. Do not turn screenshots or callback tests into claims about physical readability, audibility or full game completion.

## Documentation and releases

Keep README, controls, build instructions and the changelog consistent with the application. Preserve historical test evidence under its version heading. Keep private evidence under ignored `local/`; do not copy it into Git to make a documentation path resolve.

Publishing source does not submit the app to the device store. A binary release needs its matching corresponding-source archive and checksums. Follow [store submission](store/SUBMISSION.md) for the separate approval and physical-demo requirements. Existing tested archives keep their recorded hashes when post-build documentation is updated.
