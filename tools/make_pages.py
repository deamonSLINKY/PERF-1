#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the HYP-1 site for GitHub Pages (https://rustypawz.github.io/fm1-HYP1/; the workflow runs this):

  index.html                  what HYP-1 is, the video, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        HYP-1 in the browser (build/emu, from web/emu/build.sh)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/amb-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/amb-1.0.21.fwsc 1.0.21 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_7XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = os.environ.get("REPO_URL", "https://github.com/RustyPawz/fm1-HYP1")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HYP-1</title>
<meta name="description" content="HYP-1: hypnotic techno and acid firmware for the M-VAVE FM-1. Generative voices you shape like a small modular, over drums you sequence. By RedtailWorks.">
<meta name="theme-color" content="#141318">
<link rel="icon" type="image/png" href="img/favicon.png">
<meta property="og:title" content="HYP-1: hypnotic techno and acid for the M-VAVE FM-1">
<meta property="og:image" content="img/hyp-hero.png">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600;700&family=Barlow+Semi+Condensed:wght@400;500;600&display=swap">
<style>
/* the boot screen's colours: a charcoal ground, HYP-1's purple and orange, and the red panda's rust and
 * cream for RedtailWorks (the same on every page: landing, installer, emulator, manual) */
:root { --ground: #141318; --card: #1f1c27; --card-2: #2a2534; --ink: #f1ecf4; --muted: #aaa1b8; --rule: #3e3a4c;
        --purple: #a44cff; --purple-2: #c18cff; --orange: #ff8020; --rust: #c85420; --cream: #f6ecde; color-scheme: dark; }
* { box-sizing: border-box; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 28px 64px;
       font: 400 18px/1.6 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 1040px; margin: 0 auto; }
h1, h2, h3, .ways strong, .tag { font-family: "Chakra Petch", "Helvetica Neue", Arial, sans-serif; }
header.site { display: flex; flex-wrap: wrap; align-items: baseline; gap: .4rem 1.4rem; margin: 0 0 1.2rem; }
header.site { align-items: center; }
header.site .home { font: 700 1.4rem "Chakra Petch", sans-serif; letter-spacing: .06em; color: var(--ink); text-decoration: none;
                    display: inline-flex; align-items: center; gap: .6rem; }
img.panda { image-rendering: pixelated; display: block; height: auto; }
header.site nav a.yt, footer a.yt { color: var(--orange); }
header.site nav { margin-left: auto; display: flex; flex-wrap: wrap; gap: .3rem 1.1rem; font-size: 1rem; }
img.hero { width: 100%; height: auto; display: block; border-radius: 14px; margin: 0 0 1.6rem; }
.lede { font-size: 1.3rem; line-height: 1.5; max-width: 44rem; margin: 0 0 1.8rem; }
.lede b { color: var(--purple-2); font-weight: 600; }
h2 { font-size: 1.5rem; font-weight: 700; letter-spacing: .04em; text-transform: uppercase; margin: 3rem 0 1rem;
     display: flex; align-items: center; gap: .7rem; }
h2::before { content: ""; width: 34px; height: 5px; background: linear-gradient(90deg, var(--orange) 0 62%, var(--purple) 62%); }
h3 { font-size: 1.1rem; font-weight: 600; letter-spacing: .03em; margin: 0 0 .3rem; color: var(--purple-2); }
.ways { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 12px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 10px; padding: 14px 16px; transition: border-color .15s, background .15s; }
.ways a:first-child { background: linear-gradient(135deg, #5a2a9a, #2a1b44); border-color: var(--purple); }
.ways a:nth-child(2) { border-color: var(--orange); }
.ways a:nth-child(5) { border-color: var(--rust); }
.ways a:hover, .ways a:focus-visible { border-color: var(--purple-2); background: var(--card-2); outline: none; }
.ways strong { font-weight: 700; font-size: 1.1rem; letter-spacing: .05em; text-transform: uppercase; display: block; }
.ways span { color: var(--muted); font-size: .98rem; }
.ways em.v { font-style: normal; text-transform: none; letter-spacing: 0; color: var(--orange); }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(290px, 1fr)); gap: 14px; }
.card { background: var(--card); border: 1px solid var(--rule); border-radius: 10px; padding: 16px 18px; }
.card p { margin: 0; color: var(--muted); }
.card p b { color: var(--ink); font-weight: 600; }
.pair { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 14px; margin: 0 0 1rem; }
figure { margin: 0; }
figure img { width: 100%; height: auto; display: block; border-radius: 10px; border: 1px solid var(--rule); }
figcaption { color: var(--muted); font-size: .95rem; margin-top: .4rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
.shots img { image-rendering: pixelated; }
table { border-collapse: collapse; width: 100%; font-size: 1rem; }
th, td { text-align: left; vertical-align: top; padding: .5rem .8rem .5rem 0; border-bottom: 1px solid var(--rule); }
th { font: 600 .85rem "Chakra Petch", sans-serif; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
td:first-child { font-family: "Chakra Petch", sans-serif; font-weight: 600; white-space: nowrap; color: var(--purple-2); }
kbd { font: 600 .85em "Chakra Petch", sans-serif; background: var(--card-2); border: 1px solid var(--rule); border-radius: 4px; padding: 0 .35em; color: var(--orange); }
p.small, .credits { color: var(--muted); font-size: .95rem; }
a { color: var(--ink); text-decoration-color: var(--purple); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { color: var(--purple-2); }
code { font-size: .9em; background: var(--card-2); padding: .05em .35em; border-radius: 4px; }
footer { margin-top: 3.5rem; padding-top: 1.2rem; border-top: 1px solid var(--rule); display: flex; flex-wrap: wrap; gap: .6rem 1.6rem; align-items: center; }
footer .rw { font: 700 1.05rem "Chakra Petch", sans-serif; letter-spacing: .08em; text-decoration: none; display: inline-flex; align-items: center; gap: .55rem; }
footer .rw span:nth-child(2) { color: var(--purple-2); } footer .rw span:last-child { color: var(--orange); }
.video { margin: 0 0 1.8rem; }
.video .frame { position: relative; aspect-ratio: 16 / 9; border-radius: 12px; overflow: hidden; border: 1px solid var(--rule); background: #000; }
.video iframe { position: absolute; inset: 0; width: 100%; height: 100%; border: 0; }
.video figcaption { display: flex; flex-wrap: wrap; gap: .3rem 1rem; justify-content: space-between; }
.video figcaption a { color: var(--orange); }
footer .ver { font-family: "Chakra Petch", sans-serif; color: var(--muted); font-size: .9rem; }
@media (max-width: 560px) { .lede { font-size: 1.15rem; } h2 { font-size: 1.25rem; } }
</style>
</head>
<body>
<main>
<header class="site"><a class="home" href="./"><img class="panda" src="img/panda.png" width="64" height="50" alt="">HYP-1</a>
  <nav><a href="emu/">Emulator</a><a href="install/">Install</a><a href="manual/">Manual</a><a href="manual/HYP-1_Manual.pdf">PDF manual</a><a href="board/">Board</a><a href="__REPO__">Source</a><a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a><a href="https://www.redtail.works">RedtailWorks</a></nav></header>

<img class="hero" src="img/hyp-hero.png" width="1600" height="1161" alt="An M-VAVE FM-1 running HYP-1: the screen shows piece 01 in A Aeolian at 128 BPM, the chord Am7, the drum pattern and the Tempo page; the sixteen white keys glow purple with the scale.">

<p class="lede"><b>HYP-1</b> turns the M-VAVE FM-1 into a hypnotic techno and acid machine. It plays from the moment
it's switched on: a breathing pad, a deep drone, a generative 303 line and grain, over drums you sequence.
You shape the voices like a small modular, flip patterns on the bar, break it down, build it up, and record
the whole performance into your DAW.</p>

<figure class="video">
  <div class="frame"><iframe src="https://www.youtube-nocookie.com/embed/0UvGnZE2_VI?rel=0" title="HYP-1: a five-minute live take on the M-VAVE FM-1" loading="lazy"
    allow="accelerometer; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe></div>
  <figcaption><span>Five minutes played live on piece 01: no edits, just the firmware.</span><a href="https://www.youtube.com/watch?v=0UvGnZE2_VI">Watch on YouTube</a></figcaption>
</figure>

<nav class="ways" aria-label="Get HYP-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Every control, every page, every screen, on one page.</span></a>
  <a href="manual/HYP-1_Manual.pdf"><strong>PDF manual</strong><span>Printable, with every control numbered and every screen lettered.</span></a>
  <a href="board/"><strong>Board</strong><span>Request a feature, vote for others, or tell us how it's going on your FM-1.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
</nav>

<h2>The voices</h2>
<div class="grid">
  <div class="card"><h3>Pad</h3><p>Four breathing voices, Rhodes or sung, each on its own cycle, so the chord forms and dissolves. <b>Env</b> and <b>LFO</b> pages shape the breath and move the filter, level, pan or pitch.</p></div>
  <div class="card"><h3>Drone</h3><p>Detuned saws and a sub through a resonant 24 dB low-pass, with its own <b>envelope</b> (on each chord, or every 1&ndash;8 bars) and <b>LFO</b>: a slow 16-bar sweep by default.</p></div>
  <div class="card"><h3>Acid</h3><p>A 303-style line written as it plays: 1&ndash;16 steps of chord tones with accents and slides, resonant and driven. <b>Mutate</b> rewrites a few steps each time round.</p></div>
  <div class="card"><h3>Drums</h3><p>Kick, snare, clap, closed and open hats, perc. A <b>16-step sequencer</b> with six patterns per piece; a new pattern waits for the bar.</p></div>
  <div class="card"><h3>Noise</h3><p>A bed of grain in eight colours (white, pink, brown, blue, violet, grey, crackle, radio hiss), with its own tone and sidechain pump.</p></div>
  <div class="card"><h3>Space</h3><p>A tape echo, from an eighth to a bar, and a plate reverb, from a room to a long hall.</p></div>
</div>

<h2>Styles</h2>
<figure><img src="img/hyp-styles.png" width="2040" height="568" alt="Four HYP-1 screens: Hypnotic, House, Techno and Breakcore, each named at the left of the line above the knobs."></figure>
<p>Hold <kbd>SEL</kbd> and turn <kbd>ALGORITHM</kbd>, and the same piece is reshaped: every voice's sound and the feel change, your patterns and settings stay. Each piece keeps its own style.</p>
<table>
  <thead><tr><th>Style</th><th>BPM</th><th>What it does</th></tr></thead>
  <tbody>
  <tr><td>Hypnotic</td><td>110&ndash;145</td><td>Deep kick, metallic 808-style hats, breathing pad, smooth drone.</td></tr>
  <tr><td>House</td><td>118&ndash;128</td><td>Round kick, shuffle, wide clap, long open hats, congas; the pad stabs chords on the off-beats.</td></tr>
  <tr><td>Techno</td><td>128&ndash;138</td><td>Hard, driven kick with a rumble tail, metal hats, a bell perc; dark pad, gritty drone.</td></tr>
  <tr><td>Acid</td><td>120&ndash;132</td><td>Punchy 909-style kit with a cowbell and a little swing; the 303 driven and up front.</td></tr>
  <tr><td>Breakcore</td><td>160&ndash;180</td><td>Distorted, crushed drums chopped live with ghosts and 32nd rolls; dirty sub; acid at half time.</td></tr>
  </tbody>
</table>

<h2>Play it</h2>
<div class="pair">
  <figure><img src="img/hyp-device-seq.png" width="1600" height="1161" alt="The FM-1 in the sequencer: the kick's hits lit on the white keys."><figcaption><b>Hold SEQ:</b> the sixteen white keys are the steps of the drum you're on. Tap to toggle; <kbd>OCT&minus;</kbd> / <kbd>OCT+</kbd> choose the drum.</figcaption></figure>
  <figure><img src="img/hyp-device-fx.png" width="1600" height="1161" alt="The FM-1 with the FX menu open: three keys held, glowing orange."><figcaption><b>FX:</b> every key is a momentary effect: beat repeats, reverse, tape stop, mutes and solos, filters, trance gate, echo throw, crush, drum fill. Hold <kbd>SAVE</kbd> and tap to lock any number on.</figcaption></figure>
</div>
<div class="grid">
  <div class="card"><h3>Ten pieces</h3><p><b>PRESETS</b> picks 01&ndash;10, each with its own settings, six drum patterns and drum sounds. On Global, <b>SAVE</b> stores the piece; it's written while the music is stopped, so it never interrupts a take.</p></div>
  <div class="card"><h3>Breakdowns</h3><p><b>SAVE</b> drops a breakdown on the next bar; tap again for a four-bar build and the drop, or hold it for the drop straight away. <b>REC</b> freezes the chord.</p></div>
  <div class="card"><h3>Shift</h3><p>Hold <b>SEL</b>: white keys 1&ndash;6 jump to a pattern on the bar, <b>PRESETS</b> changes piece on the next bar, and in the sequencer you copy, clear and restore lanes.</p></div>
  <div class="card"><h3>Links</h3><p>Patch the drums into the voices: each <b>kick</b> opens the drone's filter, <b>snares</b> flash the pad, <b>hats</b> accent the acid, and the <b>perc</b> lane can clock the acid line. <b>SEL + SEQ</b> turns them on and off.</p></div>
  <div class="card"><h3>Remote</h3><p>Plug the FM-1 into a computer, open the <a href="emu/">emulator</a> and press <b>SEL + GLO</b>: its whole panel plays the browser, lights and all. Any other MIDI controller works too, with MIDI learn.</p></div>
  <div class="card"><h3>Always different</h3><p>Every power-on starts on a piece at random: as you stored it, or slightly nudged from its factory settings. Chords drift, the acid mutates, a slow tide moves between sparse and full.</p></div>
</div>

<h2>Screens</h2>
<div class="shots">
  <figure><img src="img/hyp-screen-splash.png" width="480" height="480" alt="The boot screen: a pixel red panda sitting on the RedtailWorks name"><figcaption>Switching on</figcaption></figure>
  <figure><img src="img/hyp-screen-splash-hyp.png" width="480" height="480" alt="The second boot screen: HYP-1 in big pixel letters, steps filling in"><figcaption>&hellip;then HYP-1</figcaption></figure>
  <figure><img src="img/hyp-screen-home.png" width="480" height="480" alt="Tempo page"><figcaption>Tempo</figcaption></figure>
  <figure><img src="img/hyp-screen-seq.png" width="480" height="480" alt="The sequencer"><figcaption>Sequencer</figcaption></figure>
  <figure><img src="img/hyp-screen-fx.png" width="480" height="480" alt="The FX menu"><figcaption>FX menu</figcaption></figure>
  <figure><img src="img/hyp-screen-acid-line.png" width="480" height="480" alt="The Acid line page"><figcaption>Acid line</figcaption></figure>
  <figure><img src="img/hyp-screen-breakdown.png" width="480" height="480" alt="A breakdown"><figcaption>Breakdown</figcaption></figure>
  <figure><img src="img/hyp-screen-stored.png" width="480" height="480" alt="Storing a piece on Global"><figcaption>Storing a piece</figcaption></figure>
</div>
<figure style="margin-top:14px"><img src="img/hyp-themes.png" width="1344" height="328" alt="The five screen themes: Synth, Acid, Ice, Ember, Phosphor."><figcaption>Five screen themes on Global: Synth, Acid, Ice, Ember, Phosphor.</figcaption></figure>

<h2>Credits</h2>
<p class="credits">HYP-1 is a fork of <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal, the generative ambient firmware it grew from.
The platform (USB, update loader, storage, installer) is <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (@kurogedelic), Hügelton Instruments.
Screen font: Chakra Petch (SIL OFL). GPL-3.0.
M-VAVE and FM-1 are trademarks of their owners; HYP-1 is not affiliated with M-VAVE.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
Installing firmware is at your own risk.</p>
<footer><a class="rw" href="https://www.redtail.works"><img class="panda" src="img/panda.png" width="48" height="38" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <span class="small">HYP-1 is a RedtailWorks project &middot; <a href="https://www.redtail.works">www.redtail.works</a> &middot; <a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a> &middot; <a href="__REPO__">source on GitHub</a></span><span class="ver">version __VERSION__</span></footer>
</main>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_7\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"hyp1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"amb_installer.html must contain {mark} once")
    page = page.replace("https://github.com/charlesvestal/fm1-amb", REPO).replace("github.com/charlesvestal/fm1-amb", REPO[8:])
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    man = out / "manual" / "index.html"                 # the build's own version, wherever the manual names one
    man.write_text(re.sub(r"HYP-1 version \d+\.\d+\.\d+", "HYP-1 version " + html.escape(version), man.read_text(encoding="utf-8")),
                   encoding="utf-8")
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    if (SRC / "docs" / "HYP-1_Manual.pdf").exists():
        shutil.copy(SRC / "docs" / "HYP-1_Manual.pdf", out / "manual" / "HYP-1_Manual.pdf")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    # the board: giscus over the repository's Discussions; config.json is what the workflow found
    # (BOARD_JSON: the GraphQL answer for the repository's id and Discussions categories), or nothing yet
    (out / "board").mkdir()
    (out / "board" / "index.html").write_text((SRC / "web" / "board.html").read_text(encoding="utf-8")
                                              .replace("__REPO_NAME__", REPO.split("github.com/")[-1]).replace("__REPO__", REPO),
                                              encoding="utf-8")
    shutil.copy(SRC / "web" / "board-giscus.css", out / "board" / "giscus-theme.css")
    board = {}
    try:
        r = json.loads(Path(os.environ["BOARD_JSON"]).read_text())["data"]["repository"]
        if r.get("hasDiscussionsEnabled"):
            board = {"repoId": r["id"], "categories": [{"id": c["id"], "name": c["name"]}
                                                       for c in r["discussionCategories"]["nodes"]]}
    except (KeyError, TypeError, ValueError, OSError):
        pass
    (out / "board" / "config.json").write_text(json.dumps(board), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, board/ ({'ready' if board else 'waiting for Discussions'}), firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
