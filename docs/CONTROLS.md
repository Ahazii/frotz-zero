# Controls, reading and saves

Home shows **Continue [last played title], Library, Downloads, Settings, Help and Exit**. All four arrows move between the visible buttons; Tab/Shift+Tab move through them in order. Numbers **1–6 open actions immediately**. Browsing a different story does not change Home Continue. The footer explains the focused action. Escape exits.

**Library** shows a synopsis for the highlighted title, its filename and edition. Up/Down selects; Right/Enter or the displayed **1–4** opens that story's page. Left/Esc returns home. **R** refreshes and **O** opens downloads. The story page shows artwork, a summary and **1 Continue / 2 New / 3 Saves / 4 Delete**; all four arrows move between these buttons. New starts a new game; Saves opens named saves. **Tab** opens full details and **Esc** returns to Library. Long titles scroll horizontally. Unknown releases keep their filename and an explicit unavailable-synopsis message. See [catalogue matching](CATALOGUE.md) and [online use](ONLINE_LIBRARY.md).

**Delete** opens a confirmation with Cancel selected. Review file list shows exactly which files will be removed. Confirmation permanently deletes identical copies of the selected release and its exclusive media/art inside the library. Other releases, shared files, manuals, saves and checkpoints stay. Download or copy back the same story release to use retained saves. An active session or changed file list stops deletion. No title is deleted merely by selecting its Delete button.

Other menus use Up/Down to move, Right/Enter to open and Left/Esc to go back. The displayed numbers **1–4 refer to the current four visible rows** and open them immediately. Plain F/X/Z/C also navigate only in menus. Editable fields keep letters and digits as text. Long messages scroll with Up/Down, page with Fn+L/M, and jump to the start/end with Fn+K/N. Focus hints and Help replace pointer hover on this keyboard-only device.

Home **Help** contains Controls, About Frotz Zero and Credits & licences. About shows the installed version and Ahazi (jbanner@btinternet.com); Enter opens the credits. Code authors is a short list of the authors of the included code. Library/source notices and the GPL licence remain available offline; complete upstream AUTHORS notices accompany the installed package and source archive. The credits also reuse the Online Library's manual/website QR menu. **Fn+H during play still opens Controls directly.**

**Controls** opens a topic list: Reading, Game controls, Menus, Saves and Keyboard. Each topic uses separate shortcut badges and larger action labels, with complete rows kept together on each page. Up/Down or Fn+L/M changes pages; Fn+K/N goes to the first/last page. A number jumps to that page. Enter/Right or Esc/Left returns to the topic list with the same topic selected. Esc/Left from the topic list returns to Help or the story. Both themes use the same spacing and controls; opening help does not change a story's draft, display mode or font preference.

On the inspected Zero firmware, wake the sleeping launcher with **Tab, then Enter** before opening the app. This device wake sequence is separate from story input.

