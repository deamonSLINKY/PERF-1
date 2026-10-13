# HYP-1: hypnotic techno and acid for the M-VAVE FM-1

A fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal, by RedtailWorks. It keeps
AMB-1's platform and generative harmony and turns it into something closer to a small modular: generative
voices you shape over time, over drums you sequence.

- **Pad**: four breathing voices (Rhodes or Voices) with their own envelope page (attack, hold,
  release, rest) and LFO page (rate, depth, shape, target).
- **Noise**: eight colours (white, pink, brown, blue, violet, grey, black crackle, yellow), a tone
  filter and its own sidechain pump.
- **Drone**: detuned saws and a sub through a resonant 24 dB low-pass, with an envelope (on each chord
  or every 1–8 bars) and an LFO (16-bar sine on the cutoff by default).
- **Acid**: a generative 303-style line: 1–16 steps of chord tones with accents and slides,
  resonant filter, envelope, saw or square. Mutate re-rolls steps as it plays.
- **Drums**: a 16-step sequencer with six lanes (kick, snare, clap, closed hat, open hat, perc), six
  patterns, and a level, tune and decay per lane. A new pattern waits for the end of the bar, so
  dialling past others plays none of them. Hold SEQ for the sequencer: the 16 white keys are the
  steps (lit where the drum hits, tap to toggle), OCT−/+ choose the drum.
- **SAVE** is the breakdown: tap for a breakdown on the next bar, again for a four-bar build and the
  drop; hold for the drop on the next bar.
- **FX** toggles a menu of momentary effects on the keys: beat repeats, reverse, tape stop, mutes and
  solos, filters, trance gate, echo throw, crusher, drum fill. Hold keys and press SAVE to lock their
  effects on; tap any key or close FX to let them go.
- **Pieces**: ten, numbered 01–10 on PRESETS, each with its own settings, its own six drum patterns
  and its own drum sounds. On the Global page, SAVE stores the piece as it is (hold it for the factory
  settings). Stored pieces go to flash as soon as you stop (a write would interrupt the sound). Each
  power-on starts on a random piece: as stored, or its factory settings slightly nudged if never stored.
  The drum patterns are written as grids in `tools/gen_pats.py`.
- **Styles** (SEL + ALGORITHM, stored with each piece): Hypnotic (the original sound), House (round kick,
  shuffle, off-beat chord stabs), Techno (hard kick with rumble, metal hats, gritty drone), Acid (punchy
  kit, cowbell, driven 303) and Breakcore (distorted, crushed, chopped drums, acid at half time).
  Switching takes the tempo into the style's range; the tempo goes from 70 to 190.
- **SEL is a shift**: hold it and white keys 1–6 pick the pattern, PRESETS picks a piece that comes in
  on the next bar, ALGORITHM picks the style. In the sequencer, SEL + black keys 1–6 copy the pattern, SEL + OCT− clears the
  drum's lane and SEL + OCT+ restores the piece's own.
- **Global**: root, five screen themes, tune, MIDI out (these three are the player's, kept across pieces).
- A RedtailWorks card at boot, and Chakra Petch (SIL OFL) as the screen's font.

`web/emu/build_clang.sh` builds the browser emulator with plain clang (no Emscripten). Demo renders are in
`demo/`. The manual in `docs/` is still AMB-1's.

---

# AMB-1 for the M-VAVE FM-1

AMB-1 turns the M-VAVE FM-1 into a generative ambient machine, inspired by Brian Eno's
*Music for Airports*. It plays itself from the moment it's switched on: breathing voices, soft
bells on tape loops of unequal length, a gliding bass and quiet Euclidean percussion, all through
a tape echo, a long tape loop and a plate reverb. You don't perform it, you tend it: pick a piece
and a scale, take notes out, put them back, and let it grow.

![AMB-1's screen: the garden](docs/img/screen-garden.png) ![Voices](docs/img/screen-voices.png) ![Frozen](docs/img/screen-frozen.png)

- **Try it in your browser** (no FM-1 needed): <https://charlesvestal.github.io/fm1-amb/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://charlesvestal.github.io/fm1-amb/install/>
- **Read the manual**: <https://charlesvestal.github.io/fm1-amb/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))
- **Download the firmware file**: [releases](https://github.com/charlesvestal/fm1-amb/releases)

Installing it is at your own risk, though it's hard to get stuck: the web installer can put
M-VAVE's own firmware back, and an FM-1 that fails to start twice comes up in a safe mode the
installer can reach.

## In short

| | |
|---|---|
| Keys | The scale: each key adds its note or takes it out |
| PLAY · REC · EDIT | Grow / rest · freeze · the next chord |
| HOME · ENV · ARP · LFO · SEQ · FX · GLO | The pages: Garden, Pad, Bells, Bass, Pulse, Space, Setup; KNOB 1–4 set their values, a layer's level on KNOB 1 |
| SELECT | The previous or next page; with HOME held, the tempo |
| ALGORITHM · PRESETS | The mode · the piece (eight of them) |
| OCT− / OCT+ | The bells' octave |
| SAVE | Saves; held, puts the piece back to its recipe |

Hold any button for a second and a card says what it does.

The [manual](https://charlesvestal.github.io/fm1-amb/manual/) has the rest.

## Building and testing

The toolchain is the same as X0X's (see [BUILDING.md](BUILDING.md)):

```
tests/run_tests.sh      # the instrument, the platform pieces, the app in a simulator, the web builds
./build.sh              # build/amb.fwsc
python3 tools/fm1_install.py build/amb.fwsc     # install it over USB
```

`build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out. `web/emu/build.sh` builds the
same code for the browser (the emulator); `web/app/build.sh` builds the engine alone into a web
app for phone and desktop.

## Credits

The idea comes from Brian Eno's generative music: *Music for Airports*' tape loops of unequal
length, and the echo chain of *Discreet Music*. The reverb is Jon Dattorro's plate ("Effect Design,
Part 1", JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); the Euclidean
rhythms are Godfried Toussaint's reading of Bjorklund's algorithm. The platform (hardware layer,
USB, update loader, storage, installer) is [Felucca](https://github.com/hugelton/Felucca)'s by Leo
Kuroshita (Hügelton Instruments), by way of [X0X](https://github.com/charlesvestal/fm1-x0x) and
FoMni-1. The idea of running FM-1 firmware in the browser comes from
[Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. AMB-1 is not affiliated with M-VAVE, Brian Eno or
Hügelton Instruments.
