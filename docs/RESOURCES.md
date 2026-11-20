# Graphics and audio resources

Use media for the **same release** as the story. The library reads version, release, serial and checksum from the story header and uses its code SHA-256 for save identity. A filename is a display fallback, not proof of a retail title or matching media.

## Supported association

- An executable Blorb (`.zblorb`, `.zlb`, `.blb`, `.blorb`) can contain both indexed `ZCOD` and resources.
- For a separate story, place `.blb` or `.blorb` media beside it. An embedded `IFhd` release/serial/checksum or matching `ZCOD` content hash associates different filenames automatically, such as `Arthur.blb` and `arthur-r74-s890714.z6`. Filename case does not matter for explicit same-stem companions.
- An embedded `ZCOD` must match the story's content hash. A companion `IFhd` release/serial/checksum must match when present. A mismatching same-stem companion is ignored with a diagnostic. Without embedded identity, a media file is only accepted when it has the exact same base filename as the story, and it is flagged for provenance verification. A same-stem identified companion takes priority over other identified matches; several equally good matches are left unattached. Malformed companions are isolated from the valid story.
- The pinned Frotz SDL renderer decodes indexed PNG and JPEG pictures, including the upstream palette/scaling logic. Audio decodes AIFF PCM and the upstream supported Ogg/MOD paths via SDL_mixer. The original AIFF fixture is tested; Ogg/MOD game combinations require their own verification.

The app derives picture/sound capability from its indexed resources and available backend. It does not invent missing media. The game menu's mute and volume preferences apply to the mixer, so muted effects still run and complete. If the real audio sink cannot open, a silent SDL backend retains resource duration and completion callbacks; the local per-story `audio-status` explains the fallback. It cannot recreate timing from absent or undecodable media.

## Original Infocom media

The chosen SDL frontend loads Blorb PNG/JPEG and supported audio. It has no decoder for original `.mg1`, `.eg1`, `.cg1` graphics sets or original `.snd` files. Upstream's documentation points to Blorb packaging/conversion for graphical media. Conversion is a separate, release-sensitive step; no commercial resource download or unverified conversion is included here. Keep original files unchanged and retain your rights/provenance information. Do not simply rename an original resource file to `.blb`.

## Development collection checked in November 2026

The private development collection checked during the November 2026 updates contained 49 story binaries and seven resource-only Blorbs. These files are not included in Git or releases. Four packs had exact matches: Arthur r74/890714, Journey r83/890706, Shogun r322/890706 and Zork Zero r393/890714. Shogun r321 is a different release and does not match the r322 pack.

`Lurking.blb` identifies r221/870918, while the supplied Lurking Horror story is r203/870506. They are not paired. `BeyondZork.blb` and `Sherlock.blb` contain neither IFhd nor executable code, so the generic filenames are not enough to establish a match. The supplied Sherlock r21 is explicitly the no-sound PC edition. Adding a sound pack cannot be assumed to change that edition's behaviour.

No story bytes, media filenames or save identities are changed to force a match. `--inventory` records the attached resource filename and indexed picture/sound flags. Local evidence and commercial-game screenshots remain outside release artifacts. Exact media association and a successful startup are separate from full game playability.

## Public fixture

`scripts/make_fixtures.py` compiles original Inform 6 sources and generates a checker picture and short 440 Hz AIFF tone. All fixture content is CC0. Its v6 Blorb tests the real graphics, sound and timed-input paths independently of commercial media. It is evidence for those paths, not evidence that the supplied v6 titles are fully playable.

The separate v5 `audio.blb` fixture uses normal-sized text and waits for **P** before playing three tones. `scripts/Test-Speaker.ps1` copies it into the default story library as `000-speaker-check.blb`; open it through the normal launcher and app. Test both unmuted and muted playback before treating software completion callbacks as physical audibility evidence.

Primary implementation references: [maintained Frotz source](https://gitlab.com/DavidGriffith/frotz), especially `src/sdl/sf_images.c`, `sf_resource.c`, `sf_sound.c`, and the [Z-machine standard](https://www.inform-fiction.org/zmachine/standards/current/index.html).
