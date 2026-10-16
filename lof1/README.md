# LOF-1 for the M-VAVE FM-1 (experimental)

LOF-1 turns the FM-1 into a lofi groovebox: you play the lead on the keys and a tired jazz band follows you.
The engine hears which chord your notes imply and the comp, bass and drums move with it on the beat; stop
playing and the band carries on by itself, and after a while a ghost takes the lead (Daydream). An Answer
voice replies to your phrases. Under it all, DUST: vinyl crackle, cassette hiss, reel-to-reel haze or radio
static, plus a granular smear of the music itself. A RedtailWorks project, alongside HYP-1.

> **Experimental.** Built and tested in the simulator and the browser emulator, not yet on a real FM-1.

- **Try it:** <https://rustypawz.github.io/fm1-HYP1/lof/emu/> (the same code, in the browser)
- **Install:** <https://rustypawz.github.io/fm1-HYP1/lof/install/> (Chrome or Edge, USB)
- **Manual:** [docs/MANUAL.md](docs/MANUAL.md), or <https://rustypawz.github.io/fm1-HYP1/lof/manual/>

## What's in it

- **The lead** on the keys: fourteen instruments, single notes or whole chords on each key, MIDI in too.
- **The band:** comp (nine instruments, four patterns, four voicings), bass (five, roots to walking to
  dub), an Answer voice (ten), and drums: a sixteen-step, eight-lane, eight-pattern sequencer on a sampled
  or a synthesized kit, with swing and fills.
- **Daydream:** a ghost lead when the keys rest, as sparse or busy as you set it. **Drift:** the drums and
  the progression change by themselves every few bars.
- **DUST and Space:** vinyl, cassette, reel or radio; grain; tape wow and saturation, a low-pass, a plate
  reverb, an echo.
- **FX on the keys:** stutters, record skip, half time, tape stop, backspin, muffle, radio, mutes, solos,
  throws, wobble, crush, fill, grain freeze, dropout; a map on screen; hold SAVE to lock them on.
- **Ten presets**, stored over with Global › SAVE. MIDI clock in, notes out (channels 1–4 and 10).
- **The screen** in five colour themes (Lamp, Matcha, Sakura, Midnight, Sepia), as if on an old TV (a
  curve, scanlines, a rolling band; Global 2 turns it off). And an easter egg: leave it untouched for half an
  hour and Red, RedtailWorks' red panda, turns up under a tree with a coffee, nodding on the beat.

## Build and test

```
tests/run_tests.sh                                   # every test, the simulator scenarios included
host/build_host.sh                                   # the simulator alone
build/host/amb_host tests/scenarios/lof_basic.amb build/scen
web/emu/build_clang.sh && python3 web/emu/make_artifact.py   # the browser emulator
./build.sh --release 0.1                             # the firmware (JieLi toolchain, as HYP-1)
```

[BUILDING.md](BUILDING.md) has the details: the toolchain, the renders, the site, and how HYP-1's
workflow builds LOF-1 alongside it.

## Instruments

The sampled instruments are Versilian Studios' CC0 libraries (VCSL, VSCO-2 CE), at 16 kHz, 4-bit IMA ADPCM:

```
python3 tools/samples/lof1_samples.py --rate 16000 --out build/samples \
    --only UPRIGHT,UPRIGHT2,VIBES,KALIMBA,GLOCK,STRUM,UPBASS,MUTETPT,TENORSAX,FLUTE,HARMONICA,KIT
python3 tools/gen_samples.py build/samples/sets firmware/src/dsp/lof_samples.h
```

The rest (Rhodes, Wurli, bell, sine, pad, organ, strings, sub, round, pluck, Moog, whistle, square) are
synthesized.

## The screen

Everything on the screen goes out through `firmware/src/app/crt.c`: a slight barrel curve, scanlines, a
vignette and a soft band rolling slowly down. There's no RAM for a frame buffer, so each strip is warped
from its own pixels on its way to the panel. The themes are `THEMES` in `firmware/src/app/ui.c`. The
screensaver's art is in `tools/art/`, packed into `firmware/src/app/saver_art.h` (about 40 KB) by
`tools/gen_saver.py`.

## Licence

GPL-3.0-only ([LICENSING.md](LICENSING.md)). The plate reverb, command queue, fast maths, platform and
emulator glue are AMB-1's (Charles Vestal) and Felucca's (Leo Kuroshita, Hügelton Instruments), by way of
HYP-1 and ECO-1. M-VAVE and FM-1 are trademarks of their owners; LOF-1 is not affiliated with M-VAVE.
