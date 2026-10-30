<!-- footer: PERF-1 quick manual · 0.15, 30 October 2026 · full manual: deamonslinky.github.io/PERF-1/manual/ · GPL-3.0-only -->

# PERF-1 quick manual

<p class="lead">Eight pages of what to press. The full manual, with screenshots and more detail, is at <b>https://deamonslinky.github.io/PERF-1/manual/</b>; if the two disagree, trust the full manual. The 27 keys run from <code>F3</code> to <code>G5</code>: sixteen white, eleven black.</p>

<div class="cols">

**What it is.** PERF-1 turns the M-VAVE FM-1 into a groovebox that makes up music as it plays. Five tracks start when you switch it on: a slow pad, a bass, a 303-style acid line, a plucked counter-melody and drums you can program, all written on the spot from the key, scale and chord, in sixteen genres of ten pieces. You steer: break it down, build it, drop it, mute and solo, throw effects, edit the drums, play along, store what you like, and record it over the same USB cable (the FM-1 is a USB audio and MIDI device). By deamonSLINKY; a fork of HYP-1 (RedtailWorks), itself a fork of AMB-1 (Charles Vestal). GPL-3.0-only, no warranty. It's still in development and can crash.

## Install in five steps

1. **Try it first.** The emulator at https://deamonslinky.github.io/PERF-1/ runs the same firmware in your browser, with sound.
2. **Keep copies.** PERF-1 can't read other firmwares' projects, and what's stored may be lost when you switch either way: keep your `.syx` files. On PERF-1, the installer's **Back up and restore** card saves everything stored to a `.p1bak` file.
3. **Open the web installer**, https://deamonslinky.github.io/PERF-1/install/, in Chrome or Edge. Plug the FM-1 straight into the computer with a USB *data* cable, and close anything using its MIDI port (a DAW). Allow MIDI with SysEx (the messages the installer uses) when asked.
4. **Press Install.** The line under the button names the version it will write; if it's older than you expect, hard-refresh (Ctrl+F5 or Cmd+Shift+R). Don't unplug or close the page while it writes. The FM-1 restarts into PERF-1 and the page checks the version.
5. **To go back** to M-VAVE's FM-1 V15, use "Back to the stock firmware" at the bottom of the same page: download `FM-1.fwsc` from the link there, choose it (the page checks its SHA-256) and press **Install the stock firmware**. It works from PERF-1, safe mode and update mode; M-VAVE's M-UPGRADE works too.

**Update mode.** Hold **OCT-** and **OCT+** together for 5 s while PERF-1 runs. From 2 s the screen counts `UPDATE MODE IN 3..`; let go to cancel. If an install was cut off, the FM-1 stays in update mode: press Install again. **Safe mode.** Two crashes in a row while starting give a black screen, `PERF-1 SAFE MODE`, with USB on: reinstall, or press **PLAY** to try again. Installing is at your own risk; nothing protects against a damaged boot loader or a power cut mid-write.

</div>

## The panel

| Control | Tap / turn | Hold | With SEL held |
|---|---|---|---|
| **MASTER** | Volume. | | |
| **SELECT** | The pages, in order (31, stops at the ends). FX menu: the bank. Settings: the next page. Visualizers: the next one. | With **HOME** held: the tempo. | |
| **PRESETS** | The piece, 1-10 (wraps). | With **GLO** held: the genre. | The piece, on the next bar. |
| **ALGORITHM** | The mode of the scale. Perform, visualizers: the mood. | | The voice's sound (drum pages: the kit). |
| **KNOB1-4** | The page's values a, b, c, d. FX menu, Perform, visualizers: Filter, Crush, Throw, Wash. | | Sequencer: Euclid, rotate, chance, length. |
| **OCT-** / **OCT+** | An octave down / up: on a voice's pages that voice, elsewhere the song. Sequencer: the drum. FX menu: OCT- turns all off. | Both 5 s: update mode. | Sequencer: clear the lane / the piece's own lane back. |
| **FX** | The FX menu on and off. Twice quickly: the last effect's settings. | | Steps through the effect settings, then back. |
| **SEL** | Shift: hold it with another control. | Alone 0.4 s: what shift does here. | |
| **ENV** | Pad; again Pad env, Pad LFO, Chords. | Any page button 0.9 s: its help card. | On a voice's page: play it by hand; again, back. |
| **LFO** | Bass; again Bass env, Bass LFO, Bass line. | | |
| **ARP** | Acid; again Acid env, Acid line, Acid answer, Acid steps, Pluck, Pluck 2, Deja vu, Pluck steps. | | |
| **EDIT** | The next chord, on the next bar. | | Stopped: starts the music by itself. |
| **GLO** | Global (Root, Theme, Tune, Genre). SAVE here stores the piece. | 0.4 s: the mix map, the load meter. | Remote: the FM-1 plays the emulator. |
| **HOME** | Tempo page; again Perform; again Tempo. Twice quickly: visualizers. Leaves the FX menu. | 0.7 s: the settings. | The sigil's middle: eye or imp. |
| **SAVE** | Breakdown on the next bar; again a build, then the drop. On Global: store. | 0.5 s: the drop. | On Global, 1.5 s: back to factory. |
| **SEQ** | Drums; again Links, Kit, Drum drift, Groove, Steps, Feel, Phrase. In the sequencer: back to Drums. | 0.4 s: the sequencer. | The links on and off. |
| **PLAY** | Play and stop. | | |
| **REC** | Freeze: the chord holds, the reverb rings on, until pressed again. | | Capture: the bar played on Jam into the voice's line. |
| White keys | Add or remove a scale note (the root stays). By hand, Jam: notes. Sequencer: the 16 steps. FX: effects. Perform, GLO held: the mix. | A scene key 0.6 s: store it. | White 1-6 (`F3`..`D4`): pattern 1-6, on the bar. |
| Black keys | The same. Sequencer: which pattern the lane plays (black 1-6; black 7: follow). | | Sequencer: copy this pattern into 1-6. |

---

## The pages on the SELECT dial

**SELECT** steps through 31 pages in this order. The buttons (**ENV**, **LFO**, **ARP**, **SEQ**, **GLO**, **HOME**) jump to their group and step through it; from elsewhere, a button returns to the page you last used in its group. Columns a-d are **KNOB1**-**KNOB4**; on a voice's main page KNOB1 is its level. Pages that shape a sound draw it. Off the dial: the **sequencer** (hold SEQ), **Perform** (HOME on Tempo), the **visualizers** (HOME twice quickly).

