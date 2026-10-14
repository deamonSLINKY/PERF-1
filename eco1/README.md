# ECO-1 for the M-VAVE FM-1 (experimental)

ECO-1 turns the M-VAVE FM-1 into a small plant that plays music. Each of its three leaves holds a charge
that light and water build; when a leaf's charge is full, it fires a note. A stem drones underneath. The
day turns, rain comes and goes, the wind picks up, and the plant answers all of it: busy at noon, sparse at
night, wilting in a drought, shaking in a gale. A RedtailWorks project, alongside HYP-1 in this repository.

> **Experimental.** Built and tested in the simulator and the browser emulator, not yet on a real FM-1.
> Versions are 0.x until it has been. Install it at your own risk; HYP-1 or M-VAVE's firmware goes back on
> the same way.

![ECO-1 on the FM-1](docs/img/eco-hero.png)

![The eight plants](docs/img/eco-plants.png)

- **Try it in the browser:** <https://rustypawz.github.io/fm1-HYP1/eco/emu/>: with sound, MIDI in and
  out, and eight channels (each voice apart) for your DAW.
- **Install it** (Chrome or Edge, the FM-1 plugged in): <https://rustypawz.github.io/fm1-HYP1/eco/install/>
- **Manual:** <https://rustypawz.github.io/fm1-HYP1/eco/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))

## In short

| | |
|---|---|
| Keys | The scale: each key adds its note or takes it out |
| PLAY · REC · EDIT | Grow / rest · freeze the day and the weather · touch the plant |
| HOME · SEL · ENV · ARP · LFO · SEQ · FX · GLO | The pages: Plant, Stem, Low, Mid and Top leaf, Weather, Space, Setup (GLO again: MIDI out) |
| SELECT | The previous or next page; with HOME held, the day's length |
| ALGORITHM · PRESETS | The mode · the plant (Fern, Orchid, Moss, Cactus, Willow, Lotus, Ivy, Bamboo) |
| OCT− / OCT+ | An octave: the page's leaf or stem, else the whole plant |
| SAVE | Saves; held, puts the plant back to its recipe |

**To your DAW:** MIDI clock in (USB or TRS) syncs the day and puts the leaves on a grid; MIDI out sends each
leaf's notes on its own channel, the stem on another, and the plant's signals as CCs 20–31.

## Renders

`docs/img/` holds the renders: the device on several pages (`eco-device-*.png`, `eco-hero.png`), every
screen at 2× (`eco-screen-*.png`), and the sheets `eco-plants.png`, `eco-pages.png`, `eco-moods.png` and
`eco-boot.png`, all drawn from the firmware itself in the simulator. The workflow redraws them on each push.

## Building

Self-contained in this folder: see [BUILDING.md](BUILDING.md). HYP-1's workflow
(`../.github/workflows/firmware.yml`) builds it on every push, after HYP-1. Licensing:
[LICENSING.md](LICENSING.md) (GPL-3.0-only).

ECO-1 is a fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal, by way of HYP-1,
on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments.
M-VAVE and FM-1 are trademarks of their owners; ECO-1 is not affiliated with M-VAVE.
