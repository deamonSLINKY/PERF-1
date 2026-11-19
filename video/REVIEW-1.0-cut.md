# Notes on ChatGPT's 1.0 release cut (Director, 8:47; checked against the Final Cut's opening, 9:49), 2026-11-18

For the maker to pass on, alongside their own notes. Most recent notes already cover: say what PERF-1 is first, mood to 75, captions
too small, no settings lists, no "37 min", resonator off PERC, play-it-keep-it louder, credits with THEORY + TRIP at the end.

## Drop-in: the 3D fly-in (Claude, Cycles 1080p)

`G:\fm1\video-render\director-intro-claude\frames\intro-0001.jpg .. intro-0132.jpg`: same count, names and size as `director-intro\frames`,
made from the same LCD frames (`director-intro\lcd`), so the screen stays in sync. It ends straight down with the LCD 1054 px tall,
25 px from the top: where render-recut.py's launch card starts its screen inset (`inset(... 433, 25, 1054 ...)`). To use it:
`INTRO = RENDER / "director-intro-claude" / "frames"` in render-recut.py. Rebuild: `vis/video/blender/flyin_shot.mjs` then `blender/fm1.py --anim`.

## What else would lift it

1. **The screen is too small most of the time.** Console, Song mode, Build, the genres: the LCD is about a quarter of the frame, and the
   left half is a headline plus decorative level bars that repeat the strips. On a phone the screen's text is unreadable. When the screen
   is the subject, make it at least 60% of the frame height and put the words over or beside it.
2. **The panel drawing** shows a second, tiny copy of the screen. Show it only when a key or knob move is the point, and ring that control.
3. **Cream cards** (title, "What's new", "Thank you") flash against the dark look. Use the dark background with the cream as an accent.
4. **Web Studio** is a 1920 px UI shrunk to half the frame, so nothing is readable. Zoom 2-3x on the control being changed and follow
   the cursor.
5. **Long holds**: Song mode is about 25 s on one layout, the rebuilt wub 25 s of a waveform, Ambient 10 s of a drawn wave. Cut on the music
   (4 or 8 bars) and change the shot at each cut: screen close-up, panel with the key ringing, the 3D unit.
6. **The waveform decorations** take space and say nothing. Drop them, or make them the real meters.
7. **More of the 3D unit** (the maker's ask): the same model can render any slice of the take with the keys pressing and the screen
   live (`driver/blendshot.mjs` + `blender/fm1.py --anim`), about 5 s a frame on the 2080. Good for section openers (2 s each) and the link
   (two units side by side, one cable).
