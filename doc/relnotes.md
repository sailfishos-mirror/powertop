# PowerTOP Release Notes

## v2.16-rc2

### User-visible enhancements and changes

**Display / UI**
- Show "Preparing to take measurements" message during the initial
  one-second measurement so the screen is no longer blank at startup
- Vertical scrollbar added to tab content panes
- Scroll-down capped at content extent to prevent over-scrolling
- Terminal resize (KEY_RESIZE / SIGWINCH) now handled correctly while
  PowerTOP is running
- Notification system redesigned to use the status bar instead of
  overlaying content
- Tab bar and bottom line now correctly fill the full terminal width

**Reports**
- HTML and CSV output: fixed escaping and i18n issues
- Markdown report output format added (`--output=file.md`)

**Correctness / stability fixes**
- Fixed three memory leaks found by Valgrind (power meters, tuning
  window, wakeup tab window)
- Fixed double-free in devfreq cleanup
- Fixed sysfs residency overflow in Intel GPU reporting
- Fixed RAPL division-by-zero and improved error handling
- Fixed Bluetooth byte-counter logic and removed deprecated tooling
- Added near-zero division guards for AHCI, ALSA, and RAPL power
  calculations
- Fixed `--batch` mode numeric safety and i18n issues
- Fixed halfdelay timeout clamping to valid ncurses range

**Translations**
- 17 previously missing source files added to translation coverage
- All `.po` files regenerated; format-specifier changes (`%s` → `{}`)
  applied mechanically across all 11 languages

### New command-line options

- `--once`: take a single measurement and exit
- `--auto-tune-dump`: run auto-tune and dump results without
  interactive display

### Thanks

We thank the following external contributors who submitted patches for this release:

- Anthony Ruhier
- George Burgess IV
- Jaroslav Škarvada
- Johannes Truschnigg
- Pascal Nasahl
- Renat Sabitov
- rottenpants466
- Simon Howard
- Steven Rostedt
- Zentaro Kavanagh
