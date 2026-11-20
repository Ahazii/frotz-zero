# Download Stories

Open **Downloads** on Home, or press **O** in Library. Choose [Andrew Plotkin's Infocom catalogue](https://eblong.com/infocom/) or the [IF Archive Z-code collection](https://ifarchive.org/indexes/if-archive/games/zcode/). Up/Down selects, Right/Enter opens, Left/Esc goes back, and displayed number shortcuts open actions immediately.

1. Choose **Browse titles** or **Search titles**. First use fetches that source's catalogue; subsequent browsing uses the offline cache.
2. Choose a title and release/file. The preview shows catalogue notes; **Read all release notes** opens the complete description. Infocom collection releases appear first and carry an asterisk. Higher release numbers do not guarantee a better or finished game.
3. Choose **Download story**. Checked files enter your configured story folder and the story page opens with **Continue, New, Saves and Delete**. Browsing or downloading does not change Home Continue. A ZIP containing several stories installs the bundle and offers a choice of story.
4. **Manuals & websites** shows URLs and QR codes for the source catalogues, Infocom manuals, accessible manuals and media archive. Open these on a phone or PC. The app has no browser or PDF reader.

## Sources and formats

The Infocom source offers its supported v3–v6 story releases. IF Archive includes the main Z-code directory and language subfolders, excluding `old/`. It lists v3–v8 story files, Blorbs and ZIPs up to 32 MiB. Other formats, oversized files and unusual non-URL-safe legacy filenames remain accessible on the website. A ZIP with no supported Z-machine story is reported without installing it. Historical prototypes and unusual games may have interpreter limitations; listing a file does not prove full playability.

IF Archive titles use its IFWiki metadata where supplied, otherwise the description or filename. Descriptions and notes belong to the respective catalogues and authors. Existing verified offline synopses take precedence. Downloaded identity metadata is stored by content hash, so renaming a story keeps its title and notes. Covers come only from local images or a Blorb frontispiece, with an original illustrated fallback; the app does not fetch online cover art.

## Offline use and recovery

Startup and ordinary play make no network request. Infocom metadata is cached at `~/.local/share/frotz-zero/online/catalog.json`; the compact IF Archive cache is `ifarchive.json` beside it. Downloaded title metadata lives in `online/installed/`. These paths respect `XDG_DATA_HOME`. **Refresh catalogue** explicitly updates the selected source. Failed refreshes preserve the prior valid cache. First use and downloads require Wi-Fi.

Transfers show progress; **Esc/Left** cancels. Connection, total-transfer and low-speed timeouts bound failures. HTTPS certificate and hostname checks remain enabled; redirects and non-HTTPS URLs are refused. File paths must belong to the chosen source's permitted directory. No accounts or passwords are used.

| Bound | Limit |
|---|---|
| Infocom catalogue / story | 4 MiB / 8 MiB |
| IF Archive master index / compact cache | 32 MiB / 4 MiB |
| IF Archive story or ZIP download | 32 MiB |
| ZIP contents | 512 entries, 32 MiB per file, 64 MiB total, bounded path depth |

IF Archive metadata is streamed through libxml2 with network parsing disabled; DTDs and entity declarations are refused. Both sources require the catalogue's full-file MD5; IF Archive also checks published size. MD5 is the catalogue's integrity identifier and HTTPS authenticates transport. Story headers must validate, with Infocom release/serial/version also checked against its catalogue. A historical internal VM-checksum warning remains visible even when the full downloaded file matches the published digest.

## Files, bundles and saves

Raw stories install atomically without replacing a different file or symlink. An identical file can be reused. ZIP extraction rejects traversal, absolute paths, symlinks, hard links, special files, encryption, duplicate names and excessive sizes. Supported story/media/image/text/PDF files are retained with their safe relative layout under `stories/IFArchive/<bundle>-<digest>/`. All playable contents are validated before that directory becomes visible through an atomic no-replacement rename. Failed extraction leaves the library unchanged.

ZIP bundles keep supplied media and documents together. Infocom's separate media packs still need to be added locally; see [resource matching](RESOURCES.md). Story-only and resource-only Blorbs are distinguished during scanning. Story saves remain under their content identities. Downloading another release neither migrates nor deletes saved games. The one-off preferred-release cleanup requested for a personal library is separate from download installation; the app does not silently remove a user's other releases.

## Development checks

`scripts/test_online.py` tests the real UI and a local HTTPS server, including certificate/hostname enforcement, cancellation, size bounds and failures. `--live` fetches public metadata only. `archive-tests` checks XML, caches, ZIP boundaries and safe installation. `scripts/test_views.py` tests the IF Archive download flow with original CC0 fixtures. `--online-fixture DIRECTORY` is available only with the deterministic `--drive` test mode; no insecure transport option is exposed in normal use.
