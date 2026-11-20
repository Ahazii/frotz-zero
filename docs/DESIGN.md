# Adventure artwork and credits

Frotz Zero uses an original open book, lantern and terminal prompt as its identity. The launcher receives a square 256×256 PNG with a navy background. The same mark appears on the home screen and About page. Teal menu icons, warm gold focus outlines and labelled actions provide a coherent style without putting artwork behind reading text. All six home actions remain visible at the native 320×170 resolution.

Dark is the default theme. Settings → Theme switches to a matching cream, deep teal and ochre light palette and saves the choice. The framebuffer launcher exposes no shared system-theme preference, so the app uses its explicit saved setting. Artwork is static; it introduces no animation loop, splash delay or online image request. Story pages add bounded local/embedded cover artwork and a procedural illustrated fallback. No online cover search or download runs.

The layout follows Microsoft's [icon guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/icons) and [inclusive design guidance](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/designing-inclusive-software): simple consistent symbols with labels, visible keyboard focus, readable contrast and clear hierarchy. The keyboard-only screen uses contextual footer hints in place of pointer hover. Decorative miniature artwork is not the only way to identify an action.

The four story actions use compact labels (Continue, New, Saves, Delete) with contextual explanations. Delete opens an explicit confirmation with a safe Cancel default and an optional exact file list, following Microsoft's [dialog guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/dialogs-and-flyouts/dialogs). Its wording distinguishes permanent story-file removal from retained saved games.

The named-save picker uses three two-line rows so names, dates and types remain readable at native resolution. Enter loads directly; the visible M hint opens management without adding a step to ordinary loading. Rename and confirmed Delete preserve other saves and automatic Continue. Dark and light themes share the same focus, navigation and spacing.

## Credits

Home → Help offers Controls, About Frotz Zero, Credits & licences and Back. Controls and in-game Fn+H open the same five-topic menu: Reading, Game controls, Menus, Saves and Keyboard. It reuses the normal menu with 16 px topic labels. Reference pages pair 14 px shortcut badges with 16 px action text, measured and wrapped before paging so a row is never divided or clipped. Up/Down, PageUp/PageDown, Home/End and page numbers navigate; Enter/Right or Esc/Left returns to the selected topic. This replaces the forced sequence of dense text screens. The same theme colours, header and keyboard routing serve both themes.

This layout applies Microsoft's [typography guidance](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography): left alignment, readable type and a clear hierarchy. Its grouped topics and predictable back navigation also follow the [keyboard interaction guidance](https://learn.microsoft.com/en-us/windows/apps/develop/input/keyboard-interactions). The app retains its existing font and does not add a rendering dependency.

About shows the build version, author and artwork; Enter opens the credits and Escape returns. The credits include:

- Ahazi, read from the existing application metadata.
- A concise Code authors page for Frotz, its Unix/renderer lineage, QR Code generator and JSON library. Full upstream AUTHORS and licence notices remain unmodified in package documentation and corresponding source.
- The project's third-party notices, including system libraries, the Infocom catalogue, documentation project and IF Archive.
- The full GPL v3 text and existing manual/website QR links.

The credit reader shows six body lines with a scroll indicator. F/X or arrows move a line; Fn+L/M page; Fn+K/N jump to the start/end; Enter or Escape returns. All credit text and QR generation work offline. Opening a linked website on another device needs its own connection.

## Rebuilding artwork and credits

`assets/frotz-zero.svg` is the canonical CC0-1.0 vector source. `scripts/make_icon.py` renders the launcher PNG and transparent 24, 40 and 96 px marks directly at their intended sizes. On Debian/Ubuntu, the optional artwork tools are `python3-gi`, `python3-cairo`, `python3-gi-cairo` and `gir1.2-rsvg-2.0`. Run `python3 scripts/make_icon.py` after editing the SVG. Normal builds use the supplied PNGs and do not require these tools.

`scripts/prepare_ui.py`, run by CMake, embeds the PNGs and reads author/contact metadata, assets/code-authors.txt, THIRD_PARTY_NOTICES.md and LICENSE. CMake tracks these inputs so edits rebuild the bundled information. `src/artwork.cpp` decodes each small PNG once using the existing libpng/SDL dependencies; it also supplies the shared menu pictograms and page header. `src/credits.cpp` reuses the existing menus, message reader and manual-link view. Story covers reuse libpng and libjpeg. Same-stem .cover.png/.cover.jpg/.cover.jpeg or .png/.jpg/.jpeg files take priority; otherwise the Blorb Fspc frontispiece is read through its Pict index. Covers are bounded to 8 MiB, 4096 pixels per side and 8 million pixels; an eight-thumbnail cache bounds memory. Missing or malformed artwork uses an original miniature landscape varied by title. No arbitrary game picture is mistaken for a cover.

`scripts/test_credits.py` exercises the real dark/light navigation, credit pages, native page keys, scroll limits and nested quit behaviour. Actual native-resolution screenshots and physical-device evidence are recorded separately in the verification report.
