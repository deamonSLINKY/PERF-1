# PERF-1 0.15.0 update video: the run-through (draft 1, 2026-10-30)

Same method as the 0.14.0 tour: one device, one continuous take, no cuts, no zoom (the FM-1 whole, the screen enlarged beside it, cue rings on the controls, a caption card and a readout). Made from the firmware itself (`driver/scenes/`, Remotion). Target about 5:30-5:45 with the on-camera setup (trim the effects or the drop if it must be 5:00).

**The piece:** SYNTHWAVE 1 (W01, A Aeolian, 108 BPM, triads), chosen on camera. The device boots as it ships (HYPNO 01, the Imp theme); we stop it, pick the genre and the piece, switch the theme, set the intro on the SET page, and press Play: the whole video from there is one take of W01. Set out of sight beforehand (`prepare()`): only the FLUX visualizer for the credits. The pluck is W01's own DY voice, SHIMMERPAD (the one you liked); the DY moment turns Sound through a few names and comes back to it.

| time | on the device | what we say (card) | readout |
|---|---|---|---|
| 0:00 | switch on as shipped: HYPNO 01 playing, the Imp theme | **0.15.0** "The second update since the release. This time we set the stage first." | |
| 0:06 | tap PLAY: stopped | "Stop." | STOPPED |
| 0:08 | hold GLO, turn PRESETS to SYNTHWAVE; PRESETS to piece 1 (W01) | **THE PIECE** "Synthwave 1, for the whole video." | SYNTHWAVE W01 |
| 0:16 | Global page: Theme to Synth: the home shows the night sky, the sun at rest, the scanner | **THE SYNTH IMP** "The Synth theme is a look of its own now: a horned sun behind the sigil, a scanner in its centre, the imp in neon." | THEME: SYNTH |
| 0:26 | SEL + FX, dial to the SET page: Intro to 1 MIN, Rounds 2, Ramp +4; FX closes; HOME | **THE SET PAGE** "New: rounds a piece gets before the auto-DJ moves on, a tempo ramp over them, and an intro." | INTRO 1 MIN |
| 0:40 | tap PLAY: the header counts `INTRO 0:58`; the pad alone, then hats 4 s, perc 12 s, kick 24 s, snare 31 s, bass 37 s, pluck 43 s, acid 49 s | **INTRO** "The slow progression: from Play the tracks come in one by one over one to eight minutes. Mix in under a pad and a hat." then, as the tracks arrive, three cards: **THANK YOU** "To everyone who installed 0.14.0, wrote in, posted, asked for things: this update is mostly your ideas." / **YOUR IDEAS** "The intro you are hearing was one of them. The chord loops, the pluck steps, the bass figures, the rounds: more of them." / **KEEP THEM COMING** "Issues on GitHub, the Firmware Hub, the comments. Tell me what the music should do next." | INTRO |
| 1:05 | SEL + HOME: the imp into the sigil, bouncing on the kick; back to the scanner | "The imp, summoned into the sigil." | |
| 1:38 | everything in; Tempo page: Drift reads `2 bars`; the chord name under the sigil changes on the bar line | **HARMONY YOU CAN HEAR** "Each genre changes chord every so many bars now: two in synthwave. Minor keys get a real dominant; diminished chords are rare." | 2 BARS A CHORD |
| 1:55 | Chords page (ENV x4): Loop reads `4` | "Most genres loop a progression that starts from home: i, VI, III, VII, over eight bars." | LOOP: 4 CHORDS |
| 2:10 | Pad page: Sound to TRANCE, then BRASS stabs for two bars, back to the piece's pad | **A FULLER PAD** "The saw sounds sweep a resonant filter with every breath and stab, with a sub, four saws for the supersaws." | TRANCE / BRASS |
| 2:35 | Bass line page (LFO x4): Mode through OCTAVES, TWO-STEP, LOG DRUM, GALLOP, back to EIGHTHS | **THE BASS LINE** "Nine figures by name, in any genre. And it plays the fifth now, not the seventh." | the figure's name |
| 3:00 | Pluck page (ARP x6): Sound turned through DY names, TEMPLEBELL, CLAVINET, SOFTSTRNGS, and back to SHIMMERPAD | **DY VOICES** "Thirty human-made DX7 voices from public-domain banks are the default now, each under its own name. This one is SHIMMERPAD." | SHIMMERPAD |
| 3:20 | Pluck steps page (ARP x9): white keys pick steps; Level and Rung set three of them; `FROZEN` | **PLUCK STEPS** "Deja vu's loop frozen as sixteen steps you write: a rung of the chord's ladder and a level. Stored with the piece. The pluck is on MIDI channel 2 too." | STEP 5: ACCENT |
| 3:50 | GLO + F5: the DJ on; the header shows `R1/2` | "The auto-DJ, with the rounds and the ramp you set." | DJ ON · R1/2 |
| 4:00 | the auto-DJ's fill, break, build (the sun climbing), drop (white flash, the shock ring); the DJ off after | "The sun rises through the build and flashes on the drop." | BREAK / BUILD / DROP |
| 4:35 | Comp/Limit page: Ratio 6:1, Thresh -23 dB, Make-up +11.2 dB, Ceiling -10.1 dB, the curve and the meters | **THE MASTER BUS** "A stereo compressor and a look-ahead limiter on every piece. Push it and the limiter holds the ceiling." | 6:1 · -23 dB · +11.2 · -10.1 |
| 5:00 | FX menu: latch GATE 16 for two bars; SEL + key: its page, tap three steps, Depth knob; dial to TAPE, Depth | **EFFECTS, EDITABLE** "Tap latches, hold plays, SEL + key opens its page and it plays while you edit." | GATE 16 / TAPE |
| 5:20 | HOME twice: the FLUX visualizer; credits | credits: deamonSLINKY, the lineage, GPL-3.0, the site | |
| 5:40 | end | | |

## Things to decide (your notes)

1. **The opener**: stop, pick W01, switch the theme, set the intro, Play (as above, about 40 s before the music starts again); the intro 1 minute.
2. ~~The pluck voice~~: SHIMMERPAD, W01's own (decided).
3. **The drop sequence** again (45 s, it carried the last video) or shorter (Rounds 1, Break short).
4. Anything from the list below to swap in: the Groove page's density knob, Capture (SEL + REC on Jam) into the pluck steps, the Acid steps page, the stab palettes (a dub piece would break the one-song rule), the Mixer, the backup page.
5. The compressor values as given: 6:1, -23 dB, +11.2 dB, -10.1 dB (the knobs land exactly: Ratio 5, Thresh 23, Make-up 94, Ceiling 16).

## Not shown, said in a card or left out

- The pluck on MIDI channel 2 (a card line), the bass fifth fix (a card line), the stabs and comp masks (a dub piece would break the one-song rule), Scanlines (not musically interesting: left out), the DY names' provenance (the manual).
