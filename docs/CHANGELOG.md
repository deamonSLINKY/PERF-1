# PERF-1 changelog

## A note on version numbers

**0.13.0 is the first public release.** Its code is identical to the last development build, 0.12.8.

Before the first release the firmware was developed on two lines: a "main line" (0.3.x to 0.5.1) and an experimental `vis` branch that went through the numbers 0.4.0 to 0.12.8. The commit messages in the git history use those development numbers; they are all contained in 0.13.0. The leading 0 stays until PERF-1 reaches its first finished state, which will be 1.0.

The unit reports its version as `FM-1_7XXYYZZ` (0.13.0 is `FM-1_7001300`); the installer checks it.

## 0.14.0 (in development; the builds 0.13.1 .. 0.13.5 are its steps)

- **Steps page** (0.13.1): a ratchet (x2, x3, x4, x6, flam, swells), a push (late, in twelfths of a step) and a level (ghost, soft, accent) for each step of each lane, stored with the piece in the record's spare bytes (older pieces load with none); some genres ship with tricks.
- **Bass line page** (0.13.2): the drone as notes (Root, Kick, Eighths, Off-beat, Walk, Riff) with Length, Move and Glide, a genre's own rhythm and riff; lofi, house, dubstep, electro, trap and the newer genres start as a bass line.
- **Deja vu page** (0.13.3): the pluck remembers its last 4 to 32 notes and brings each round again with a chance (Deja vu, Loop, Leaps); Acid mut chooses what the acid's Mutate changes (all, notes, rhythm, accents).
- **Feel page** (0.13.4): the genre's ghost notes, how much accents and ghosts count, the pad's comping rhythm (auto, hold, seven rhythms), humanize; per-genre comping, ghost steps and accents.
- **Six more genres** (0.13.5): disco, funk, reggae, synthwave, amapiano and trip-hop, sixteen in all, each with its own flash object.
- **Phrase page** (0.13.6): a chain of patterns by bar (nine presets, the Amen's 1 1 3 4 among them), eight slice permutations of the drum steps (and an automatic one), half and double time, and a condition for each drum lane (odd or even bars, first or last of four, first of eight, fills only, not in fills).
- **Backup and restore** (0.13.7): the installer page saves everything the FM-1 has stored (pieces in every genre, settings, DX7 voices, theme, Tune, MIDI out) to a `.p1bak` file and puts it back, so switching firmware does not lose it. SysEx messages `7D 50 42`/`43`/`44`; each store is checked as its loader would take it, then written through the usual A/B commit, and the unit restarts.
- **A third shed step for the first core** (0.13.13): if a piece still cracks after the drone and acid have moved to the second core and the DX voices are capped, the engine now also drops the Echoplex slap, the vinyl noise and uses a cheaper output clip (about 2.5-4 points of the first core), and eases back after 20 s without a crack like the other steps. Aimed at heavy pieces with no DX voices to cap, such as Jungle.
- **Overload moves work to the idle core first** (0.13.13): when a piece starts to crack and the engine begins shedding, the drone and the acid now go to the second core (it sits at 25-30 % in the pieces that crack) before any quality is given up; they come back if the first core ends up waiting for it. Before, shedding also pinned them to the busy first core.
- **DX voices off the critical path** (0.13.13): the first core waits for the second's reverb tank and no longer for its DX voices; those now render while the first core does its master effects, compressor and limiter, and are joined at the end of the chunk. Aimed at the xruns that only ever showed in DX-pluck pieces; the sound is bit-for-bit the same. A core lost at the drone-and-acid wait is now retired before its half-written buffers are read.
- **Swing and looseness for every voice** (0.13.13): the acid, the bass line and the pad's stabs follow the Swing setting with the drums and the pluck (they used to play straight against swung hats in Garage, Lofi, Amapiano, Reggae and Funk), and Humanize loosens each by a few milliseconds (acid 6, bass 4, stabs 8 at 100).
- **Chord habits per genre** (0.13.13): each genre has its own moves between chords (ii-V-I for lofi, amapiano and disco, i-VII-VI-VII for house, i-VI-III-VII for synthwave, the i-IV vamp for funk and reggae), a pull home, no A-B-A turn-back where it does not belong, and a cadence at the end of each phrase of four (or eight) chords. Before, every genre was close to the same random walk.
- **Flex Beat takes one FX bit** (0.13.13): the pattern is a number sent to the engine, so the 14 bits it used are free and the Flex bank can grow past fifteen patterns. Nothing changes in use.
- **Safer holds and the FX menu** (0.13.13): factory reset is now SEL + SAVE held on Global (SAVE alone only stores, so a long press can no longer wipe a piece); GLO's mix map comes at 0.4 s like the other holds; a page's button closes the FX menu; HOME + SELECT sets the tempo inside the FX menu; OCT- resets the mix only on the FX keys page.
- **Fixes from the audit** (0.13.13): Flex Beat slices below 126 BPM keep the pattern's shape (Half repeat, Double and Back-forward no longer play in a scrambled order at slow tempos); a pushed hit on a swung step is no longer lost when the next step also waits; the swung pluck note no longer sounds after Stop or Panic; SAVE during a restore no longer cancels it.
- **Acid steps page and freeze** (0.13.12): the acid line shown as sixteen steps you can edit (rest, note, accent, slide, note, octave), frozen so Mutate leaves it, and stored with the piece (18 bytes in the record's spare room).
- **Eight new DX voices** (0.13.11): steel, clean, muted and drive guitars, slap bass, synth brass, sax and muted trumpet (thirty built in); the player's own voices shift up eight places in stored pieces and are renumbered when an older piece loads.
- **Seamless saves** (0.13.9): a flash erase in slices with the chip's erase-suspend command, so a store while playing no longer fades the music; the old fade and blocking erase come back if the chip cannot suspend.
- **Fixes** (0.13.8): the new genres' effect tables, a limiter made safe against a bad sample, vinyl crackle, drone renamed bass everywhere, a battery indicator, long knob values fit their card.
- Ideas from **Tuna** by delirioux (GPL-3.0), written afresh for PERF-1's engine; credited in the manual and LICENSING.

## 0.13.0

What the development builds added, in the order they came (the numbers are the development builds):

- **0.3 - 0.4:** the main line's pieces, FX menu and performing tools; then rolls and STAGE, a texture layer, easier menus, the JAM page, six more genres (jungle, DnB, breaks, garage, electro, trap), and the sequencer's tricks (Euclid, rotation, probability).
- **0.5 - 0.7:** the plate reverb on the FM-1's second core, then the pad as well; a six-operator FM engine and 22 DX7-style voices for the pluck.
- **0.8 - 0.9:** the master bus (compressor and limiter), the Mixer page with meters; the pad's chorus; reverb types (plate, shimmer, gated); the auto-DJ's drop, fill and SYNC; the full-screen screensaver and visualizers; load shedding; DJ snips; settings that are remembered; polymeter.
- **0.10:** USB telemetry (a status log over MIDI) and a long run of measurements and fixes that made the heavy pieces (DnB, jungle) hold their audio: a smarter second core, integer FM buses, a cheaper master stage.
- **0.11:** load your own DX7 voices (`.syx`) over USB; the web editor with a DX7 voice editor; crash fixes found in testing; a review of the whole firmware and its fixes.
- **0.12:** the **Groove** page (lane density, per-lane source pattern), held pattern keys, a chord loop, scenes that carry the groove; **the sigil** in the middle of the home screen; the **Imp** look in six palettes; each genre's own chord habits; **the acid answer** (nine kinds, up and down, every 2nd to 8th bar or loop, or before the drums change); **storage that keeps a piece in every genre** (a packed 376-byte record per piece, a flash object per genre); new air and rave horns; a pump that can follow the kick; many sound and timing fixes (the vinyl, the drop, the save pause).

Each development build is one commit in the git history, and its message says what it changed.
