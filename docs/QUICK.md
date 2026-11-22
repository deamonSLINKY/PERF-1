<!-- footer: PERF-1 quick manual · 1.0, November 2026 · full manual: deamonslinky.github.io/PERF-1/manual/ · GPL-3.0-only -->

# PERF-1 quick manual

<p class="lead">Eight pages of what to press. The full manual, with screenshots and more detail, is at <b>https://deamonslinky.github.io/PERF-1/manual/</b>. If the two disagree, trust the full manual. The 27 keys run from <code>F3</code> to <code>G5</code>: sixteen white, eleven black.</p>

<div class="cols">

**What it is.** PERF-1 turns the M-VAVE FM-1 into a groovebox that makes up music as it plays. Five tracks start when you switch it on: a slow pad, a bass, a 303-style acid line, a plucked counter-melody and drums you can program, all written on the spot from the key, scale and chord, in twenty-two genres of ten pieces. You steer: break it down, build it, drop it, mute and solo, throw effects, edit the drums, play along, store what you like, and record it over the same USB cable (the FM-1 is a USB audio and MIDI device). By deamonSLINKY (credits at the end). GPL-3.0-only, no warranty. It's still in development and can crash.

## Install in five steps

1. **Try it first.** The emulator at https://deamonslinky.github.io/PERF-1/emu/ runs the same firmware in your browser, with sound.
2. **Keep copies.** PERF-1 can't read other firmwares' projects, and what's stored may be lost when you switch either way: keep your `.syx` files. On PERF-1, the installer's **Back up and restore** card saves everything stored to a `.p1bak` file.
3. **Open the web installer**, https://deamonslinky.github.io/PERF-1/install/, in Chrome or Edge. Plug the FM-1 straight into the computer with a USB *data* cable, and close anything using its MIDI port (a DAW). Allow MIDI with SysEx (the messages the installer uses) when asked.
4. **Press Install.** The line under the button names the version it will write; if it's older than you expect, hard-refresh (Ctrl+F5 or Cmd+Shift+R). Don't unplug or close the page while it writes. The FM-1 restarts into PERF-1 and the page checks the version.
5. **To go back** to M-VAVE's FM-1 V15, use "Back to the stock firmware" at the bottom of the same page: download `FM-1.fwsc` from the link there, choose it (the page checks its SHA-256) and press **Install the stock firmware**. It works from PERF-1, safe mode and update mode; M-VAVE's M-UPGRADE works too.

**Update mode.** Hold **OCT-** and **OCT+** together for 5 s while PERF-1 runs. From 2 s the screen counts `UPDATE MODE IN 3..`; let go to cancel. If an install was cut off, the FM-1 waits for the installer (the key lights flash three times, the screen stays dark): press Install again. Cut off while the installer checks the package, nothing is written and the FM-1 waits dark: switch it off and on. **Safe mode.** Two crashes in a row while starting give a black screen, `PERF-1 SAFE MODE`, with USB on: reinstall, or press **PLAY** to try again. Installing is at your own risk; nothing protects against a damaged boot loader.

</div>

## The panel

| Control | Tap / turn | Hold | With SEL held |
|---|---|---|---|
| **MASTER** | Volume. | | |
| **SELECT** | The pages, in order (37, stops at the ends). FX menu: the bank. Settings: the next page. Visualizers: the next one. | With **HOME** held: the tempo. | |
| **PRESETS** | The piece, 1-10 (wraps). | With **GLO** held: the genre. | The piece, on the next bar. |
| **ALGORITHM** | The mood. Chords and Global: the mode of the scale. | | The voice's sound (drum pages: the kit). |
| **KNOB1-4** | The page's values a, b, c, d. FX menu, Perform, visualizers: Filter, Crush, Throw, Wash. | | Sequencer: Euclid, rotate, chance, length. |
| **OCT-** / **OCT+** | An octave down / up: on a voice's pages that voice, elsewhere the song. Sequencer: the drum. FX menu: OCT- turns all off. | Both 5 s: update mode. | Sequencer: clear the lane / the piece's own lane back. |
| **FX** | The FX menu on and off. Held: the last effect's settings. | | Steps through the effect settings, then back. |
| **SEL** | Shift: hold it with another control. | Alone 0.4 s: what shift does here. | |
| **ENV** | Pad; again Pad env, Pad LFO, Chords, Progression. | Any page button 0.9 s: its help card. | On a voice's page: play it by hand; again, back. |
| **LFO** | Bass; again Bass env, Bass LFO, Bass line, Bass steps, Bass answer. | | |
| **ARP** | Acid; again Acid env, Acid line, Acid answer, Acid steps, Pluck, Pluck 2, Deja vu, Pluck steps. | | |
| **EDIT** | The next chord, on the next bar. | 0.4 s: the Progression page. | |
| **GLO** | Global (Root, Theme, Tune, Genre). SAVE here stores the piece. | 0.4 s: the mix map (the load shows after a dropout). | Remote: the FM-1 plays the emulator. |
| **HOME** | Tempo page; again Perform; again Tempo. Twice quickly: visualizers. Leaves the FX menu. | 0.7 s: the settings. | |
| **SAVE** | Breakdown on the next bar; again a build, then the drop. On Global: store. | 0.5 s: the drop. | On Global, 1.5 s: back to factory. |
| **SEQ** | Drums; again Links, Kit, Drum drift, Groove, Steps, Feel, Phrase. In the sequencer: back to Drums. | 0.4 s: the sequencer. | The links on and off. |
| **PLAY** | Play and stop: each start a fresh track and its code (`PLAYING DZBT-4DTT9`). | | |
| **REC** | Freeze: the chord holds, the reverb rings on, until pressed again. | | Capture: the bar played on Jam into the voice's line. |
| White keys | Play the page's voice (Global: add or remove a scale note, the root stays). Jam: notes. Sequencer: the 16 steps. FX: effects. Perform, GLO held: the mix. | A scene key 0.6 s: store it. | White 1-6 (`F3`..`D4`): pattern 1-6, on the bar. |
| Black keys | The same. Sequencer: which pattern the lane plays (black 1-6; black 7: follow). | | Sequencer: copy this pattern into 1-6. |

---

## The pages on the SELECT dial

**SELECT** steps through 37 pages in this order. The buttons (**ENV**, **LFO**, **ARP**, **SEQ**, **GLO**, **HOME**) jump to their group and step through it; from elsewhere, a button returns to the page you last used in its group. Columns a-d are **KNOB1**-**KNOB4**; on a voice's main page KNOB1 is its level. Pages that shape a sound draw it. Off the dial: the **sequencer** (hold SEQ), **Perform** (HOME on Tempo), the **visualizers** (HOME twice quickly).

