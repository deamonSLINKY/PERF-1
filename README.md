# PERF-1: live-performance firmware for the M-VAVE FM-1

PERF-1 turns the M-VAVE FM-1 into a self-playing groovebox you perform on: ten genres of ten generative pieces, a pad, a drone, an acid line and a pluck (or one of 22 DX7-style voices, or your own) over drums you sequence, with an auto-DJ that takes the set through breakdowns, builds and drops. By **deamonSLINKY**.

**Site (install, emulator, editor, manual):** <https://deamonslinky.github.io/PERF-1/>

- **Install it** from the site, in Chrome or Edge with the FM-1 on USB (<https://deamonslinky.github.io/PERF-1/install/>). Installing firmware is at your own risk; the installer can restore M-VAVE's own firmware, and an FM-1 that fails to start twice comes up in a safe mode the installer can reach. Read section 1 of the manual first.
- **Try it in the browser** (no FM-1 needed): the same code with sound, at `/emu/`.
- **Manual:** `docs/MANUAL.md` (the site has it as a page with screenshots).
- **Editor:** every parameter, and a DX7 voice editor for the 16 voice slots (algorithms, envelopes, live to the pluck, `.syx` in and out), at `/editor/`.

## What it does

- **Ten genres of ten pieces:** HYPNO (the original techno and acid set), lofi, house, dubstep, jungle, DnB, breaks, garage, electro and trap, each with its own sounds, drum patterns, effects and chord habits. **Any piece can be stored** (Global, SAVE); each genre has its own place in flash.
- **Voices:** 24 pads, 24 drones, 31 acid sounds, 14 drum kits; the pluck is 12 plain sounds, 22 DX7-style voices from a built-in FM engine, and up to 16 DX7 voices of your own (load a `.syx` bank from the installer, the editor or any MIDI librarian).
- **The acid answers:** the line asks and then answers (octave, fifth, scale third, mirror, reverse, rotate, sparse, accents; up or down; every 2nd to 8th bar or loop, or in the bar before the drums change).
- **Sequencer:** six drum lanes of sixteen steps and six patterns; Euclidean fill and rotation, per-lane probability, length (polymeter), density and source pattern; drum drift (patterns that change and mutate by themselves); held pattern keys.
- **Performing:** break, build and drop on one button; an auto-DJ (groove, fill, breakdown, build, drop; follows the chords or a count); scenes; undo; a Mood macro; JAM keys; a four-bank FX menu (beat repeats, filters, gates, a pump that can follow the kick, tape, vinyl, phaser, flanger, talk and wah, scatter and brake, 15 one-bar Flex Beat patterns, and 27 DJ snips including air and rave horns).
- **Screen:** the Imp look in six palettes, with the sigil in the middle of the home screen (a summoning circle that turns to the chord's root, draws the chord as a shape and watches the section with a demon eye).
- **MIDI and USB:** MIDI clock in, notes and clock out over USB, the FM-1's TRS jack as MIDI in, USB audio, a remote mode that plays the browser emulator, and USB telemetry (`tools/fm1log.py`).
- **Two cores:** the reverb (and, under load, more) runs on the FM-1's second core; the engine sheds work before it drops audio, and shows its load on the Global mix map.

## Building it

`BUILDING.md` has the toolchain (JieLi's, Linux x86-64), the build and the tests. In short: `tools/release.sh 0.6.0` builds and tests everything, `python3 tools/make_pages_site.py 0.6.0 build/site` makes the site.

## Credits and licence

PERF-1 is a fork of **HYP-1** by [RedtailWorks](https://www.redtail.works), itself a fork of **AMB-1** by Charles Vestal, built on the platform layer of **Felucca** by Leo Kuroshita (Hügelton Instruments) through X0X and FoMni. The reverb is Jon Dattorro's plate ("Effect Design, Part 1", JAES 1997). The git history keeps every author's commits. The idea of a browser emulator for FM-1 firmware comes from Groove OS by Peter Gombos (no code taken). The community reverse-engineering of the chip, [AL-255/FM-1-RE](https://github.com/AL-255/FM-1-RE), informed the hardware work.

GPL-3.0-only (`LICENSE`); the code's origins and the third-party material (fonts, the JieLi SDK) are in `LICENSING.md`. "M-VAVE" and "FM-1" are trademarks of their owners; PERF-1 is independent firmware, not affiliated with or endorsed by them.
