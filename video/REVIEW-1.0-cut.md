# Notes on ChatGPT's 1.0 release video: the Final Cut (9:49), 2026-11-18

For the maker to pass on. Checked every 5 s, the loudness every 5 s (momentary LUFS), and the silences.

## Fixed since the Director cut (checked)

- Says what PERF-1 is in the title ("A generative groovebox you play").
- Mood rises 54 -> 76 (0:24-0:34).
- Sound shaping is at the start: acid filter and reso, answer, pluck NEUTRNPLUK 78 / Déjà vu 78 / motif 76 / leaps 34, drum drift, the master.
- Captions are bigger. The "beat of silence" line is gone, and so is "37 min" ("Tested extensively on hardware, with safeguards to reduce dropouts").
- The resonator targets PLUCK, not PERC.
- Custom bass, acid and pluck lines after the beat, with the phrase recorder.
- Chords sent from a drawn MIDI keyboard, and the link as two drawn units with "direct USB-C".
- A longer, warmer thank-you; the credits run over THEORY, then TRIP.

## Still to fix

1. **The loudness jumps 5 dB twice.** 0:10-1:05 is about -14.7 LUFS. After "Shape the master" (6:1, +9.6 dB make-up) it's -10 to -9.4 until
   5:47. At HIP HOP (5:50) it drops back to -14 to -15, because the new piece doesn't carry the master setting. The rebuilt wub opens at
   -21 (6:15) because it starts in a BREAK. YouTube turns the whole video down to -14, so the second half will sound weak. Either keep the
   master setting for every piece, or level each section to about -14 in the mix. Start the wub section on the drop, not the break.
2. **The screen doesn't match the section** in three places:
   - 1:15-1:28 "Pull it apart" still shows the MASTER COMP/LIMIT page, not the mix map;
   - 4:03-4:08 "Duck the returns" shows FX OFF / the set list;
   - 5:28-5:35 "Play it. Keep it." shows PAD PROGRESSION.
   Close the page before the next section starts, or cut later.
3. **A missing glyph:** the caption at 4:00 reads "Off [box] Custom". The caption font has no arrow; use "to" or "->".
4. **A 0.42 s dropout at 3.8 s**, as the intro hands over to the title card.
5. **The intro still uses ChatGPT's own 3D model.** Its knobs run down the right side, unlike the FM-1. The drop-in Cycles fly-in is ready:
   `G:\fm1\video-render\director-intro-claude\frames\intro-0001.jpg .. 0132`, made from the same 132 LCD frames, landing where the title card's
   screen starts. Use it with `INTRO = RENDER / "director-intro-claude" / "frames"` in render-recut.py.
6. **DRUM DRIFT PATTERN n pop-ups** cover the screen's top line at 1:15, 1:30, 3:35, and in the TRIP credits (9:45). Drift set to NEXT
   every 8 bars shows a pop-up each change. Set Drift back to OFF for the credits, or time the shots between changes.

## Still worth doing (from the first notes)

- **The screen is small** in Console, Song mode, Build, the genres, and the DX7 voices: about a quarter of the frame, beside a headline and
  decorative waves or level bars. It should fill at least 60% of the frame height whenever it's the subject.
- **Web Studio (7:38-8:48)** is still a full UI shrunk to half the frame and can't be read. Zoom 2-3x on the control that changes.
- **Long holds:** Song mode 3:05-3:28 on one layout, Ambient 5:57-6:13 and the wub 6:13-6:35 with a drawn wave. Change the shot every 4-8 bars.
- **Cream cards** are still there at 0:05 (the title) and 3:00 ("What's new"), flashing against the dark look.
