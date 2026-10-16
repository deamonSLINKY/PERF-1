# LOF-1 samples

Builds LOF-1's built-in sampled instruments from two CC0 (public-domain) libraries by Versilian Studios,
[VCSL](https://github.com/sgossner/VCSL) and [VSCO-2 CE](https://github.com/sgossner/VSCO-2-CE), the same
libraries Felucca's built-in PIANO, FLUTE and SAX come from. CC0 means they can ship inside GPL firmware.

```
pip install numpy scipy
python3 lof1_samples.py                      # everything -> ./out
python3 lof1_samples.py --only UPBASS,KIT    # a few
python3 lof1_samples.py --rate 16000         # smaller and grittier
python3 lof1_samples.py --list               # the lineup
```

Only the first second or two of each WAV is downloaded (cached in `.cache/`). Edit `MANIFEST` at the top
of the script to change notes, lengths or instruments.

## What comes out (`out/`)

| Path | What |
|---|---|
| `BUDGET.md` | flash cost per instrument, totals, every zone's root note and key range |
| `preview/NAME.wav` | an audition: a short minor-pentatonic phrase played through the zones (or a beat, for the kit) |
| `preview/ALL.wav` | every audition back to back |
| `wav/NAME/*.wav` | each zone after compression, i.e. what the FM-1 will play, with root and loop in a `smpl` chunk |
| `sets/NAME.adpcm` | the 4-bit IMA ADPCM data, zones back to back |
| `sets/NAME.json` | the zone table: offset, length, loop, root, key range, ADPCM state at the loop start |
| `CREDITS.md` | every source file used |

The ADPCM is the same format Felucca plays (4-bit IMA, low nibble first, starting from predictor 0 /
index 0), and each zone's decoder state at its loop start is stored so loops restart exactly.

## Sample kinds

- **ONESHOT**: cut to a length, last quarter faded (kalimba, glock, strumstick, upright bass)
- **SUS**: attack plus a crossfaded loop, held as long as the key is (trumpet, sax, flute, harmonica)
- **DECAY**: short attack plus a tiny loop; the engine's envelope fades it (pianos, vibes). This is the
  trick that keeps pianos to about 7 KB per zone.
- **DRUM**: one-shots on single keys, no pitch mapping (the kit and the page 2 pads)

## Root notes

The libraries number octaves differently from set to set, so each instrument has an `oct` setting
(octaves added to the note in the file name). These were checked with two pitch methods and the
spectrum on every zone; detection is then used only to fine-tune the cents. If you add an instrument,
check its roots in `BUDGET.md` and the audition before trusting them.