| Page | Button | a | b | c | d |
|---|---|---|---|---|---|
| Tempo | HOME | Tempo (70-190) | Evolve | Motion | Drift (how long a chord lasts; HYPNO 20 s - 5 min) |
| Pad | ENV | Level | Bright | Sound (24; the saw sounds sweep a filter) | Detune |
| Pad env | ENV x2 | Attack (0.05-10 s) | Hold (0-8 s) | Release (0.1-12 s) | Rest (0-6 s) |
| Pad LFO | ENV x3 | Rate (1/4 note - 32 bars) | Depth | Shape (6: sine .. drift) | Target (filter, level, pan, pitch) |
| Chords | ENV x4 | Chord (7 kinds, below) | Voicing (close, open) | Chorus (0-100) | Loop (off; 2, 4, 8 chords) |
| Bass | LFO | Level | Sound (24) | Cutoff | Reso |
| Bass env | LFO x2 | Trigger (chord; 1-8 bars; off) | Attack (0.01-4 s) | Decay (0.05-16 s) | Amount |
| Bass LFO | LFO x3 | Rate | Depth | Shape (as the pad's) | Target (cutoff, pitch, level, drive) |
| Bass line | LFO x4 | Mode (Hold, 6 modes, 9 figures; page 6) | Length (how long a note holds) | Move (octaves, fifths ..) | Glide |
| Acid | ARP | Level | Cutoff | Reso | Env mod |
| Acid env | ARP x2 | Decay | Accent | Slide | Sound (31) |
| Acid line | ARP x3 | Density | Length (1-16) | Range (1-3 oct) | Mutate (SEL + knob: what it changes) |
| Acid answer | ARP x4 | Answer (off, 8 kinds, page 6) | Every (2nd .. 8th, drift) | Direction (down 2 .. up 2) | Sync (bar, loop) |
| Acid steps | ARP x5 | Step (rest, note, accent, slide) | Note (0-11 scale steps) | Octave (0-3) | Frozen (free, frozen) |
| Pluck | ARP x6 | Level | Sound (12 + 30 DX + yours) | Density | Octave (-2 to +2) |
| Pluck 2 | **ARP** x7 | Decay | Style (up, down, up-down ..) | Rate (1/4, 3/16, 1/8, 1/16) | Range (1-3 oct) |
| Deja vu | ARP x8 | Pluck vu (0 new .. 100 locked) | Loop (4, 8, 16, 32 notes) | Leaps (an octave away) | Motif (develop the first bar) |
| Pluck steps | ARP x9 | Step (rest, soft, note, accent) | Note (1-12, the chord's notes low to high) | Octave (-2 to +2) | Frozen (free, frozen) |
| Drums | SEQ | Level | Drive | Swing (50-75 %) | Pattern (1-6, on the bar) |
| Links | SEQ x2 | K>Bass | S>Pad | H>Acid | P>Clock (off, clock) |
| Kit | SEQ x3 | Kick | Snare (and clap) | Hats (closed and open) | Perc |
| Drum drift | SEQ x4 | Change (off, next, random) | Every (the chord, 4-64 bars) | Pool (patterns 1-2 .. 1-6) | Mutate (off, shift, morph) |
| Groove | SEQ x5 | Lane (kick .. perc) | Density (0-100; 50 as written) | Source (follow, pattern 1-6) | All dens |
| Steps | SEQ x6 | Lane | Ratchet (x2 .. x6, flam, swell) | Push (0-7 twelfths late) | Level (normal, ghost, soft, accent) |
| Feel | SEQ x7 | Ghosts (0-100) | Accents (0-100; 50 as marked) | Pad comp (auto, hold, 7 rhythms) | Humanize (0-100) |
| Phrase | SEQ x8 | Chain (off, 9 chains) | Slice (off, 8 orders, auto) | Time (normal, half, double) | Cond (when the lane plays) |
| Space | dial | Echo | Echo time (1/8 .. 1 bar) | Reverb | Size |
| Reverb | dial | Type (plate, shimmer, gated) | Damping | Predelay (6-30 ms) | Amount |
| Comp/Limit | dial | Ratio (off - 10:1) | Thresh (-30 - 0 dB) | Make-up (0 - +12 dB) | Ceiling (-12 - 0 dB) |
| Mixer | dial | Track (pad .. drums) | Level | Pan (L100 - C - R100) | Send (0-100) |
| Global | GLO | Root | Theme (6) | Tune (+-50 cents) | Genre (the bank) |
| Jam | dial | Voice (pluck, pad, acid, bass) | Keys (scale, chords, harp) | Octave (-2 to +2) | Answer (off, on) |

<div class="cols">

**What changes by itself.** Every few bars the chord changes. Each genre has its own pace and its own habits for where the chord goes next; **Drift** makes the pace up to four times faster or slower (in HYPNO, 20 s to 5 min a chord), and **EDIT** moves on now. The pad's notes swell and fade, taking the new chord as they swell. The bass follows the root. The acid's riff moves with the chord, and **Mutate** rewrites a few steps each time round. The pluck plays the chord's notes one at a time (an arpeggio). **Motion** slowly moves the song between sparse and full; **Evolve** makes the acid and chords change more.

**Tempo** 70-190 BPM. Hold **HOME** and turn **SELECT** on any page, or tap it in: hold **GLO** and tap `E4` four times or so. MIDI clock from another device (its tempo, sent over MIDI) takes over, and the screen shows `EXT`.

**Octaves.** **OCT-** / **OCT+** move the voice whose page you're on (the Pad, Bass or Acid pages and their env and LFO pages; the Pluck pages move the pluck). On any other page they move the whole song. You'll see `OCT+2` or `SONG-1` above the knobs. It's stored with the piece.

**Chords** (ENV x4) are built from the scale's notes. **Auto**: 7ths on a 7-note scale, triads (three-note chords) on a pentatonic. **Triads**: Am, C, G. **7ths**: Am7, Dm7, CM7. **9ths**: root, 3rd, 7th, 9th. **Sus**: root, 4th, 5th and 7th, spread out. **Dub**: one minor 9 shape on every chord, the dub techno stab. **Fourths**: stacked fourths, a hollow m11 sound. **Voicing** is how the notes are spread: Open drops the second note from the top an octave. **Chorus** thickens the pad (0 off; the genres use 20-50). **Loop** repeats the last 2, 4 or 8 chords, counting from the one playing; with a ~, one is swapped for a new one now and then. House, disco, garage, trap and synthwave pieces bring a loop of four; dubstep, DnB, electro, amapiano and trip-hop two.

**Sound words.** Cutoff is where the filter starts cutting the highs (lower is darker); Reso adds a squelchy peak there. An env (envelope) shapes each note: attack fades in, decay and release fade out. An LFO is a slow wobble: Rate is its speed, Depth how far, Target what it moves.

**Space** is the echo and reverb send. Echo is how much echo, Echo time the gap between repeats, in time with the tempo (1/8, 3/16, 1/4, dotted 1/4, 1/2, 1 bar), Reverb how much goes into the reverb, Size how long it rings. On **Reverb**, choose Plate (Amount does nothing), Shimmer (the tail rings an octave up; Amount sets how long) or Gated (cut off after 80-400 ms, set by Amount).

**Comp/Limit** works on the whole mix, before **MASTER**: a stereo compressor evens out loud and quiet, then a limiter stops anything passing the Ceiling. The page draws the curve and meters COMP, LIMIT and OUT. Default 1.5:1 at -12 dB, ceiling 0 dB; each genre brings its own. **Mixer** sets one track at a time, with a fader and meter for each: Level (as on its own page), Pan (left-right balance), Send (to the echo and reverb: 50 as the piece has it, 0 none, 100 double). All stored with the piece.

</div>

---

## The FX menu

<div class="cols">

**FX** opens and closes the menu; **HOME** or a page button also leaves it. While it's open every key is an effect, and **SELECT** steps through four banks: **FX**, **BEAT FX**, **FLEX BEAT**, **DJ SNIPS**. **Tap** a key (let go within 0.25 s) to latch the effect on; **hold** it to play it only while the key is down. Tap a latched key again to turn it off. Several can be latched at once (a bar beside each).

**Latched effects keep going after you close the menu.** The **FX** button blinks, `FX` shows by the tempo, and the keys are the scale again. **OCT-** in the menu turns everything off: effects, FX knobs, mutes and solos (GLO + `B3` undoes it). The FX knobs Filter, Crush, Throw and Wash are the knobs here, on Perform and on the visualizers (page 4).

**Settings pages.** Each effect with settings has a page that draws it: **SEL + a key** in the menu (that key's effect), **FX twice quickly** (the last one used), **SEL + FX** (all 23 in turn). **SELECT** moves between them and **FX** closes. The effect plays while its page is up, so you hear what you change (a few need a key held). All kept across power-off. The knobs on each page: Gate: Pattern (3-3-3, tres, offbeat, Euclid 5/7/9, waltz, chop, sparse, dense, dotted, 5/10, or your own from its white keys), Length, Targets, Depth (Gate 16 and 8 share it). Phaser and Flanger: Rate, Depth, Feedback, Mix. Vinyl: Level, Crackle, Hiss, Tone. Tape: Wow, Flutter, Drive, Dull. Roll: From, To, Each, Swell. Pump: Depth, Release, Every (1/2 .. 1/16 or KICK), Targets. Riser: Time, Filter, Wash, Echo. Scatter: Chaos, Reverse, Slice, Slices. Brake: Time, Spin back, Return, Dull. Half time: Speed, Slide. Sweep: Every, Depth, Reso, Type. Scratch: Every, Reach, Style. Freeze: Size, Spread, Grains. Rv swell: Length, Mix. Ring mod: Tone, Mix. Talk: Every, Depth, Reso, Mix. Wah: Sens, Range, Reso, Mix. Flex: Quant (off, beat, bar: when it starts), Mode (one shot, or loop until you let go or tap it off), Mix. Snips: Pitch (normal, an octave up or down), Play (while held, or one shot of about a second), Echo (dry, some, lots), Level. Auto DJ, Settings, USB & MIDI: pages 4 and 7.

### Bank 2: BEAT FX

| Keys | Effect |
|---|---|
| `F3` .. `C4` | Beat repeats (a slice looped) 1/2, 1/4, 1/6, 1/8, 1/16, 1/32, 1/8T, 1/16T; the shortest held wins |
| `D4` `D#4` `E4` | Scatter (slices shuffled), Brake (a vinyl brake), Spin back |
| `F4` `F#4` | Half time, Scratch (automatic) |
| `G4` `A4` `B4` `C5` | Mutes, as in bank 1: kick, hats, snare, perc |
| `G#4` `A#4` | Reverse, Tape stop |
| `C#5` .. `F5` | Hat roll (32nds), Hat trip (triplets), Snare roll (louder through the bar), Flam (a grace note before each snare), Double kick |

</div>

### Bank 1: FX

| Key | Effect | Key | Effect |
|---|---|---|---|
| `F3` | Sweep: a filter swept in time with the tempo | `F#4` | Tape: wow, flutter, drive |
| `F#3` | Gate 16: the synths chopped in 1/16s (trance gate) | `G4` | M:KICK, a momentary mute |
| `G3` | Ring mod: a metallic tone | `G#4` | Roll: repeats that speed up |
| `G#3` | Gate 8: the same in 1/8s | `A4` | M:HATS |
| `A3` | Freeze: holds the sound still | `A#4` | Pump: the mix ducks in time (sidechain) |
| `A#3` | Gate pattern: the gate with its own pattern | `B4` | M:SNARE (snare and clap) |
| `B3` | Reverse swell | `C5` | M:PERC |
| `C4` | Talk: a vowel filter, A E I O U | `C#5` | Riser: a build, filter and wash climbing |
| `C#4` | Phaser | `D5` | Throw: sends the mix into the echo |
| `D4` | Wah: an auto-wah that follows the loudness | `D#5` | Vinyl: crackle, hiss and pops |
| `D#4` | Flanger | `E5` | Fill: a drum fill |
| `E4` | Reverse | `F5` | Low-pass, closing in |
| `F4` | Tape stop | `F#5` | FILL>: a drum fill in the next bar (tap) |
| | | `G5` | High-pass |

### Bank 3: FLEX BEAT (27 one-bar reworks of the beat; held, a key repeats its bar, then lets it play out)

| Key | Pattern | Key | Pattern | Key | Pattern |
|---|---|---|---|---|---|
| `F3` | Bt Rept: the last beat looped | `D4` | Roll out | `B4` | Slowmo: each 16th twice |
| `F#3` | Hf Rept: the last half bar | `D#4` | Build: live, then a stutter | `C5` | Skip 3: three against four |
| `G3` | Bt Rev: the beat backwards | `E4` | Scratch | `C#5` | Stut up: a stutter, faster |
| `G#3` | Skip | `F4` | Dropout | `D5` | Stut dn: a stutter, slower |
| `A3` | Trance: a 3-3-2 gate | `F#4` | Jump bk: the last beat again | `D#5` | Tresillo: 3-3-2 |
| `A#3` | Gate 8 | `G4` | Halves: half bar swapped | `E5` | Offbeat: off-beats only |
| `B3` | Double | `G#4` | Swap: 16ths in pairs | `F5` | Cut 4: a gap every 4th |
| `C4` | Shuffle | `A4` | Mirror: forwards, then back | `F#5` | Rev 8: half bar backwards |
| `C#4` | Bck-Fwd | `A#4` | Lag: late, with a gap | `G5` | Half gap: half bar, then rest |

### Bank 4: DJ SNIPS (27 sounds made on the spot; up to four at once; they play while held and never latch)

| Key | Snip | Key | Snip | Key | Snip |
|---|---|---|---|---|---|
| `F3` | AIR HORN (held) | `D4` | ZAP | `B4` | DUB LOW |
| `F#3` | AIR BLAST (short) | `D#4` | ZAP HI | `C5` | DROP |
| `G3` | HORN MIN (minor, held) | `E4` | BEEP | `C#5` | DROP LONG |
| `G#3` | RAVE HORN (hoover, held) | `F4` | BEEP LO | `D5` | RISER |
| `A3` | SIREN | `F#4` | BEEP HI | `D#5` | REWIND |
| `A#3` | SIREN 2 | `G4` | 2 BEEP | `E5` | CRASH |
| `B3` | SIREN 3 | `G#4` | WHOOP | `F5` | RAVE STAB (hoover stab) |
| `C4` | LASER | `A4` | WHOOP 2 | `F#5` | BELL |
| `C#4` | LASER LO | `A#4` | DUB SIREN | `G5` | SNR ROLL (speeds up) |

---

## Performing

<div class="cols">

- **Breakdown: tap SAVE.** In the bar before, the percussion, open hat and half the hats drop out. On the next bar the drums stop (house, techno, dubstep and dub keep the kick; DnB the hats), the mix darkens, the bass closes and the pad rises, for as long as you like.
- **Build: tap SAVE again.** A snare roll climbs as the bass opens, for four bars (two in trap, lofi and trip-hop; eight in house). In the last bar the open hat plays every eighth and, in the four-on-the-floor genres, the kick comes back. Then the drop. The header counts `DROP 3`.
- **Drop: hold SAVE** (0.5 s). Everything is back on the next bar with a ringing crash, mostly on the home chord. SAVE is lit in a breakdown or build and blinks while it waits for the bar.
- **Freeze: REC.** The chord holds and the reverb rings on until you press REC again. **Next chord: EDIT**, on the next bar.
- **The scale: the keys.** Each adds or removes its note; the root stays. **ALGORITHM** picks a mode (Aeolian is plain minor, Dorian a brighter minor, Phrygian a darker one, Mixolydian major with a flat 7th). Root is on Global.
- **Patterns:** KNOB4 on the Drums page, or SEL + white keys 1-6; the change waits for the bar. Hold a pattern key past 0.25 s and it plays only while held, then the one before comes back.
- **Pieces:** PRESETS; with SEL held, on the next bar. **Genres:** GLO held + PRESETS, or Global's Genre knob.
- **Songs aren't saved:** record the audio or MIDI out into your DAW.

### The Perform screen

**HOME** on the Tempo page (elsewhere, twice). It shows the section (GROOVE, BREAK, BUILD) and the auto-DJ's place in it, the bar's 16 steps, the eight tracks (a line through a muted one, a soloed one filled), the four scenes, the effects on, and the mood. Keys: the track mix below. Knobs: the FX knobs. **ALGORITHM**: the mood.

**The FX knobs.** **Filter**: left of centre cuts the highs (16 kHz down to about 250 Hz), right cuts the lows (20 Hz up to about 1.6 kHz), `OFF` in the middle. **Crush**: lo-fi grit (fewer bits, lower sample rate). **Throw**: more of the mix into the echo, for longer. **Wash**: the mix into the reverb. They go home at power-off, but a scene stores them. `KNOB` in the header: one is away from home.

### The mix map (GLO held)

Hold **GLO** (0.4 s), or use the keys on Perform or a visualizer with FX closed. A mute stays on until pressed again; the black key above a track solos it.

| Key | Does | Key | Does |
|---|---|---|---|
| `F3` / `F#3` | Mute / solo the kick | `C5` `C#5` `D5` `D#5` | Scenes 1-4 |
| `G3` / `G#3` | Hats | `E5` | Reset all |
| `A3` / `A#3` | Snare (with the clap) | `F5` | Auto-DJ on / off |
| `B3` | Undo (again: redo) | `F#5` | Solo the pluck |
| `C4` / `C#4` | Perc | `G5` | DJ new pieces on / off |
| `D4` / `D#4` | Bass | `E4` | Tap tempo |
| `F4` / `F#4` | Acid | `A4` / `A#4` | Mute / solo all four drums |
| `G4` / `G#4` | Pad | `B4` | Mute the pluck |

- **Scenes** store what's latched in FX, the mutes and solos, the FX knobs and the groove (each lane's density and source, and the pattern playing). **Tap** a scene key to recall it (on the next bar if the Auto DJ page's Scenes is `NEXT BAR`, the default; the FX knobs glide over about a second). **Hold** it 0.6 s to store what's playing now. Kept until power-off.
- **Undo** (`B3`) goes back to before the last big change (a scene, a reset, all drums muted, a clear); again to redo. **Reset all** (`E5`): nothing latched, no mutes or solos, the FX knobs home; you can undo it. **Tap tempo** (`E4`): four taps or so.

### The auto-DJ

GLO + `F5` runs breakdowns and drops for you (`DJ` in the header). On the bar it plays (1) a **groove** of 8, 16, 32 or 64 bars (16 by default); (2) a **drum fill** as its last bar, if Fill is on; (3) a **breakdown**: short (4 bars), half the groove (at least 4; the default) or full; (4) a **build** of the genre's length, the riser rising and two sixteenths of silence at the end, then the **drop**; (5) the groove again. The header counts `BREAK IN 4`, then `BREAK 2`. Groove on **Sync** (its fifth setting) lasts until the chord changes (Drift sets when): the bar before is a fill, the breakdown starts with the new chord. **DJ new pieces** (GLO + `G5`) brings in the bank's next piece on the fill's bar, so the breakdown and drop are its own. SAVE by hand just moves the DJ on.

The **SET** settings page (after USB & MIDI): **Rounds** a piece plays before the next (1, 2, 4, 8; `R2/4` in the header). **Ramp**: the tempo climbs +2 to +12 BPM over a piece's rounds. **Intro**: after PLAY the tracks come in one by one over 1, 2, 4 or 8 minutes, pad first, acid last; `INTRO 1:42` counts down and the DJ waits. The **Auto DJ** settings page: Scenes (at once, next bar), Groove (8, 16, 32, 64, sync), Break (short, half, full), Fill (off, on).

### Mood

On Perform and the visualizers **ALGORITHM** sets the energy: 0 chill, 50 (default) the piece as it is, 100 peak. It nudges the acid's density and cutoff, the hats' and perc's levels, the pad's brightness, the pluck's density, level and decay, the bass's cutoff and drive, reverb and echo (less at peak), Evolve and the drums' level. It changes what plays, not what's stored, and is forgotten at power-off.

### Drum drift (SEQ x4)

Want the drums to change by themselves? **Change**: off, the next pattern, or a random one from 1 up to the **Pool** limit, **Every** 4-64 bars or with each chord change. The bar before a change ends in a fill. **Mutate** rewrites a little at the same moments: Shift moves a hit or two a step, Morph also adds or removes one. The kick and clap stay as written, and a snare on a beat never moves. Mutate works with Change off too. Off by default; these four settings stay when you change piece or genre.

### What the screen says

**Top left:** the piece's tag (`01` HYPNO, `L03` lofi ..; a mark on it means unstored changes), the root and the mode. **Top right:** the mascot, the tempo (`EXT` on an outside clock; a play triangle while stopped), then the marks: **BREAK**, **BUILD** (with `>`: waiting for the bar; `DROP 3`: bars to the drop), **KNOB** (an FX knob off home), **KEY** (the drop's key change is on), **S1>** (a scene waiting), **DJ**, **FX** (effects latched, menu closed), a small dot (a save to memory waiting).

**Left:** the levels of pad (P), bass (D), acid (A), pluck (C) and kit (K). **Middle:** the **sigil**, turned so the chord's root is at the top. The scale's notes are lit and the chord's notes are joined into a shape (a triad is a triangle, a seventh chord four-sided), with its name under the eye. Around it run the 4-bar phrase and the bar's 16 steps. The eye is half open in a groove, shut in a breakdown, wide in a build. **Right:** the pattern playing (`PAT 1`; `> 3` when one waits) and the load line (page 7). **Above the knobs:** the voice's sound, `BY HAND`, `+LINK` or `LINKS OFF`, the octave when it isn't home, the page's name and dots. On a change of page the header says where you are: `DRUMS  PHRASE 8/8`.

</div>

---

## The sequencer and the drums

<div class="cols">

**Hold SEQ** (0.4 s) to open the sequencer; tap **SEQ** to go back to the Drums page. Six lanes (kick, snare, clap, closed hat, open hat, perc), sixteen steps, six patterns a piece (roughly intro, groove, build, peak, variation, and a thinner one for mixing). The sixteen **white keys** are the steps of the drum you're on, lit where it hits, with the playhead running across: tap one to add or remove a hit. **OCT-** / **OCT+** choose the drum. The knobs: the drum's **Level**, **Tune** (+-1 octave), **Decay**, and the **Pattern** you're editing (changing the playing one waits for the bar).

| In the sequencer | Does |
|---|---|
| SEL + white 1-6 | The pattern to edit (and play, on the bar). |
| SEL + black 1-6 | Copy this pattern into pattern 1-6. |
| SEL + OCT- / OCT+ | Clear this drum's lane / bring back the piece's own lane. |
| SEL + KNOB1 | Euclid: fill the lane with n hits spread as evenly as possible over 16 steps (0 clears). |
| SEL + KNOB2 | Rotate it, 0-15 steps. |
| SEL + KNOB3 | The chance that each hit plays, in steps of 5 %. |
| SEL + KNOB4 | The lane's length, 1-16 steps (`L5` under its name). A shorter lane loops against the others (polymeter), counting from the start of the song. |
| Black 1-6; black 7 | Play this lane from pattern 1-6 instead; black 7 follows the piece again. |

Euclid and rotate change the pattern itself. Chance, length, density and source are stored with the piece separately.

### Groove (SEQ x5)

This page draws every lane as it plays: a bright hit as written, a white one added, a dash one taken out. **Density** (50 = as written) thins or fills a lane without editing the pattern: down takes out the least important hits first (off-sixteenths, then off-eighths, the beats last), up adds hits on the most important empty steps. **All dens** moves every lane. **Source** makes a lane play its part from another pattern, from the next bar: the hats from pattern 4 while the kick stays on 1. Scenes remember density and source.

### Steps (SEQ x6): ratchet, push, level

KNOB1 picks the lane, a **white key** the step; the other knobs set that step. **Ratchet** makes the hit a quick roll inside its step: x2, x3, x4, x6, a flam (a quiet grace note just before) or a swell (3 or 4 hits getting louder). **Push** makes it land late, 0-7 twelfths of a step (never early). **Level**: normal, ghost (very soft), soft, accent. These belong to the lane's steps whatever pattern plays, and are stored with the piece. They're skipped in fills, builds, breakdowns, on the hits the drop lands on, and in the first bar after PLAY. Some genres come with them set (soft off-beat hats, a laid-back snare in lofi and trip-hop, trap's triplets and swells).

### Feel (SEQ x7)

**Ghosts** (0-100): the genre's ghost notes, soft snares (and soft kicks in lofi, breaks, funk, trip-hop) on empty steps; lofi, house, DnB, breaks, garage, funk and trip-hop have them. **Accents** (50 = as marked): how much the Steps page's accents and ghosts stand out; at 0 every hit is the same level. **Pad comp**: the rhythm of the pad's stabs (short chord hits) for the stab sounds (Stabs, Dub, Pings, Hits, Rave, Wurli, Mallet, Brass, Pluck): Auto (the genre's), Hold (no stabs), or forced on any pad sound: Off-beat, On-beat, 3-3-2, Charleston, Syncopated, Sparse, Eighths. **Humanize**: each hit a touch softer and later, like a person playing; it loosens the acid, bass line and stabs too.

### Phrase (SEQ x8)

**Chain** plays a different pattern each bar, round and round: 12, 1112, 1123, 1134 (the Amen break's), 1213, 1234, 1232, 1115, 123456. While on, it picks the pattern at every bar line, over the Pattern knob, held pattern keys and drift's changes (drift's fills still play). **Slice** plays the bar's steps in another order: Halves (swapped), Stutter (the first beat of each half repeated), Reverse, Delay (the back half an eighth late), Beats (the middle two swapped), Mirror (the second half backwards), Double (the first half twice), Swap (the beats within each half swapped); Auto picks one for the last bar of every four. **Time**: Half spreads the pattern over two bars (DnB into drumstep), Double plays it twice a bar. **Cond**, when the chosen lane plays: Always, 1:2 (odd bars), 2:2, 1:4, 4:4 (the last of four), 1:8, Fill (only in fill bars), No fill. All stored with the piece.

### Links (SEQ x2)

Drum hits push the other voices, 0 to 100 each; **SEL + SEQ** turns them all on and off (`+LINK`). **K>Bass**: each kick opens the bass's filter, closing again over the beat. **S>Pad**: each snare or clap flashes the pad's filter open. **H>Acid**: a hat on an acid note accents it. **P>Clock**: the acid line steps on only when the perc hits (free again in a breakdown or build). A muted drum still drives its link, so a muted perc is a silent clock for the acid.

**The Drums page:** Level, Drive, Swing (50-75 %: how late the off-beats land; the acid, bass line, stabs and pluck follow it) and Pattern. Store the piece (Global, SAVE) to keep edits.

</div>

### The kits (SEQ x3)

Each knob picks one drum's sound from the same fourteen kits: Kick, Snare (and clap), Hats (closed and open), Perc. **SEL + ALGORITHM** on any drums page turns all four to the next kit. Mix them freely.

| Kit | The kick | The rest |
|---|---|---|
| Hypno | Deep | Metallic 808-style hats, a soft rim |
| House | Round | A wide clap, long open hats, congas |
| Techno | Hard, with a rumble tail | A tight clap, metal hats, a bell perc |
| Acid | Punchy 909-style | A cowbell for the perc |
| Break | Distorted | A cracking snare, noisy hats, a tom; the kick also chops and crushes the drums as they play |
| Minimal | Short and clicky | A rim for the snare, tight hats, a woodblock |
| Hard | Industrial: distorted, a long rumble | A noisy snare, a metal clank; it crushes the drums a little |
| 808 | A long boom | A snappy snare, a wide clap, a cowbell |
| Dusty | Soft and round; the rest dips a little under it | A loose low snare, a muted clap, dull short hats, a soft rim (lofi) |
| Heavy | A deep sub with a long tail; the bass dips under it | A big cracking snare, a wide clap, tight hats, a metal perc (dubstep) |
| 909 | Punchy, with a click | A snappy snare, a clap, ticking hats and long open ones, a tom |
| Electro | Short and high | A tight bright snare, a thin clap, glassy hats, a zappy tom |
| DnB | Tight | A loud cracking snare, a clap, fast tiny hats, a clicky perc |
| Trap | A long 808 sub, almost no click | A snare and a wide clap together, tiny hats, a bell |

---

## The voices

<div class="cols">

### The acid

**Acid** (ARP): Level, Cutoff, Reso, Env mod (how far each note opens the filter). **Acid env**: Decay, Accent, Slide, Sound (31: Saw, Square, Driven, Sync, Pluck, PWM, Chip, Sub, Trance, Bleep, Fifth, Bell, Organ, Laser, Fold, Soft, Marimba, E.Piano, Kalimba, Brass, Gong, Clav, Vibes, Digital, Super, CZ, Mini, Hyper, CZ Pulse, CZ Bell, Lead). **Acid line**: Density, Length (1-16 steps), Range (1-3 octaves), Mutate (how much is rewritten each time round). **SEL + the Mutate knob** chooses what changes: All, Notes only (the classic acid way), Rhythm only, Accents (accents and slides).

**Acid answer** (ARP x4): call and response. **Answer**, how the line changes: Octave, Fifth, Third (two scale steps, so it stays in key), Mirror (the shape upside down round the line's middle), Reverse, Rotate (moved a beat), Sparse (even steps only), Accents (accented notes only). **Every**: the 2nd to the 8th time round, or Drift (the bar before the drums change). **Direction**: down 2, down, same, up, up 2. **Sync**: Bar (the answer is a bar) or Loop (the whole line; for lines that aren't 16 steps). Off by default; stored with the piece.

**Acid steps** (ARP x5): the line as sixteen steps you can edit. A **white key** picks a step. **Step**: Rest, Note, Accent, Slide (glides into the next), Acc+Slide. **Note**: 0-11 scale steps above the chord. **Octave**: 0-3 up. **Frozen**: Free (it keeps changing; each piece grows a new one) or Frozen (held as it is, Mutate leaves it alone, **stored with the piece**). Any edit freezes it. Steps past Length are dim and silent. The answer and the Acid octave still work on a frozen line.

**Pluck steps** (ARP x9): the Deja vu loop as sixteen steps you can edit. Laid out like Acid steps. A **white key** picks a step. **Step**: Rest, Soft, Note, Accent. **Note**: 1-12, the chord's notes from low to high. **Octave**: the whole pluck's. **Frozen** holds the loop (setting Loop 16, Rate 1/16, Pluck vu 100) and **stores it with the piece**; any edit freezes it. Left free, a new piece forgets the loop.

**Capture (SEL + REC)** turns the last bar you played on Jam into the voice's own line. Jam remembers your last 32 keys: play the bar, then press (`CAPTURED`). On the acid: a frozen 16-step line on Acid steps, each sixteenth you pressed a key on that note (scale steps above the chord of the moment), a key on a beat an accent, a key held past the next step a slide; it plays when you leave Jam. On the pluck: into the Deja vu loop (Loop 16, Rate 1/16, Pluck vu 100) from the next bar. The pad and bass can't be captured yet.

### The pluck

The counter-melody: two plucked voices taking turns, an arpeggio of the chord on the bar's grid, swung off-sixteenths. Quiet by default (Level 30, Density 45; Level 0 for none); thinner in a breakdown. **Pluck** (ARP x6): Level, Sound, Density, Octave. **Pluck 2**: Decay (for DX sounds, how long the key is held, 0.12-1.7 s), Style (up, down, up-down, random, skip = every second note, thirds), Rate (1/4, 3/16, 1/8, 1/16), Range (1-3 octaves).

**Deja vu** (ARP x8) makes the pluck remember its last 4, 8, 16 or 32 notes (**Loop**). **Pluck vu** is the chance a note coming round is the same as last time (0 all new, 100 locked); a locked phrase still follows the chord. **Leaps**: how often a new note jumps an octave. **Motif**: the first bar of each eight-bar phrase is the idea, and the bars after develop it (moved up or down, turned upside down now and then, a new ending, back in the fifth bar). 60 by default; 0 makes up every bar fresh. Changing Loop, or a new piece, forgets the loop.

**Sounds:** 12 plain (Pluck, Marimba, E.Piano, Kalimba, Bell, Vibes, Gong, Clav, Digital, Brass, Bleep, Glass), then 30 built-in DX7 voices on a six-voice, six-operator FM synth like the Yamaha DX7 (all 32 algorithms, envelopes, scaling, velocity, LFO, pitch envelope, feedback): DX Tine EP, Glass Bell, Round Bass, Brass Sect, Soft Pad, Wood Bars, Drawbars, Nylon Pick, Brass 2, Solid Bass, Bells, Marimba, Clavinet, Strings, Glass Pad, Sync Lead, Harp, Kalimba, Flute, Steel Drum, Saw Bass, Tubular, Steel Gtr, Clean Gtr, Muted Gtr, Drive Gtr, Slap Bass, Synth Brass, Sax, Mute Trumpet. Then up to 16 of **your own** (page 8), named `DX` and the voice's name. Heavy patches play three or four notes at most. SEL + ALGORITHM on a pluck page steps through the sounds there are.

### The bass

**Bass** (LFO): Level, Sound (24: Saws, Warm, Grit, Dirty, Reese, Pulse, FM, Organ, Rumble, Sub, Square, Hoover, Fifths, Metal, Sync, Bright, Wub, Bite, Tine, Clang, Gnarl, Wide, Power, Air), Cutoff, Reso. **Bass env**: Trigger (when it restarts: each chord, every 1, 2, 4 or 8 bars, or off), Attack, Decay, Amount. **Bass LFO**: Rate, Depth, Shape, Target (cutoff, pitch, level, drive). Dubstep's wobble is this LFO on the cutoff.

**Bass line** (LFO x4): notes that follow the chord. **Mode**: Hold (one long note gliding to each chord's root), Root (the genre's bass rhythm: the beats in hypnotic, a lazy three in lofi, a roll in DnB), Kick (on every kick, as an 808 does), Eighths, Off-beat (the off-beat eighths of house), Walk (on the beats: root, third or fifth, fifth, the scale note below the root), Riff (the genre's figure: disco bounces the octave, funk and amapiano syncopate, reggae leans on the fifth), then nine figures by name in any genre: Octaves, Two-step, One-drop, Log drum, Skippy, Funk, Big beat, Dub, Gallop. **Length**: how long a note holds (a quarter step to five steps). **Move**: how often a note goes up an octave, to the fifth or to the note below the root instead. **Glide**: 0 none, 100 about 0.4 s. Each note opens the filter with the Bass env's attack and decay (Trigger doesn't apply). A breakdown plays no notes. New pieces start with a line in lofi, DnB, trip-hop (Root), house (Off-beat), dubstep, trap (Kick), electro, synthwave (Eighths), disco, funk, reggae, amapiano, breaks, garage (Riff); HYPNO and dub hold.

### The pad

**Pad** (ENV): Level, Bright, Sound (24: Rhodes, Voices, Strings, Glass, Organ, Warm, Stabs, Dark, Dub, Trance, Poly, Hollow, Pings, Hits, Rave, Sine, Ooh, Wurli, Mallet, Brass, Fat, Pluck, Chapel, Air), Detune. **Pad env**, how each chord swells and fades: Attack, Hold, Release, Rest (the gap before the next). For the stab sounds Release is the stab's length. **Pad LFO**: Rate (a 1/4 note to 32 bars), Depth, Shape, Target (filter, level, pan, pitch). Chords and Chorus are on page 2.

### Playing a voice by hand

On any of a voice's pages (**ENV** pad, **LFO** bass, **ARP** acid) hold **SEL** and press **ENV**. It stops playing its part (`BY HAND`); while its pages are up, the keys play it over the song. Pad: up to four notes, with the Pad env's attack and release. Bass: the last key held, two octaves down. Acid: the last key held, an octave down, with its filter and envelope; a second key slides to it, as on a 303. The lit keys still show the scale; on other pages the keys edit it again. **SEL + ENV** again hands it back. Kept with the piece; MIDI out sends what you play.

### Jam (the dial, after Global)

The 27 keys become an instrument and never edit the scale. **Voice**: pluck, pad, acid or bass (that voice stops its own part while the page is up). **Keys**: Scale (every key a scale note, a black key the one at or below it: no wrong notes), Chords (every key a triad of the scale) or Harp (the scale's notes one per key, from C3). **Octave** -2 to +2. **Answer** on: when you stop (two notes, then quiet), the pluck plays your phrase back an octave up at the next bar line. A nearly silent pluck is raised to 40 so you hear it. Then **SEL + REC** captures the bar (above). Any voice's sound: the Sound knob on its page, or **SEL + ALGORITHM** on any of its pages.

</div>

---

## Genres, pieces and what is kept

Sixteen **genres**, each a bank of ten **pieces**. **PRESETS** steps through the bank (01-10, wraps round); **GLO** held + **PRESETS**, or Global's Genre knob, changes bank. The piece number stays, and the genre's own effects come with it (lofi latches vinyl and tape, house the pump; switching first clears latched vinyl, tape, pump and phaser). Each piece has its own settings, six patterns and drum sounds, a set of sounds by its number and a made-up title (`VELVET RAIN`); each genre its own compressor, reverb, stab rhythm, ghost notes, chord habits and section lengths. Power-on always starts in HYPNO on a random piece.

| Genre | Tag | BPM | What it is |
|---|---|---|---|
| HYPNO | 01 | 118-138 | The original hypnotic techno and acid set, from four on the floor to dub techno and warehouse. |
| LOFI | L01 | 70-94 | Swung boom-bap, Rhodes in 7ths and 9ths, a warm bass, the Dusty kit; vinyl and tape latched. |
| HOUSE | H01 | 118-126 | Four on the floor, claps on 2 and 4, off-beat open hats, stabs and organ; the pump latched. |
| DUBSTEP | D01 | 136-150 | Half time, the Heavy kit, a wobble bass (its LFO on the cutoff), a harsh lead, chords in fourths. |
| DUB | V01 | 64-76 | One-drop, steppers, rockers; a deep bass on the one and three, the off-beat skank, a long echo, worn tape. |
| DNB | N01 | 170-178 | Two-step, liquid, roller, neuro, jump-up, half-time; the DnB kit, shimmer reverb, a light pump. |
| BREAKS | B01 | 100-135 | After the classic breaks (Funky Drummer, Impeach, Levee, Cold Sweat); the Break kit, gated reverb. |
| GARAGE | G01 | 126-138 | Swung two-step, skippy, speed garage, a conga kick, rim Q&A; tape and a light pump. |
| ELECTRO | E01 | 118-130 | A syncopated kick, the clap on the backbeat, the Electro kit, gated reverb; the phaser latched. |
| TRAP | T01 | 130-150 | The long 808 (the Trap kit), the clap on 3, hat rolls; shimmer reverb and a pump under the 808. |
| DISCO | S01 | 112-126 | Four on the floor, accented open hats, an octave-bouncing bass, strings and brass stabs, Wurli, clav. |
| FUNK | F01 | 96-110 | Syncopated sixteenths, ghost snares, soft kick pickups, a funk bass figure, a clav line, the Break kit. |
| REGGAE | R01 | 70-84 | One-drop, steppers, rockers; the rim a touch late, a melodic bass, an off-beat skank, the Dusty kit. |
| SYNTHWAVE | W01 | 100-118 | A driving eighth-note bass, gated reverb, a big snare, lots of chorus, an arpeggiating acid. (SYNTHWV) |
| AMAPIANO | A01 | 108-115 | A soft kick with shakers, a gliding syncopated bass (the log drum), jazzy 9ths, marimba, kalimba. (AMAPI) |
| TRIPHOP | P01 | 70-88 | Slow and dusty, the snare laid back, ghosts, vinyl noise, a pluck loop that comes round; minor, sparse. |

| HYPNO | Character | BPM | Key | HYPNO | Character | BPM | Key |
|---|---|---|---|---|---|---|---|
| 01 | Hypnotic four on the floor | 128 | A Aeolian | 06 | Dub techno: chord stabs, sparse, swung | 120 | F Pent minor |
| 02 | Deep and dubby: rims, shakers | 124 | D Dorian | 07 | Minimal glitch: threes on fours | 134 | B Pent major |
| 03 | Pounding: rumble kick, claps | 132 | E Phrygian | 08 | Rolling, tom-like percussion | 126 | Bb Mixolydian |
| 04 | Trance: offbeat opens, rolls | 138 | G Aeolian | 09 | Warehouse: hard, rides, rumble (Techno kit) | 136 | C Phrygian |
| 05 | Acid electro: cowbells, breaks | 130 | A Dorian | 10 | Deep field: half time, breaks | 118 | Eb Aeolian |

<div class="cols">

### Storing a piece

Go to **Global** (GLO) and press **SAVE**. The piece is stored as it is (every page, scale and mode, sounds and kit, patterns and drum sounds, the frozen acid line, the step settings) in the genre you're in, in place of its factory piece. The line above the knobs says `01 SAVE: STORE` while there are changes, then `01 STORED`. Unstored changes last while you stay in the genre and are lost when you change genre or switch off. **Back to factory:** on Global hold **SEL + SAVE** 1.5 s (KEEP HOLDING: FACTORY). A store is written at once; while playing, the music dips for a moment (`STORING`). Don't switch off during it.

### The settings pages

**Hold HOME** (0.7 s) for **Settings**. **Key**: the drop's key change (same, +2, +3, +5, +7 semitones); the root rises for that round, the next breakdown brings it back, `KEY` shows in the header. **Saver**: the screensaver (off, 1, 3, 10 min); the visualizers fill the screen until you touch something. **Lights**: off, or show (the key lights follow the music). **Voices**: DY (the default), DX7 voices made by people, named as their banks name them; or DX, the built-in set. **SEL + FX** goes on to the other settings pages, SELECT moves between them, FX leaves. **USB & MIDI**: Level (KNOB: the USB audio follows MASTER; FIXED: it stays at unity, no boost or cut, like MASTER at three quarters) and MIDI out (off, on). **Auto DJ**: page 4. **Global** (GLO): Root, Theme (Imp, Synth, Acid, Ice, Ember, Phosphor: one look in six colour sets), Tune (+-50 cents), Genre.

### Visualizers

**HOME twice quickly**; HOME again leaves. **SELECT** chooses (remembered): PLASMA, KALEIDO, BARS (a 24-band spectrum), FLUX, DISCO (the mascot dancing to the drums), LIFE, ORBIT (the chord drawn as a pendulum pattern), AUTO. The knobs stay the FX knobs, ALGORITHM the mood, the keys the track mix; FX opens its menu over them.

### What is remembered across power-off

**Kept:** the pieces you stored, in any genre (written at once); theme, Tune and MIDI out; every effect's settings, the gate's pattern and targets; the Auto DJ, SET, Settings, USB & MIDI and Jam pages; the visualizer; your DX7 voices; the panel calibration. Settings are written a couple of seconds after you stop changing them (2 s stopped, 4 s playing), with a short dip in the music. **Not kept:** the genre (HYPNO at power-on), anything latched in FX, the FX knobs, the mood, mutes and solos, scenes, a running auto-DJ, unstored edits. A genre sets its own tape, vinyl and pump settings when you switch to it.

### The load line

How hard the FM-1 is working: `CPU 41 XR 0`, with `C2 38` and `S1` or `S2` under it (home pages; top right of the mix map). **CPU**: how busy the main processor is. **XR**: dropouts, each one a click. **C2 nn**: the second processor, nn % busy (pad and chorus, DX voices, reverb); `C1`: it stopped after a fault. **S1**: saving effort, the chorus off and DX voices held to four; **S2**: to two. It goes back after twenty quiet seconds. Heaviest: DX plucks with the shimmer reverb, and the fast genres.

</div>

---

## The computer side

<div class="cols">

**USB audio.** The FM-1 is a stereo USB audio input (16-bit, 44.1 kHz) carrying the master output. The USB & MIDI page's Level: KNOB (follows MASTER) or FIXED (a recording stays put when you turn the speakers down). Its third knob, **Scanlines**, darkens every other row of the screen, like a CRT.

**MIDI out** (USB): the pad on channel 1, pluck 2, bass 3, acid 4, drums 10 (General MIDI notes), and MIDI clock with start, continue and stop. Notes played by hand go on their voice's channel; the DJ snips aren't sent. On and off on the USB & MIDI page.

**MIDI in:** notes on channel 1 add or remove their note from the scale (use another channel to avoid that). Clock and start / stop drive the tempo (`EXT`). It listens on the USB MIDI port, and the TRS input where your version has it.

**The site**, https://deamonslinky.github.io/PERF-1/: the **installer** (install, back to stock, Load DX7 voices, Back up and restore, Live status), the **emulator**, the **editor** and the **manual**.

**Backup and restore** (installer). **Save a backup** copies everything stored (pieces in every genre, settings, DX7 voices, theme, Tune, MIDI out) to a `.p1bak` file; do it before you install anything else. Unstored edits and the panel calibration aren't in it. **Restore the chosen backup** writes it back one item at a time, then the FM-1 restarts. Needs PERF-1 0.14 or later on the FM-1.

**The editor** has two tabs. **Sound**: every setting of the piece playing, with the FM-1's page names and ranges, plus a page for the rest (lane levels, tunings, decays, chances, lengths; pans and sends; octaves; by-hand switches). Changes play at once; store the piece on the FM-1 to keep them. **DX7 voices**: read the 16 slots, edit a voice (name, algorithm, feedback, LFO, pitch envelope, each operator's level, ratio, envelope, scaling, velocity); **Live** sends each change as you make it; load `.syx` banks, save a slot or the bank as `.syx`.

**Your own DX7 voices**: up to 16, kept after power-off, after the Pluck page's 30 built-in DX sounds. From the installer: **Load DX7 voices**, choose `.syx` files (32-voice banks, single voices, bare 4096-byte banks), tick up to 16, press **Send the chosen voices**. **Clear the loaded voices** empties the slots. From any librarian app, a 32-voice bank dump to the FM-1's USB MIDI port replaces the loaded voices with its first 16; a single-voice dump adds one after the last. The header says `DX7 VOICES 16 LOADED`, `1 ADDED` or `BAD CHECKSUM` (a damaged file). Levels aren't matched (adjust the pluck's Level). A piece stores the pluck's sound by slot number. Free banks: Bobby Blues' DX7 archive, Soundarchive, Yamaha Black Boxes (licences are their authors').

**The emulator** runs the firmware in the browser. Drag or scroll a knob, click buttons and keys; Shift-click (or right-click) holds a button until the next click. **New start** restarts it (stored pieces stay, in that browser); **Forget stored** clears them. Keys: Space PLAY, Enter REC, Esc HOME, V / L / P ENV / LFO / ARP, M SEQ (hold it for the sequencer), O / C / N GLO / EDIT / SEL, backtick FX, B SAVE, Z / X OCT- / OCT+, Up / Down SELECT, Q-I and A-K the white keys 1-8 and 9-16, 1-0 and - the black keys 1-11.

**Remote:** with the FM-1 on USB and the emulator's **Connect MIDI** pressed, stop the FM-1, hold **SEL** and press **GLO**. It says REMOTE, goes quiet, and its whole panel plays the emulator, lights and all. SEL + GLO again ends it. **Stream view** (or `?stream`) is a clean 16:9 stage for OBS. **MIDI learn** maps any other controller to the emulator (kept in that browser). Remote uses USB MIDI channel 16: keys notes 48-74, buttons CC 102-115, knobs CC 85-91, MASTER CC 7.

**Live status and the logger.** The installer's **Live status** panel reads the FM-1's numbers four times a second (CPU, second processor, dropouts, late and slowest audio, what's cut back, genre and piece); **Save CSV** saves them for a bug report. Or `python tools/fm1log.py` (after `pip install mido python-rtmidi`) writes `telemetry/fm1_<time>.csv`, and `tools/fm1sum.py` sums it up by genre and piece.

### Troubleshooting

- **No sound:** MASTER; PLAY lit; a Level at 0 (KNOB1 on its page; the Mixer shows all); `BY HAND` (SEL + ENV); a mute on the mix map; an effect latched (`FX` in the header; OCT- in the menu clears all). Jam silences the voice it plays; REMOTE silences the FM-1 on purpose.
- **A short dip or tick when you store or change a setting:** normal (saving to memory). **A stored piece came back as before:** the power went off during the save. **My changes vanished:** only a stored piece (Global, SAVE) is kept.
- **The installer doesn't find the FM-1:** Chrome or Edge, a data cable, no DAW using the port, MIDI with SysEx allowed. **It offers an old version:** hard-refresh. **An install was cut off:** run it again; the FM-1 stays in update mode. **`PERF-1 SAFE MODE`:** two crashes while starting; reinstall, or PLAY to try again. **`UPDATE MODE IN 3..`:** you're holding OCT- and OCT+; let go.
- **Buttons or knobs don't match their labels:** hold OCT- and OCT+ while switching on; the calibration asks for each control in turn (it gives up after 30 s untouched).
- **Loaded DX7 voices don't show:** they come after the 30 built-in DX sounds; check for `LOADED`, the pluck's Level, the checksum.
- **Crackles or dropouts:** watch `XR` and `S1`/`S2`; run Live status a minute and send the CSV with the piece, its sounds, the tempo.
- **The emulator doesn't see the FM-1:** no DAW on the port; Chrome, Edge or Firefox (not Safari); SEL + GLO for remote.

### Credits and licence

PERF-1 is by **deamonSLINKY**: the firmware, the slinky mascot and the pages around them. It is a fork of **HYP-1** (RedtailWorks), itself a fork of **AMB-1** by Charles Vestal. The platform layer (USB, update loader, storage, installer, the panel calibration) is **Felucca** by Leo Kuroshita (Hügelton Instruments), by way of X0X and FoMni. The six-operator FM engine, its factory voices and the second-core work are from **Melodee** by Kerem Kilic (Ellic Studio); its synthesis is after **Dexed** by Pascal Gauthier, on **MSFA** by Raph Levien (Google). The bass line modes, pad comping, ghost notes, deja vu and the motif follow ideas in **Tuna** by delirioux, written afresh for this engine. The plate reverb is Jon Dattorro's; the emulator idea is Groove OS's; the hardware knowledge is FM-1-RE's. Fonts: Chakra Petch and Barlow Semi Condensed (SIL OFL 1.1). Source and issues: https://github.com/deamonSLINKY/PERF-1 (a bug report is most useful with the version, the genre and piece, and a status log). **Licence: GPL-3.0-only, no warranty.** M-VAVE and FM-1 are trademarks of their owners; PERF-1 is not affiliated with M-VAVE.

</div>