| Page | Button | a | b | c | d |
|---|---|---|---|---|---|
| Tempo | HOME | Tempo (70-190) | Evolve | Motion | Drift (how long a chord lasts; HYPNO 20 s - 5 min) |
| Pad | ENV | Level | Bright | Sound (24; the saw sounds sweep a filter) | Detune |
| Pad env | ENV x2 | Attack (0.05-10 s) | Hold (0-8 s) | Release (0.1-12 s) | Rest (0-6 s) |
| Pad LFO | ENV x3 | Rate (1/16T - 32 bars in speed order, with 1/4T, 1/8., 1/4., 1/2.; locked to the bar) | Depth | Shape (6: sine .. drift) | Target (filter, level, pan, pitch) |
| Chords | ENV x4 | Chord (8 kinds, below) | Voicing (close, open) | Comp (auto, hold, 7 rhythms) | Loop (off; 2, 4, 8 chords) |
| Progression | ENV x5 | Chord (I-VII, Hold) | Type (Piece's, Triad .. 11th) | Bars (1-8) | Play (Walk, Loop) |
| Bass | LFO | Level | Sound (24) | Cutoff | Reso |
| Bass env | LFO x2 | Trigger (chord; 1-8 bars; off) | Attack (0.01-4 s) | Decay (0.05-16 s) | Amount |
| Bass LFO | LFO x3 | Rate | Depth | Shape (as the pad's) | Target (cutoff, pitch, level, drive) |
| Bass line | LFO x4 | Mode (Hold, 6 modes, 13 figures, Steps) | Length (how long a note holds) | Move (octaves, fifths ..) | Glide |
| Bass steps | LFO x5 | Step (rest, note, accent, glide, hold) | Note (root .. octave in the scale) | Octave | Mode |
| Bass answer | LFO x6 | Answer (octave, fifth, run, busy, sparse, slide) | Every (2-8 bars, drift, with or after the acid) | Acid ans | Mode |
| Acid | ARP | Level | Cutoff | Reso | Env mod |
| Acid env | ARP x2 | Decay | Accent | Slide | Sound (31) |
| Acid line | ARP x3 | Density | Length (1-16) | Range (1-3 oct) | Mutate (SEL + knob: what it changes) |
| Acid answer | ARP x4 | Answer (off, 8 kinds, page 6) | Every (2-8 bars, drift; 2-8 loops, drift) | Direction (down 2 .. up 2) | Level (same .. +6 dB) |
| Acid steps | ARP x5 | Step (rest, note, accent, slide) | Note (0-15 scale steps) | Octave (0-3) | Frozen (free, frozen) |
| Pluck | ARP x6 | Level | Sound (12 + 60 DX + yours) | Density | Octave (-2 to +2) |
| Pluck 2 | **ARP** x7 | Decay | Style (up, down, up-down ..) | Rate (1/4, 3/16, 1/8, 1/16) | Range (1-3 oct) |
| Deja vu | ARP x8 | Pluck vu (0 new .. 100 locked) | Loop (4, 8, 16, 32 notes) | Leaps (an octave away) | Motif (develop the first bar) |
| Pluck steps | ARP x9 | Step (rest, soft, note, accent) | Note (1-12, the chord's notes low to high) | Octave (-2 to +2) | Frozen (free, frozen) |
| Drums | SEQ | Level | Drive | Swing (50-75 %) | Pattern (1-6, on the bar) |
| Links | SEQ x2 | K>Bass | S>Pad | H>Acid | - |
| Kit | SEQ x3 | Kick | Snare (and clap) | Hats (closed and open) | Perc |
| Drum drift | SEQ x4 | Change (off, next, random) | Every (the chord, 4-64 bars) | Pool (patterns 1-2 .. 1-6) | Mutate (off, shift, morph) |
| Groove | SEQ x5 | Lane (kick .. perc) | Density (0-100; 50 as written) | Source (follow, pattern 1-6) | All dens |
| Steps | SEQ x6 | Lane | Ratchet (x2 .. x6, flam, swell) | Push (0-7 twelfths late) | Level (normal, ghost, soft, accent) |
| Feel | SEQ x7 | Ghosts (0-100) | Accents (0-100; 50 as marked) | - | Humanize (0-100) |
| Phrase | SEQ x8 | Chain (off, 9 chains) | Slice (off, 8 orders, auto) | Time (normal, half, double) | Cond (when the lane plays) |
| Space | dial | Echo | Echo time (1/8 .. 1 bar) | Reverb | Size |
| Reverb | dial | Type (plate, shimmer, gated) | Damping | Predelay (6-30 ms) | Amount |
| Comp/Limit | dial | Ratio (off - 10:1) | Thresh (-30 - 0 dB) | Make-up (0 - +12 dB) | Ceiling (-12 - 0 dB) |
| Mixer | dial | Track (pad .. drums) | Level | Pan (L100 - C - R100) | Send (0-100) |
| Global | GLO | Root | Theme (7) | Tune (+-50 cents) | Genre (the bank) |
| Set list | GLO x2 | Slot (1-16, a white key) | Piece (every genre's, or Empty) | Bars (Auto, 4-128) | Play (off, loop, once) |
| Code | GLO x3 | Place | Letter | Roll (a new code; PLAY starts it) | - |
| Jam | dial | Voice (pluck, pad, acid, bass) | Keys (scale, chords, harp) | Octave (-2 to +2) | Sound (that voice's) |

<div class="cols">

**What changes by itself.** Every few bars the chord changes. Each genre has its own pace and its own habits for where the chord goes next; **Drift** makes the pace up to four times faster or slower (in HYPNO, 20 s to 5 min a chord; SEL + Drift: bars or seconds), and **EDIT** moves on now. The pad's notes swell and fade, taking the new chord as they swell. The bass follows the root. The acid's riff moves with the chord, and **Mutate** rewrites a few steps each time round. The pluck plays the chord's notes one at a time (an arpeggio). **Motion** slowly moves the song between sparse and full; **Evolve** makes the acid and chords change more.

**Tempo** 70-190 BPM. Hold **HOME** and turn **SELECT** on any page, or tap it in: hold **GLO** and tap `E4` four times or so. MIDI clock from another device (its tempo, sent over MIDI) takes over, and the screen shows `EXT`.

**Octaves.** **OCT-** / **OCT+** move the voice whose page you're on (the Pad, Bass or Acid pages and their env and LFO pages; the Pluck pages move the pluck). On any other page they move the whole song. You'll see `OCT+2` or `SONG-1` above the knobs. It's stored with the piece.

**Chords** (ENV x4) are built from the scale's notes. **Auto**: 7ths on a 7-note scale, triads on a pentatonic. **Triads**: Am, C, G. **7ths**: Am7, Dm7, CM7. **9ths**: root, 3rd, 7th, 9th. **Sus**: root, 4th, 5th, 7th. **Dub**: one m9 shape on every chord, the dub techno stab. **Fourths**: stacked fourths (A4ths), a hollow m11. **11ths**: root, 7th, 9th, 11th.

**Voicing**: Open drops the second note from the top an octave. **Comp**: the rhythm of the pad's stabs (short chord hits) for the stab sounds (Stabs, Dub, Pings, Hits, Rave, Wurli, Mallet, Brass, Pluck): Auto (the genre's), Hold (no stabs), or forced on any pad sound: Off-beat, On-beat, 3-3-2, Charleston, Syncopated, Sparse, Eighths. The pad's chorus is on the **CHORUS** settings page (Pad). **Loop** repeats the last 2, 4 or 8 chords (with a ~, one is swapped now and then); many genres bring one. **Progression** (hold EDIT): your own chords, eight slots (white keys) of the scale's chords, each its own type and bars (Hold: the chord before rings on). Turning one starts the loop next bar; Walk hands back to the genre.

**Sound words.** Cutoff is where the filter starts cutting the highs (lower is darker); Reso adds a squelchy peak there. An env (envelope) shapes each note: attack fades in, decay and release fade out. An LFO is a slow wobble: Rate is its speed, Depth how far, Target what it moves.

**Space** is the echo and reverb send. Echo is how much echo, Echo time the gap between repeats, in time with the tempo (1/8, 3/16, 1/4, dotted 1/4, 1/2, 1 bar), Reverb how much goes into the reverb, Size how long it rings. On **Reverb**, choose Plate (Amount does nothing), Shimmer (the tail rings an octave up; Amount sets how long) or Gated (cut off after 80-400 ms, set by Amount).

**Comp/Limit** works on the whole mix, before **MASTER**: a stereo compressor evens out loud and quiet, then a limiter stops anything passing the Ceiling. The page draws the curve and meters COMP, LIMIT and OUT. Default 1.5:1 at -12 dB, ceiling 0 dB; each genre brings its own (HYPNO: 6:1 at -18 dB, ceiling -6 dB). **Mixer** sets one track at a time, with a fader and meter for each: Level (as on its own page), Pan (left-right balance), Send (to the echo and reverb: 50 as the piece has it, 0 none, 100 double). All stored with the piece.

</div>

---

## The FX menu

<div class="cols">

**FX** opens and closes the menu; **HOME** or a page button also leaves it. While it's open every key is an effect, and **SELECT** steps through four banks: **FX**, **BEAT FX**, **FLEX BEAT**, **DJ SNIPS**. **Tap** a key (let go within 0.25 s) to latch the effect on; **hold** it to play it only while the key is down. Tap a latched key again to turn it off. Several can be latched at once (a bar beside each).

**Latched effects keep going after you close the menu.** The **FX** button blinks, `FX` shows by the tempo, and the keys play a voice again. **OCT-** in the menu turns everything off: effects, FX knobs, mutes and solos (GLO + `B3` undoes it). The FX knobs Filter, Crush, Throw and Wash are the knobs here, on Perform and on the visualizers (page 4).

**Settings pages.** Each effect with settings has a page that draws it: **SEL + a key** in the menu (that key's effect), **hold FX**, from any page (the last one used), **SEL + FX** (all 25 in turn). **SELECT** moves between them and **FX** closes. The effect plays while its page is up, so you hear what you change (a few need a key held). All kept across power-off. The knobs on each page: Gate: Pattern (3-3-3, tres, offbeat, Euclid 5/7/9, waltz, chop, sparse, dense, dotted, 5/10, or your own from its white keys), Length, Targets, Depth (Gate 16 and 8 share it). Phaser and Flanger: Rate, Depth, Feedback, Mix. Vinyl: Level, Crackle, Hiss, Tone. Tape: Wow, Flutter, Drive, Dull. Roll: From, To, Each, Swell. Pump: Depth, Release, Every (1/2 .. 1/16 or KICK), Targets. Riser: Time, Filter, Wash, Echo. Scatter: Chaos, Reverse, Slice, Slices. Brake: Time, Spin back, Return, Dull. Half time: Speed, Slide. Sweep: Every, Depth, Reso, Type. Scratch: Every, Reach, Style. Freeze: Size, Spread, Grains. Rv swell: Length, Mix. Ring mod: Tone, Mix. Talk: Every, Depth, Reso, Mix. Wah: Sens, Range, Reso, Mix. Flex: Quant (off, beat, bar: when it starts), Mode (one shot, or loop until you let go or tap it off), Mix. FILTER, DELAY, PHASE: each has Targets; FILTER's Type runs on into MORPH, low-pass through band-pass to high-pass. Snips: Pitch (normal, an octave up or down), Play (while held, or one shot of about a second), Echo (dry, some, lots), Level. Snip land: Now, Beat, 2 beats, Bar (the riser, rewind and roll end on that line; pressed again, they start over). Auto DJ, Settings, USB & MIDI: pages 4 and 7.

### Bank 2: BEAT FX

| Keys | Effect |
|---|---|
| `F3` .. `C4` | Beat repeats (a slice looped) 1/2, 1/4, 1/6, 1/8, 1/16, 1/32, 1/8T, 1/16T; the shortest held wins |
| `D4` `D#4` `E4` | Scatter (slices shuffled), Brake (a vinyl brake), Spin back; Brake and Spin back held stay stopped and silent until let go, then come back in time |
| `F4` `F#4` | Half time (held, it goes back to the live music each bar and stays slowed), Scratch (automatic) |
| `G4` `A4` `B4` `C5` | Mutes, held: kick, hats, snare, perc |
| `G#4` `A#4` | Reverse, Tape stop |
| `C#5` .. `F5` | Hat roll (32nds), Hat trip (triplets), Snare roll (louder through the bar), Flam (a grace note before each snare), Double kick |

</div>

### Bank 1: FX

| Key | Effect | Key | Effect |
|---|---|---|---|
| `F3` | Sweep: a filter swept in time with the tempo | `F#4` | Tape: wow, flutter, drive |
| `F#3` | Gate 16: the synths chopped in 1/16s (trance gate) | `G4` | FILTER (tracks) |
| `G3` | Ring mod: a metallic tone | `G#4` | Roll: repeats that speed up |
| `G#3` | Gate 8: the same in 1/8s | `A4` | DELAY (tracks) |
| `A3` | Freeze: holds the sound still | `A#4` | Pump: the mix ducks in time (sidechain) |
| `A#3` | Gate pattern: the gate with its own pattern | `B4` | PHASE (tracks) |
| `B3` | Reverse swell | `C5` | M:DRUMS |
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

### Bank 4: DJ SNIPS (27 sounds; up to four at once, fewer under load; they play while held and never latch)

| Key | Snip | Key | Snip | Key | Snip |
|---|---|---|---|---|---|
| `F3` | AIR HORN (held) | `D4` | ZAP | `B4` | DUB LOW |
| `F#3` | AIR BLAST (short) | `D#4` | ZAP HI | `C5` | DROP |
| `G3` | CHORD STAB (the chord playing) | `E4` | BEEP | `C#5` | DROP LONG |
| `G#3` | OH! (a voice) | `F4` | BEEP LO | `D5` | RISER |
| `A3` | SIREN | `F#4` | BEEP HI | `D#5` | REWIND |
| `A#3` | SIREN 2 | `G4` | 2 BEEP | `E5` | CRASH |
| `B3` | KICK | `G#4` | WHOOP | `F5` | OH! LOW (an octave down) |
| `C4` | LASER | `A4` | WHOOP 2 | `F#5` | BELL |
| `C#4` | SNARE | `A#4` | DUB SIREN | `G5` | SNR ROLL (speeds up) |

---

## Performing

<div class="cols">

- **Breakdown: tap SAVE.** In the bar before, the percussion, open hat and half the hats drop out. On the next bar the drums stop (house, techno and dub keep the kick; DnB, industrial, forest and hip-hop the hats, hip-hop the bass too; breaks and funk the drums), the mix darkens, the bass closes and the pad rises, for as long as you like.
- **Build: tap SAVE again.** A snare roll climbs as the bass opens, for four bars (two in lofi, trip-hop, hip-hop and ambient; eight in house, dubstep, DnB, psy and trance). In the last bar the open hat plays every eighth and, in the four-on-the-floor genres, the kick comes back. Then the drop. The header counts `DROP 3`.
- **Drop: hold SAVE** (0.5 s). Everything is back on the next bar with a ringing crash, mostly on the home chord. SAVE is lit in a breakdown or build and blinks while it waits for the bar.
- **Freeze: REC.** The chord holds and the reverb rings on until you press REC again. **Next chord: EDIT**, on the next bar.
- **The scale: the keys on Global** (elsewhere they play a voice). Each adds or removes its note; the root stays. **ALGORITHM** there (and on Chords) picks a mode (Aeolian is plain minor, Dorian a brighter minor, Phrygian a darker one, Mixolydian major with a flat 7th; also Locrian, harmonic and melodic minor, Phrygian dominant, Hungarian minor, double harmonic). Root is on Global.
- **Patterns:** KNOB4 on the Drums page, or SEL + white keys 1-6; the change waits for the bar. Hold a pattern key past 0.25 s and it plays only while held, then the one before comes back.
- **Pieces:** PRESETS; with SEL held, on the next bar. **Genres:** GLO held + PRESETS, or Global's Genre knob.
- **Songs aren't saved:** record the audio or MIDI out into your DAW.

### The Perform screen

**HOME** on the Tempo page (elsewhere, twice). It shows the section (GROOVE, BREAK, BUILD) and the auto-DJ's place in it, the bar's 16 steps, the eight tracks (a line through a muted one, a soloed one filled), the four scenes, the effects on, and the mood. Keys: the track mix below. Knobs: the FX knobs. **ALGORITHM**: the mood. **SEL** tapped: the swell (mood, throw and wash up over a bar, back over four). **SEL +** white 1-6: the drum pattern; **A#4**: the drums' next pattern; **F#4**: a new acid line; **F#5**: a new pluck line (not a frozen one).

**The FX knobs.** **Filter**: left of centre cuts the highs (16 kHz down to about 250 Hz), right cuts the lows (20 Hz up to about 1.6 kHz), `OFF` in the middle. **Crush**: lo-fi grit (fewer bits, lower sample rate). **Throw**: more of the mix into the echo, for longer. **Wash**: the mix into the reverb. They go home at power-off, but a scene stores them. `KNOB` in the header: one is away from home. **Gesture:** in the FX menu, **SEL + REC** records your FX knob moves from the next bar (`REC`); again, and they loop (1-4 bars, `LOOP`). A knob turned by hand is yours again; SEL + REC lets go.

### The mix map (GLO held)

Hold **GLO** (0.4 s), or use the keys on Perform or a visualizer with FX closed. A mute stays on until pressed again; the black key above a track solos it, but the hats', snare's and perc's freeze a part: the bass (Pedal), the acid line, the pluck line.

| Key | Does | Key | Does |
|---|---|---|---|
| `F3` / `F#3` | Mute / solo the kick | `C5` `C#5` `D5` `D#5` | Scenes 1-4 |
| `G3` / `G#3` | Hats / freeze the bass | `E5` | Reset all |
| `A3` / `A#3` | Snare / freeze the acid | `F5` | Auto-DJ on / off |
| `B3` | Undo (again: redo) | `F#5` | Solo the pluck |
| `C4` / `C#4` | Perc / freeze the pluck | `G5` | DJ new pieces on / off |
| `D4` / `D#4` | Bass | `E4` | Tap tempo |
| `F4` / `F#4` | Acid | `A4` / `A#4` | Mute / solo all four drums |
| `G4` / `G#4` | Pad | `B4` | Mute the pluck |

- **Scenes** store what's latched in FX, the mutes and solos, the FX knobs and the groove (each lane's density and source, and the pattern playing). **Tap** a scene key to recall it (on the next bar if the Auto DJ page's Scenes is `NEXT BAR`, the default; the FX knobs glide over about a second). **Hold** it 0.6 s to store what's playing now. Kept until power-off.
- **Undo** (`B3`) goes back to before the last big change (a scene, a reset, all drums muted, a clear); again to redo. **Reset all** (`E5`): nothing latched, no mutes or solos, the FX knobs home; you can undo it. **Tap tempo** (`E4`): four taps or so.

### The auto-DJ

GLO + `F5` runs breakdowns and drops for you (`DJ` in the header). On the bar it plays (1) a **groove** of 8, 16, 32 or 64 bars (16 by default); (2) a **drum fill** as its last bar, if Fill is on; (3) a **breakdown**: short (4 bars), half the groove (at least 4; the default) or full; (4) a **build** of the genre's length, the riser rising and two sixteenths of silence at the end, then the **drop**; (5) the groove again. The header counts `BREAK IN 4`, then `BREAK 2`. Groove on **Sync** (its fifth setting) lasts until the chord changes (Drift sets when): the bar before is a fill, the breakdown starts with the new chord. **DJ new pieces** (GLO + `G5`) brings in the bank's next piece on the fill's bar, so the breakdown and drop are its own. SAVE by hand just moves the DJ on.

The **SET** settings page (after Auto FX): **Rounds** a piece plays before the next (1, 2, 4, 8; `R2/4` in the header). **Ramp**: the tempo climbs +2 to +12 BPM over a piece's rounds. **Intro**: after PLAY the tracks come in one by one over 1, 2, 4 or 8 minutes, pad first, acid last; `INTRO 1:42` counts down and the DJ waits. **Break**: a piece you start or turn to begins in its breakdown (SAVE brings it in). The **Auto DJ** settings page: Scenes (at once, next bar), Groove (8, 16, 32, 64, sync), Break (short, half, full), Fill (off, on).

### Mood

On every page but Chords and Global **ALGORITHM** sets the energy: 0 chill, 50 (default) the piece as it is, 100 peak. It nudges the acid's density and cutoff, the hats' and perc's levels, the pad's brightness, the pluck's density, level and decay, the bass's cutoff and drive, reverb and echo (less at peak), Evolve and the drums' level. It changes what plays, not what's stored, and is forgotten at power-off.

### Drum drift (SEQ x4)

Want the drums to change by themselves? **Change**: off, the next pattern, or a random one from 1 up to the **Pool** limit, **Every** 4-64 bars or with each chord change. The bar before a change ends in a fill. **Mutate** rewrites a little at the same moments: Shift moves a hit or two a step, Morph also adds or removes one. The kick and clap stay as written, and a snare on a beat never moves. Mutate works with Change off too. Off by default; these four settings stay when you change piece or genre.

### What the screen says

**Top left:** the piece's tag (`01` HYPNO, `L03` lofi ..; a dot after it means unstored changes), the root and the mode. **Top right:** the tempo (`EXT` on an outside clock; a play triangle while stopped; the imp hops in on a drop, break or build), then the marks: **BREAK**, **BUILD** (with `>`: waiting for the bar; `DROP 3`: bars to the drop), **KNOB** (an FX knob off home), **KEY** (the drop's key change is on), **S1>** (a scene waiting), **DJ**, **FX** (effects latched, menu closed), a small dot (a save to memory waiting).

**Under it:** the chord (an arrow and the next root when one is coming; `HELD` while frozen) and the section, GROOVE, BREAK or BUILD, over a bar filling towards the next chord. **The strips:** pad, bass, acid, pluck and drums, each with the bar's coming notes falling to a line (the pad: its chord), a meter and its sound; a voice's own pages open its strip wide. **Above the knobs:** the page and the voice's sound, `BY HAND`, `+LINK` or `LINKS OFF`, the octave when it isn't home, then a tab for each group of pages. On a change of page the header says where you are: `DRUMS  PHRASE 8/8`.

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

**Ghosts** (0-100): the genre's ghost notes, soft snares (and soft kicks in lofi, breaks, funk, trip-hop, industrial, hip-hop) on empty steps; lofi, house, DnB, breaks, garage, funk, trip-hop, industrial, forest and hip-hop have them. **Accents** (50 = as marked): how much the Steps page's accents and ghosts stand out; at 0 every hit is the same level. (The pad's stab rhythm is Comp, on Chords.) **Humanize**: each hit a touch softer and later, like a person playing; it loosens the acid, bass line and stabs too.

### Phrase (SEQ x8)

**Chain** plays a different pattern each bar, round and round: 12, 1112, 1123, 1134 (the Amen break's), 1213, 1234, 1232, 1115, 123456. While on, it picks the pattern at every bar line, over the Pattern knob, held pattern keys and drift's changes (drift's fills still play). **Slice** plays the bar's steps in another order: Halves (swapped), Stutter (the first beat of each half repeated), Reverse, Delay (the back half an eighth late), Beats (the middle two swapped), Mirror (the second half backwards), Double (the first half twice), Swap (the beats within each half swapped); Auto picks one for the last bar of every four. **Time**: Half spreads the pattern over two bars (DnB into drumstep), Double plays it twice a bar. **Cond**, when the chosen lane plays: Always, 1:2 (odd bars), 2:2, 1:4, 4:4 (the last of four), 1:8, Fill (only in fill bars), No fill. All stored with the piece.

### Links (SEQ x2)

Drum hits push the other voices, 0 to 100 each; **SEL + SEQ** turns them all on and off (`+LINK`). **K>Bass**: each kick opens the bass's filter, closing again over the beat. **S>Pad**: each snare or clap flashes the pad's filter open. **H>Acid**: a hat on an acid note accents it. A muted drum still drives its link.

**The Drums page:** Level, Drive, Swing (50-75 %: how late the off-beats land; the acid, bass line, stabs and pluck follow it) and Pattern. Store the piece (Global, SAVE) to keep edits.

</div>

### The kits (SEQ x3)

Each knob picks one drum's sound from the same twenty-three kits (the first fourteen below; then Boom, Ride, Snap, 606, Gabber, Linn, Shaker, and the sampled Machine and Boombap), then three slots for your own sample kits, loaded from the installer page (Your own sample kits): Kick, Snare (and clap), Hats (closed and open), Perc. **SEL + ALGORITHM** on any drums page turns all four to the next kit. Mix them freely.

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

**Acid answer** (ARP x4): call and response. **Answer**, how the line changes: Octave, Fifth, Third (two scale steps, so it stays in key), Mirror (the shape upside down round the line's middle), Reverse, Rotate (moved a beat), Sparse (even steps only), Accents (accented notes only). **Every**: 2 to 8 bars, or Drift (the bar before the drums change); further on, 2 to 8 loops and Drift lp (counting times round the line, the answer the whole line; for lines that aren't 16 steps). **Direction**: down 2, down, same, up, up 2. **Level**: same, +2, +4, +6 dB (the unit's setting). Off by default; the rest stored with the piece.

**Acid steps** (ARP x5): the line as sixteen steps you can edit. A **white key** picks a step. **Step**: Rest, Note, Accent, Slide (glides into the next), Acc+Slide. **Note**: 0-15 scale steps above the chord. **Octave**: 0-3 up. **Frozen**: Free (it keeps changing; each piece grows a new one) or Frozen (held as it is, Mutate leaves it alone, **stored with the piece**). Any edit freezes it. Steps past Length are dim and silent. The answer and the Acid octave still work on a frozen line.

**Pluck steps** (ARP x9): the Deja vu loop as sixteen steps you can edit. Laid out like Acid steps. A **white key** picks a step. **Step**: Rest, Soft, Note, Accent. **Note**: 1-12, the chord's notes from low to high. **Octave**: the whole pluck's. **Frozen** holds the loop (setting Loop 16, Rate 1/16, Pluck vu 100) and **stores it with the piece**; any edit freezes it. Left free, a new piece forgets the loop.

**Capture (SEL + REC)** turns the last bar you played on Jam into the voice's own line. Jam remembers your last 32 keys: play the bar, then press (`CAPTURED`). On the acid: a frozen 16-step line on Acid steps, each sixteenth you pressed a key on that note (scale steps above the chord of the moment), a key on a beat an accent, a key held past the next step a slide; it plays when you leave Jam. On the pluck: into the Deja vu loop (Loop 16, Rate 1/16, Pluck vu 100) from the next bar. On the bass: into Bass steps (Mode Steps). Each note keeps the chord it was played over. The pad can't be captured. **On an Acid page, SEL + REC keeps the answer** as the line (frozen).

### The pluck

The counter-melody: two plucked voices taking turns, an arpeggio of the chord on the bar's grid, swung off-sixteenths. Level 50 and Density 45 by default (Level 0 for none); thinner in a breakdown. **Pluck** (ARP x6): Level, Sound, Density, Octave. **Pluck 2**: Decay (for DX sounds, how long the key is held, 0.12-1.7 s), Style (up, down, up-down, random, skip = every second note, thirds), Rate (1/4, 3/16, 1/8, 1/16), Range (1-3 octaves).

**Deja vu** (ARP x8) makes the pluck remember its last 4, 8, 16 or 32 notes (**Loop**). **Pluck vu** is the chance a note coming round is the same as last time (0 all new, 100 locked); a locked phrase still follows the chord. **Leaps**: how often a new note jumps an octave. **Motif**: the first bar of each eight-bar phrase is the idea, and the bars after develop it (moved up or down, turned upside down now and then, a new ending, back in the fifth bar). 60 by default; 0 makes up every bar fresh. Changing Loop, or a new piece, forgets the loop.

**Sounds:** 12 plain (Pluck, Marimba, E.Piano, Kalimba, Bell, Vibes, Gong, Clav, Digital, Brass, Bleep, Glass), then 30 built-in DX7 voices on a six-voice, six-operator FM synth like the Yamaha DX7 (all 32 algorithms, envelopes, scaling, velocity, LFO, pitch envelope, feedback): made by people and shown by their own names (DX MARIMBA and DX KALIMBA keep `DX`, apart from the plain ones): MF EPRHOD1, WINEGLASS, SMOOTHBASS, FULL-BRASS, EASY DXPAD, FLUTEMRMBA, DRAWBARS, GITARA AKU, ROLAND BRS, SOLID BASS, TEMPLEBELL, DX MARIMBA, CLAVINET, SOFTSTRNGS, SHIMMERPAD, SKY SYNC, HARFA LSR, DX KALIMBA, FLUTE BLOW, STEEL PANS, MOOGBASS, TUBUL-BELL, PEDALSTEEL, CHOR.STRAT, GUITARDAMP, AXE, SLAPS, MFSYNBRASS, SAXOPHONE, SOLO TRPT; then **thirty more** made by people, likewise by their own names: WURLITZER, TF-RHODES, REAL GRAND, ARCTICKEYS, HARPSICHORD, MAGICM1ORG (house organ), ACCORDION, Farfisa1, PROPHET, JUNO 106 7, OBOE, Saw3Op, SquarePWM, Techno2, TSQR DECAY, CS 80, ANALOG 1, PAN PIPE, ANGEL VOX, Fairlt.Vox, WD SQ BASS, KOTO, CIMBALOM, Femalevibe, BANJO, DOUG22PICK, SITAR VIII, NEUTRNPLUK, ETH. STAB, SHORTHORNS. The DX voices play in stereo (a light ensemble; basses in the middle). Then up to 16 of **your own** (page 8), shown by the voice's name. Heavy patches play three or four notes at most. SEL + ALGORITHM on a pluck page steps through the sounds there are.

### The bass

**Bass** (LFO): Level, Sound (24: Saws, Warm, Grit, Dirty, Reese, Pulse, FM, Organ, Rumble, Sub, Square, Hoover, Fifths, Metal, Sync, Bright, Wub, Bite, Solid, Growl, Gnarl, Wide, Power, Air), Cutoff, Reso. **Bass env**: Trigger (when it restarts: each chord, every 1, 2, 4 or 8 bars, or off), Attack, Decay, Amount. **Bass LFO**: Rate, Depth, Shape, Target (cutoff, pitch, level, drive). Dubstep's wub is this LFO on the cutoff, its speed changing each beat on the drop.

**Bass line** (LFO x4): notes that follow the chord. **Mode**: Hold (one long note gliding to each chord's root), Root (the genre's bass rhythm: the beats in hypnotic, a lazy three in lofi, a roll in DnB), Kick (on every kick, as an 808 does), Eighths, Off-beat (the off-beat eighths of house), Walk (on the beats: root, third or fifth, fifth, the scale note below the root), Riff (the genre's figure: disco bounces the octave, funk and amapiano syncopate, reggae leans on the fifth), then nine figures by name in any genre: Octaves, Two-step, One-drop, Log drum, Skippy, Funk, Big beat, Dub, Gallop; **Steps**: your own, from **Bass steps** (LFO x5); then four more figures: Rolling, Tresillo, Push, Bounce. **Length**: how long a note holds (a quarter step to five steps). **Move**: how often a note goes up an octave, to the fifth or to the note below the root instead. **Glide**: 0 none, 100 about 0.4 s. Each note opens the filter with the Bass env's attack and decay (Trigger doesn't apply). A breakdown plays no notes. New pieces start with a line in lofi, dub, DnB, trip-hop (Root), house (Off-beat), dubstep, trap (Kick), electro, synthwave (Eighths), disco, funk, reggae, amapiano, breaks, garage (Riff); HYPNO holds.

### The pad

**Pad** (ENV): Level, Bright, Sound (24: Rhodes, Voices, Strings, Glass, Organ, Warm, Stabs, Dark, Dub, Trance, Poly, Hollow, Pings, Hits, Rave, Sine, Ooh, Wurli, Mallet, Brass, Fat, Pluck, Chapel, Air), Detune. **Pad env**, how each chord swells and fades: Attack, Hold, Release, Rest (the gap before the next). For the stab sounds Release is the stab's length. **Pad LFO**: Rate (1/16T to 32 bars), Depth, Shape, Target (filter, level, pan, pitch). Chords and Comp are on page 2; the pad's chorus is on the CHORUS settings page.

### Playing a voice by hand

On any of a voice's pages (**ENV** pad, **LFO** bass, **ARP** acid) hold **SEL** and press **ENV**. It stops playing its part (`BY HAND`); while its pages are up, the keys play it over the song. Pad: up to four notes, with the Pad env's attack and release. Bass: the last key held, two octaves down. Acid: the last key held, an octave down, with its filter and envelope; a second key slides to it, as on a 303. The lit keys still show the scale. Without BY HAND the keys play the voice over its own part; on pages that aren't a voice's, the Jam page's voice; on Global, they edit the scale. **SEL + ENV** again hands it back. Kept with the piece; MIDI out sends what you play.

### Jam (the dial, after Global)

The 27 keys become an instrument (they only edit the scale on Global); Voice, Keys and Octave here also set how they play on pages that aren't a voice's. **Voice**: pluck, pad, acid or bass (that voice stops its own part while the page is up). **Keys**: Scale (every key a scale note, a black key the one at or below it: no wrong notes), Chords (every key a triad of the scale) or Harp (the scale's notes one per key, from C3). **Octave** -2 to +2. **Sound**: the voice's sound, the same setting as on its own page. A nearly silent pluck is raised to 40 so you hear it. Then **SEL + REC** captures the bar (above). Any voice's sound: the Sound knob on its page, or **SEL + ALGORITHM** on any of its pages.

</div>

---

## Genres, pieces and what is kept

Twenty-two **genres**, each a bank of ten **pieces**. **PRESETS** steps through the bank (01-10, wraps round); **GLO** held + **PRESETS**, or Global's Genre knob, changes bank. The piece number stays, and the genre's own effects come with it (lofi latches vinyl and tape, house the pump; switching first clears latched vinyl, tape, pump and phaser). Each piece has its own settings, six patterns and drum sounds, a set of sounds by its number and a made-up title (`VELVET RAIN`); each genre its own compressor, reverb, stab rhythm, ghost notes, chord habits and section lengths.

| Genre | Tag | BPM | What it is |
|---|---|---|---|
| HYPNO | 01 | 118-138 | The original hypnotic techno and acid set, from four on the floor to dub techno and warehouse. |
| LOFI | L01 | 70-94 | Swung boom-bap, Rhodes in 7ths and 9ths, a warm bass, the Dusty kit; vinyl and tape latched. |
| HOUSE | H01 | 118-126 | Four on the floor, claps on 2 and 4, off-beat open hats, stabs and organ; the pump latched. |
| DUBSTEP | D01 | 136-150 | Half time, the Heavy kit, a wub bass, a harsh lead, chords in fourths. |
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
| INDUS | I01 | 118-145 | Industrial: harsh tuned drums, broken kicks, phrygian. |
| PSY | Y01 | 92-146 | Psytrance, rolling bass; Y01, Y02, Y10 psychill. |
| FOREST | O01 | 148-165 | Dark forest: glitching perc, hat rolls. |
| HIPHOP | M01 | 86-98 | Boom-bap; M06, M08 G-funk; M05, M09 dark. |
| TRANCE | U01 | 132-140 | Classic trance: supersaw, long breakdowns. |
| AMBIENT | Z01 | 70-88 | Beatless or nearly: pads, shimmer. |


<div class="cols">

### Storing a piece

Go to **Global** (GLO) and press **SAVE**. The piece is stored as it is (every page, scale and mode, sounds and kit, patterns and drum sounds, the frozen acid line, the step settings) in the genre you're in, in place of its factory piece. The line above the knobs says `01 SAVE: STORE` while there are changes, then `01 STORED`. Unstored changes last while you stay in the genre and are lost when you change genre or switch off. **Back to factory:** on Global hold **SEL + SAVE** 1.5 s (KEEP HOLDING: FACTORY). **Start from nothing:** on Global, **SEL + OCT-** empties the piece (no drums, one chord, the bass, acid and pluck on steps, all rests); build it up and SAVE. **Every stored piece of every genre:** stopped, hold on to 6 s. A store is written at once; while playing, the music dips for a moment (`STORING`), unless Settings > Save is At stop: then it waits for the music to stop (`WRITTEN AT STOP`). Don't switch off during it.

### The settings pages

**Hold HOME** (0.7 s) for the settings; **SELECT** walks them, FX leaves. First **FILTERS** and **SCREAM**: the piece's, saved with it. **Settings**: **Key**, the drop's key change (same, +2, +3, +5, +7 semitones; `KEY` in the header); **Save**, Anytime (a store while playing is written at once, with a short dip) or At stop (written when the music stops); **Form**, Song (a piece moves through its patterns; Few rests or No rests: the parts rest half as often, or never) or Loop; **Start**, Last piece or Random. Then **Auto DJ**, **Auto FX**, **SET** (Rounds, Ramp, Intro, **Mix**: the next piece glides from the old tempo to its own over 4-32 bars), **USB & MIDI** (USB level, MIDI out, MIDI map), **MIDI IN**, **SCREEN** (**Zoom**: a knob turned shows large; Saver, the screensaver; Lights; Look: plain, scanlines, calm) **SAMPLE** (PERF's sounds one at a time, to sample), **SOUND** (Vintage drift, Polish, Tilt, Width), **CHORUS** (Juno I, II, I+II, rotary), **RESONATOR** (hits ringing in the chord), **RETURNS** (the echo's and reverb's duck: Mix, Off, Custom) and **LINK** (two units as one band). **Global** (GLO): Root, Theme (Console, the default; Imp, Synth, Acid, Ice, Ember, Phosphor: colour sets of the one layout), Tune (+-50 cents), Genre.

### Visualizers

**HOME twice quickly**; HOME again leaves. **SELECT** chooses (remembered): PLASMA, KALEIDO, BARS, FLUX, DISCO, LIFE, ORBIT, TRIP, THEORY (the circle of fifths, the chord and the next), AUTO. The knobs stay the FX knobs, ALGORITHM the mood, the keys the track mix; FX opens its menu over them.

### What is remembered across power-off

**Kept:** the pieces you stored, in any genre (written at once); theme, Tune and MIDI out; every effect's settings, the gate's pattern and targets; the Auto DJ, SET, Settings, USB & MIDI and Jam pages; the visualizer; your DX7 voices; the panel calibration. Settings are written a couple of seconds after you stop changing them (2 s stopped, 20 s playing), with a short dip in the music. **Not kept:** anything latched in FX, the FX knobs, the mood, mutes and solos, scenes, a running auto-DJ, unstored edits. A genre sets its own tape, vinyl and pump settings when you switch to it.

### The load line

How hard the FM-1 is working: `CPU 41 XR 0`, with `C2 38` and `S1` under it (top right of the mix map, GLO held, after a dropout or while easing off). **CPU**: how busy the main processor is. **XR**: dropouts, each one a click. **C2 nn**: the second processor, nn % busy (pad and chorus, DX voices, reverb); `C1`: it stopped after a fault. **S1**: the enhancers off (chorus, DX shimmer, RESONATOR), three snips at most. **S2**: DX voices held to four, two snips at most. **S3**: to two, and a lighter mix. It goes back after twenty quiet seconds. Heaviest: DX plucks with the shimmer reverb, and the fast genres.

</div>

---

## The computer side

<div class="cols">

**USB audio.** The FM-1 is a stereo USB audio input (16-bit, 44.1 kHz) carrying the master output. The USB & MIDI page's Level: KNOB (follows MASTER) or FIXED (a recording stays put when you turn the speakers down).

**MIDI out** (USB): the pad on channel 1, pluck 2, bass 3, acid 4, drums 10 (General MIDI notes), and MIDI clock with start, continue and stop. **MIDI map**, **MIDI IN**: a sampler plays the drums; an MPC, M:S or MPK mini plays PERF (manual, 14). **Notes: CHORDS**: a keyboard's keys below middle C are a chord the song follows (the lowest key the bass: inversions, slash chords; one key, the scale's chord on it), held until the next; from middle C up they play the voice. Notes played by hand go out too; the DJ snips don't.

**MIDI in:** notes on channel 1 play like the keys (the page's voice, or Jam's, snapped to the scale; on Global they edit the scale). Clock and start / stop drive the tempo (`EXT`). It listens on USB and on the TRS input.

**The site**, https://deamonslinky.github.io/PERF-1/: the **installer**, the **emulator**, the **editor**, the **library** and the **manual**.

**Backup and restore** (installer). **Save a backup** copies everything stored (pieces in every genre, settings, DX7 voices, theme, Tune, MIDI out) to a `.p1bak` file; do it before you install anything else. Unstored edits and the panel calibration aren't in it. **Restore the chosen backup** writes it back one item at a time, then the FM-1 restarts.

**The editor** has two tabs. **Sound**: every setting of the piece playing, grouped by voice. Changes play at once; store the piece on the FM-1 to keep them. **DX7 voices**: read the 16 slots and edit a voice, every operator included; **Live** sends each change as you make it; load and save `.syx` banks.

**Your own DX7 voices**: up to 16, kept after power-off, after the Pluck page's 60 built-in DX voices. From the installer: **Load DX7 voices**, choose `.syx` files (32-voice banks, single voices, bare 4096-byte banks), tick up to 16, press **Send the chosen voices**. **Clear the loaded voices** empties the slots. A librarian app's bank dump to the FM-1's USB MIDI port works too (the first 16; a single voice adds one). Levels aren't matched; a piece stores the slot number. Free banks: Bobby Blues' DX7 archive and others (manual, section 7).

**The emulator** runs the firmware in the browser. Drag or scroll a knob, click buttons and keys; Shift-click (or right-click) holds a button until the next click. **New start** restarts it (stored pieces stay, in that browser); **Forget stored** clears them. The computer keys are in the manual, section 17 (Space PLAY, Q-I and A-K the white keys).

**Remote:** with the FM-1 on USB and the emulator's **Connect MIDI** pressed, stop the FM-1, hold **SEL** and press **GLO**. It says REMOTE, goes quiet, and its whole panel plays the emulator, lights and all. SEL + GLO again ends it. **Stream view** (or `?stream`) is a clean 16:9 stage for OBS. **MIDI learn** maps any other controller to the emulator (kept in that browser).

**Live status and the logger.** The installer's **Live status** panel reads the FM-1's numbers four times a second (CPU, second processor, dropouts, late and slowest audio, what's cut back, genre and piece); **Save CSV** saves them for a bug report. Or log from a computer with `python tools/fm1log.py` (manual, section 16).

### Troubleshooting

- **No sound:** MASTER; PLAY lit; a Level at 0 (KNOB1 on its page; the Mixer shows all); `BY HAND` (SEL + ENV); a mute on the mix map; an effect latched (`FX` in the header; OCT- in the menu clears all). Jam silences the voice it plays; REMOTE silences the FM-1 on purpose.
- **A short dip when you store or change a setting:** normal. **A stored piece came back as before:** the power went off during the save. **My changes vanished:** only a stored piece (Global, SAVE) is kept.
- **The installer doesn't find the FM-1:** Chrome or Edge, a data cable, no DAW using the port, MIDI with SysEx allowed. **It offers an old version:** hard-refresh. **An install was cut off:** run it again. **`PERF-1 SAFE MODE`:** two crashes at start; reinstall, or PLAY to retry.
- **Buttons or knobs don't match their labels:** hold OCT- and OCT+ while switching on to calibrate.
- **Loaded DX7 voices don't show:** they come after the 60 built-in DX voices; check for `LOADED`.
- **Crackles or dropouts:** watch `XR` and `S1`/`S2`; send a Live status CSV with the piece.
- **The emulator doesn't see the FM-1:** no DAW on the port, not Safari.

### Credits and licence

PERF-1 is by **deamonSLINKY**: the firmware, the slinky mascot and the pages around them. It is a fork of **HYP-1** (RedtailWorks), itself a fork of **AMB-1** by Charles Vestal. The platform layer (USB, update loader, storage, installer, the panel calibration) is **Felucca** by Leo Kuroshita (Hügelton Instruments), by way of X0X and FoMni. The six-operator FM engine is **Melodee**'s, by Kerem Kilic (Ellic Studio), and so are the second-core work, the USB MIDI host and the TRS MIDI fix. The FM is after **Dexed** by Pascal Gauthier, on **MSFA** by Raph Levien (Google). The install-rescue boot stub is **ChoralRoot**'s. The bass line modes, pad comping, ghost notes, deja vu and the motif follow ideas in **Tuna** by delirioux. The plate reverb is Jon Dattorro's. The RESONATOR is after Mutable Instruments' **Rings** (MIT) and **DUB-1**. Auto FX is after **DARK-1**, the punch-ins after Caustic. The DX7 voices are public-domain patches from Bobby Blues' and Dave Benson's pages. The emulator idea is Groove OS's, the hardware knowledge FM-1-RE's, the rescue route FM-1-transporter's. Fonts: Chakra Petch and Barlow Semi Condensed (SIL OFL 1.1). Source and issues: https://github.com/deamonSLINKY/PERF-1. **Licence: GPL-3.0-only, no warranty.** M-VAVE and FM-1 are trademarks of their owners. PERF-1 is not affiliated with M-VAVE.

</div>