All primary shortcuts below use one modifier with one physical key. For Ctrl shortcuts, press the letter itself; do not add Fn, Sym or Shift. The layout follows the [M5Stack keyboard guide](https://docs.m5stack.com/en/compute/cardputerzero/usage).

| During play on the Zero | Action |
|---|---|
| Ctrl+Esc | Game menu |
| Fn+H / Help | Controls |
| Letters, numbers, punctuation | Pass to the game |
| Fn+Z / Fn+C | Move the command cursor left/right |
| Fn+K / Fn+N | Move the command cursor to start/end |
| Backspace / Fn+Backspace | Delete left/right of the command cursor |
| Ctrl+P / Ctrl+N | Previous/next command; retain the unfinished draft |
| Fn+L / Fn+M | Page Automatic/Reader text; open transcript scrollback in other views |
| Fn+N or Escape while in scrollback | Return to live screen without submitting input |
| Ctrl+U / Ctrl+D | Reading size up/down immediately, 12–24 px in 2 px steps |
| Ctrl+V | Display choices, saved separately for this story |
| Ctrl+0 | Toggle Fit Screen / Automatic for every supported story |
| Tab in Automatic text view | Switch readable text / fitted original layout, including artwork, maps and status, without advancing the story |
| Ctrl+Z / Ctrl+C | Pan left/right, using the letters bearing those arrows |
| Ctrl+F / Ctrl+X | Pan up/down, using the letters bearing those arrows |
| Ctrl+Space | Toggle keyboard pointer mode |
| Fn+F/X/Z/C, Enter, Esc in pointer mode | Move pointer, click, cancel |
| Fn+A / audio-mute key | Toggle persistent mute |
| Fn+J / PrintScreen | Private screenshot in the data directory |

The Zero's Fn layer supplies arrows, Home/End, PageUp/PageDown, function keys, Help, Mute and PrintScreen. Those actions use Fn alone with their labelled key. App actions use Ctrl with plain physical letters or the dedicated Esc/Space/0 keys. F/X/Z/C, U/D, P/N and digits remain ordinary story text without Ctrl. The shared native input translator is exercised with Linux key press/release events; physical usability is still checked separately on the device.

On a full desktop keyboard, earlier Ctrl+Plus/Minus, Ctrl+Up/Down, Ctrl+Left/Right and Ctrl+Shift+Up/Down aliases remain accepted. The table above is the primary mapping for the Zero.

## Display behaviour

Ordinary v3/v5/v7/v8 games use a 320×170 canvas. V4, recognised Beyond Zork releases and v6 retain a 640×400 VM canvas, preserving Trinity's required width and fixed layouts. Every supported version (V3-V8), including Beyond Zork, defaults to Automatic. Existing display preferences adopt Automatic once. Subsequent manual choices remain saved and override the display mode in older checkpoints. New sessions keep a 12 px internal font; the selected reading size controls the readable frontend. Existing checkpoints retain their original VM dimensions and font metrics. **Ctrl+V** or Game menu → Display changes the view without restarting or changing the VM geometry:

| View | Behaviour |
|---|---|
| Automatic | Wraps text at the selected size in every supported version, including timed prompts; Tab fits the original layout, artwork, maps and status |
| Reader | Wraps prose and non-status upper text panels at the selected size; available for text games |
| Fit Screen | Shows the complete original layout with its proportions preserved |
| Fill Screen | Stretches the complete layout to 320×170; proportions change |
| Fit Width | Uses the full width; scroll/pan vertically if needed |
| Actual Pixels | One story pixel per display pixel |
| Reading Zoom | Magnifies the original canvas using the reading-size preference |
| Frame + Text Zoom | Fills the screen with the V6 frame, magnifying only its main text window at untimed command prompts |

Reader starts at the first unread text, including Trinity's opening copyright/intro pause before the story begins. Space/Enter advances pages; at the final intro page, press again to continue the game. Changing size retains the visible text position without sending a key to the story. Up/Down and PageUp/PageDown scroll. At command input, typing jumps to the prompt and keeps the command. Esc while paging jumps to the end. Switching into Reader preserves its position instead of jumping to the last line. It is a plain-text alternative, so use another mode for maps, styles and positioned windows. Timed input temporarily shows the original canvas.

Automatic handles non-status upper text panels and V6 panels outside window zero. Graphical-font symbols stay in the original layout. Status information and spatial layouts remain available with Tab. Fn+L/M (PageUp/Down) reads text; character-menu arrows reach the game. Space/Enter pages untimed text. Timed prompts keep their original deadlines and story keys; page with Fn+L/M instead. Tab inspects the fitted original layout and returns to the same page/draft. Use a manual view for styles, spatial maps or unusual window layouts.

Menus default to dark, with a light option. Story-requested colours/styles remain intact in the original view. Reading size is **12–24 px**. Ctrl+U/D rewraps Reader and Automatic text; from V6 Fit/Fill it selects Frame + Text Zoom. This preserves original text lines and pictures, so text can need panning. Ctrl+F/X/Z/C pans within the main text area while the frame stays fixed. Manual pan stops cursor following; typing resumes it. V6 character/timed prompts, pointer mode and unsupported window arrangements show the complete layout. Reading Zoom remains available to magnify the whole canvas. Pointer coordinates use that original layout. Fit Screen and Fill Screen show the full layout without panning.

Continue preserves the captured VM geometry, draft, history, page and Automatic text/original-view selection. A later per-story display choice takes precedence over an older checkpoint's mode. Changed reading preferences apply without replaying a turn. Earlier checkpoint schemas remain readable. Portable saves use the new session's geometry. Original story colours apply on a new session; Reader follows the selected interface theme.

Scrollback is a bounded plain-text reading history (128 KiB), separate from the live story framebuffer. It does not reproduce graphics or fixed-window layouts. Typing while viewing old output is discarded rather than sent to the game.

## Saves

Use the story's **SAVE** and **RESTORE** commands for standard `.qzl` Quetzal files. The name dialog stores multiple saves under the story's content identity, lists timestamps, and confirms overwrites with Cancel first. The old file survives a failed replacement.

The game menu's **Save checkpoint** records a `.fzsave` directory with VM, screen and frontend state at supported line prompts. These are Frotz Zero checkpoints, not portable Quetzal saves. **Load save** accepts either type, validates identity, and preserves the source on failure.

Story page **3 Saves**, or game menu **Load save**, opens the save manager. Names, dates and Quetzal/checkpoint types are shown, newest first. Up/Down selects, Right/Enter or the visible **1–3** loads, and Left/Esc returns. **M** or **Tab** opens Load, Rename, Delete and Back. Long names scroll. An exclamation mark indicates a save that needs attention; attempting Load explains why and keeps the original.

Rename keeps the extension and save contents, accepts up to 48 letters/numbers/spaces/hyphens/underscores, and refuses an existing name. Delete confirms with Cancel selected and removes only the chosen named save or checkpoint directory. Files changed since opening the list are refused. Other saves and automatic Continue are protected. The story's RESTORE command loads Quetzal files; use the app's Load save for checkpoints.

Automatic Continue captures the pre-read VM instruction before its operands are consumed. Each generation contains the matching screen/windows, RNG, command history, draft and transcript. A clean menu exit updates the draft. A sudden power loss may lose the current unfinished draft, but the previous complete generation remains. The newest two valid generations are retained; corrupt/incomplete generations are skipped.

Automatic Continue is unavailable in v6, character input, timed prompts, interrupt routines, active sound or active transcript/record/replay streams. The UI states this and leaves portable story saves available. It never silently replaces a failed resume with New game. Starting a new game confirms replacement of a valid automatic resume and preserves named saves.
