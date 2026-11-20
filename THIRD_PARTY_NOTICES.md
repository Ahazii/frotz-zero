# Third-party notices

Frotz Zero's combined distribution uses **GPL-3.0-or-later**. Individual upstream files retain their original licences and copyright notices. The new application code may be used under the terms stated in its SPDX headers. Original demonstration stories, generated test graphics/audio and the original icon artwork are **CC0-1.0**, as identified in their source files and `tests/fixtures/LICENSE`.

## Compiled into the application

- **Frotz**, maintained by David Griffith and contributors, originally by Stefan Jokisch. Pinned revision `042d7bcadc1e2a8090cf737c841d8b7fc14b6eff` from <https://gitlab.com/DavidGriffith/frotz>. GPL-2.0-or-later. The complete original `COPYING`, `AUTHORS` and per-file notices are preserved in the corresponding-source archive; `Frotz-COPYING` and `Frotz-AUTHORS` accompany the installed binary. This includes its SDL graphics/font3 renderer and Blorb implementation, with their original notices.
- Frontend changes are reproducibly applied by `scripts/prepare_engine.py` to a build-directory copy. That script is part of corresponding source. No modification to the pinned checkout is needed.

- **QR Code generator (C)** by Project Nayuki, MIT. Vendored `qrcodegen.c` and `qrcodegen.h` at revision `9a217d443937f60cc56c1458c39a211d7f2d3af5` from <https://github.com/nayuki/QR-Code-generator>. The full copyright and permission notice appears at the top of both files and in the installed `qrcodegen-notice.h`. File hashes are pinned in `dependencies.lock.json`.
- **JSON for Modern C++** by Niels Lohmann and contributors, MIT. The system development header `nlohmann/json.hpp` is compiled into the app; see the full notice supplied with Debian/Ubuntu `nlohmann-json3-dev` and <https://github.com/nlohmann/json>. Its MIT notice is included in `packaging/nlohmann-json-LICENSE.MIT` and the installed documentation.

## Dynamically linked system components

No shared libraries or fonts are copied into the `.deb`. Debian supplies these dependencies and their copyright/source information under `/usr/share/doc/<package>/copyright` and Debian source archives:

| Component | Upstream terms / use |
|---|---|
| SDL2, SDL2_ttf, SDL2_mixer | zlib licence; display/events, menu fonts, audio |
| FreeType | FreeType licence or GPL-2.0-or-later; story fonts |
| libpng | libpng licence; graphics and screenshots |
| libjpeg-turbo | IJG/BSD/zlib terms by component; JPEG decoding |
| zlib | zlib licence; compressed resources |
| libxkbcommon, xkb-data | MIT/X11 family terms; Linux keyboard mapping |
| OpenSSL 3 libcrypto | Apache-2.0; SHA-256 identity/checkpoints and catalogue MD5 comparison |
| libcurl and its system TLS backend | curl licence and backend terms; verified HTTPS catalogue and story downloads |
| libxml2 | MIT; bounded IF Archive XML catalogue reader |
| libarchive | BSD family terms; bounded ZIP bundle reader |
| glibc, libstdc++, libgcc | System runtime terms, including LGPL and GCC runtime exceptions |
| DejaVu fonts | Bitstream Vera/DejaVu terms; installed system font files |
| Optional JetBrains Mono | OFL-1.1; used only if already installed or explicitly selected |

SDL_mixer's optional codec dependencies remain dynamically linked system packages with their own notices. The tested fixture is original AIFF PCM, not licensed commercial audio.

GPL v3 is selected for the combined build because OpenSSL 3 uses Apache-2.0, and Frotz permits GPL version 2 **or later**. See the [OpenSSL licence](https://www.openssl-library.org/source/license/), [GNU compatibility explanation](https://www.gnu.org/licenses/license-compatibility.en.html), and [GPL v3 guide](https://www.gnu.org/licenses/quick-guide-gplv3.html).

## Reference and build tools

The offline story catalogue contains factual release identities checked against [Andrew Plotkin's Infocom catalogue](https://eblong.com/infocom/) and original spoiler-free synopsis wording under GPL-3.0-or-later. No commercial game text or binaries are included. See `docs/CATALOGUE.md` for matching and attribution.

The manual links acknowledge the [Infocom Documentation Project](https://infodoc.plover.net/) and its contributors. The media link points to the [Interactive Fiction Archive](https://ifarchive.org/). These independent preservation resources and the catalogue remain the work of their respective maintainers; they do not endorse this application. Story and media rights remain with their respective owners.

The original book, lantern and terminal-prompt logo in `assets/frotz-zero.svg`, its generated PNGs, menu pictograms and procedural miniature landscape artwork are CC0-1.0 artwork. Embedded and local story covers retain their owners' rights and are not distributed with the app. The application embeds project author/contact metadata, a concise code-author list, this notice and the GPL licence for offline reading under Help. Full upstream notices remain in the installed documentation and matching source archive.

The official CardputerZero Template and AppBuilder are MIT-licensed. They were inspected for conventions and are not linked into this application. Their pinned revisions, launcher/documentation revisions and the official SDK URL/checksum are in `dependencies.lock.json`. Official validation scripts are invoked from pinned local checkouts and are not vendored as application code. The SDK is a build input, not redistributed in the binary or source archive.

The corresponding-source archive contains all application, QR generator and Frotz code, original notices, fixture sources and scripts needed to rebuild. Install standard system development dependencies as documented. When distributing binaries, provide that matching source archive through the same download location; a future repository URL alone is not a substitute for corresponding source.
