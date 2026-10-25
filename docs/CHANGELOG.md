# PERF-1 changelog

## A note on version numbers

**0.6.0 is the first public release.** Its code is identical to the last development build, which was numbered **0.12.8** in the experimental line.

Before the first release the firmware was developed on two lines: a "main line" (0.3.x to 0.5.1) and an experimental `vis` branch that went through the numbers 0.4.0 to 0.12.8 on its own. The commit messages in the git history use those development numbers, so you will see 0.9.x, 0.10.x, 0.11.x and 0.12.x there; they are all contained in 0.6.0. From here releases continue from 0.6.0 (0.6.1, ...) and the development numbers are retired.

The unit reports its version as `FM-1_7XXYYZZ` (0.6.0 is `FM-1_7000600`); the installer checks it.

## 0.6.0

What the development builds added, in the order they came (the numbers are the development builds):

- **0.3 - 0.4:** the main line's pieces, FX menu and performing tools; then rolls and STAGE, a texture layer, easier menus, the JAM page, six more genres (jungle, DnB, breaks, garage, electro, trap), and the sequencer's tricks (Euclid, rotation, probability).
- **0.5 - 0.7:** the plate reverb on the FM-1's second core, then the pad as well; a six-operator FM engine and 22 DX7-style voices for the pluck.
- **0.8 - 0.9:** the master bus (compressor and limiter), the Mixer page with meters; the pad's chorus; reverb types (plate, shimmer, gated); the auto-DJ's drop, fill and SYNC; the full-screen screensaver and visualizers; load shedding; DJ snips; settings that are remembered; polymeter.
- **0.10:** USB telemetry (a status log over MIDI) and a long run of measurements and fixes that made the heavy pieces (DnB, jungle) hold their audio: a smarter second core, integer FM buses, a cheaper master stage.
- **0.11:** load your own DX7 voices (`.syx`) over USB; the web editor with a DX7 voice editor; crash fixes found in testing; a review of the whole firmware and its fixes.
- **0.12:** the **Groove** page (lane density, per-lane source pattern), held pattern keys, a chord loop, scenes that carry the groove; **the sigil** in the middle of the home screen; the **Imp** look in six palettes; each genre's own chord habits; **the acid answer** (nine kinds, up and down, every 2nd to 8th bar or loop, or before the drums change); **storage that keeps a piece in every genre** (a packed 376-byte record per piece, a flash object per genre); new air and rave horns; a pump that can follow the kick; many sound and timing fixes (the vinyl, the drop, the save pause).

Each development build is one commit in the git history, and its message says what it changed.
