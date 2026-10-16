#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the LOF-1 site: HYP-1's site has it under lof/ (the HYP-1 workflow runs this, then
tools/site/switcher.py adds the HYP-1 / LOF-1 dropdown to every page):

  index.html                  what LOF-1 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        LOF-1 in the browser (build/emu, from web/emu/build_clang.sh)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/lof1-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/amb-1.0.1.fwsc 1.0.1 OUT_DIR

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

REPO = os.environ.get("REPO_URL", "https://github.com/RustyPawz/fm1-HYP1/tree/main/lof1")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LOF-1</title>
<meta name="description" content="LOF-1: a lofi groovebox for the M-VAVE FM-1. You play the lead, a tired jazz band follows, an old record crackles under it. By RedtailWorks.">
<meta name="theme-color" content="#1a1624">
<link rel="icon" type="image/png" href="img/favicon.png">
<meta property="og:title" content="LOF-1: a lofi groovebox for the M-VAVE FM-1">
<meta property="og:image" content="img/lof-hero.png">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@400;500;600;700&display=swap">
<style>
/* late night: a violet-dark ground, lamp orange, a teal second colour, and the red panda's rust and cream
 * for RedtailWorks (the same on every page: landing, installer, manual) */
:root { --ground: #1a1624; --card: #231e30; --card-2: #2e283d; --ink: #f2eade; --muted: #b4a9c4; --rule: #3d3550;
        --purple: #7eccc0; --purple-2: #ffb07c; --orange: #e8873a; --rust: #c85420; --cream: #f6ecde; color-scheme: dark; }
* { box-sizing: border-box; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 28px 64px;
       font: 400 18px/1.6 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 1040px; margin: 0 auto; }
h1, h2, h3, .ways strong, .tag { font-family: "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
header.site { display: flex; flex-wrap: wrap; align-items: center; gap: .4rem 1.4rem; margin: 0 0 1.2rem; }
header.site .home { font: 700 1.4rem "Barlow Semi Condensed", sans-serif; letter-spacing: .06em; color: var(--ink); text-decoration: none;
                    display: inline-flex; align-items: center; gap: .6rem; }
img.panda { image-rendering: pixelated; display: block; height: auto; }
header.site nav a.yt, footer a.yt { color: var(--orange); }
header.site nav { margin-left: auto; display: flex; flex-wrap: wrap; gap: .3rem 1.1rem; font-size: 1rem; }
img.hero { width: 100%; height: auto; display: block; border-radius: 14px; margin: 0 0 1.6rem; }
.lede { font-size: 1.3rem; line-height: 1.5; max-width: 44rem; margin: 0 0 1.8rem; }
.lede b { color: var(--purple-2); font-weight: 600; }
h2 { font-size: 1.5rem; font-weight: 700; letter-spacing: .04em; text-transform: uppercase; margin: 3rem 0 1rem;
     display: flex; align-items: center; gap: .7rem; }
h2::before { content: ""; width: 34px; height: 5px; background: linear-gradient(90deg, var(--purple-2) 0 62%, var(--purple) 62%); }
h3 { font-size: 1.1rem; font-weight: 600; letter-spacing: .03em; margin: 0 0 .3rem; color: var(--purple-2); }
.ways { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 12px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 10px; padding: 14px 16px; transition: border-color .15s, background .15s; }
.ways a:first-child { background: linear-gradient(135deg, #5a3420, #241c2c); border-color: var(--purple-2); }
.ways a:nth-child(2) { border-color: var(--orange); }
.ways a:hover, .ways a:focus-visible { border-color: var(--purple); background: var(--card-2); outline: none; }
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
th { font: 600 .85rem "Barlow Semi Condensed", sans-serif; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
td:first-child { font-family: "Barlow Semi Condensed", sans-serif; font-weight: 600; white-space: nowrap; color: var(--purple-2); }
kbd { font: 600 .85em "Barlow Semi Condensed", sans-serif; background: var(--card-2); border: 1px solid var(--rule); border-radius: 4px; padding: 0 .35em; color: var(--orange); }
p.small, .credits { color: var(--muted); font-size: .95rem; }
a { color: var(--ink); text-decoration-color: var(--purple); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { color: var(--purple-2); }
code { font-size: .9em; background: var(--card-2); padding: .05em .35em; border-radius: 4px; }
footer { margin-top: 3.5rem; padding-top: 1.2rem; border-top: 1px solid var(--rule); display: flex; flex-wrap: wrap; gap: .6rem 1.6rem; align-items: center; }
footer .rw { font: 700 1.05rem "Barlow Semi Condensed", sans-serif; letter-spacing: .08em; text-decoration: none; display: inline-flex; align-items: center; gap: .55rem; }
footer .rw span:nth-child(2) { color: var(--purple-2); } footer .rw span:last-child { color: var(--orange); }
footer .ver { font-family: "Barlow Semi Condensed", sans-serif; color: var(--muted); font-size: .9rem; }
.exp { font: 700 .72rem "Barlow Semi Condensed", sans-serif; letter-spacing: .12em; text-transform: uppercase; color: #1a0f05;
       background: #e8873a; border-radius: 999px; padding: .15em .7em; margin-left: .2rem; }
.expnote { background: #2a1d10; border: 1px solid #e8873a; border-radius: 10px; padding: 12px 16px; margin: 0 0 1.4rem; color: #f3dcc4; }
.expnote b { color: #ffb070; }
@media (max-width: 560px) { .lede { font-size: 1.15rem; } h2 { font-size: 1.25rem; } }
</style>
</head>
<body>
<main>
<header class="site"><a class="home" href="./"><img class="panda" src="img/panda.png" width="64" height="50" alt="">LOF-1<span class="exp">Experimental</span></a>
  <nav><a href="emu/">Emulator</a><a href="install/">Install</a><a href="manual/">Manual</a><a href="__REPO__">Source</a><a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a><a href="https://www.redtail.works">RedtailWorks</a></nav></header>
<div class="expnote"><b>Experimental.</b> LOF-1 is new: built and tested in the simulator and in the emulator here, not yet on a real FM-1. Install it at your own risk; HYP-1, or M-VAVE's own firmware, goes back on the same way.</div>
<img class="hero" src="img/lof-hero.png" width="1600" height="1161" alt="An M-VAVE FM-1 running LOF-1: a record turning, the chord Dm7, following you; the Groove page's Tempo, Swing, Jazz and Busy below it.">
<p class="lede"><b>LOF-1</b> turns the M-VAVE FM-1 into a lofi groovebox. You play the lead on the keys and a
tired jazz band follows you: it hears which chord your notes imply and the comp, bass and drums move with it
on the beat. Stop playing and the band carries on; after a while a ghost takes the lead. Under it all, an old
record crackles.</p>
<nav class="ways" aria-label="Get LOF-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch, your computer's keyboard or a MIDI keyboard; the voices apart and MIDI to your DAW.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Experimental: at your own risk.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Every page, every button, every effect, on one page.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
</nav>
<h2>The band</h2>
<div class="grid">
  <div class="card"><h3>You: the lead</h3><p>Fourteen instruments on the keys: <b>Rhodes, Wurli, two uprights, vibes, kalimba, glock, strumstick, muted trumpet, tenor sax, flute, harmonica, bell, sine</b>. Single notes, or whole chords on each key.</p></div>
  <div class="card"><h3>Comp and bass</h3><p>The chord, re-voiced on every change and rolled like a tired pianist: held, stabs, offbeats or an arpeggio. The bass plays roots, root and fifth, walks, or goes dub.</p></div>
  <div class="card"><h3>Answer</h3><p>Call and response: when you rest after a phrase, a second instrument answers it, echoing yours or inventing its own.</p></div>
  <div class="card"><h3>Drums</h3><p>A sixteen-step sequencer, eight lanes, eight patterns and swing, on the <b>Dusty</b> kit (samples) or the <b>Tape</b> kit (synthesized). Drift changes the pattern for you every few bars.</p></div>
  <div class="card"><h3>Daydream</h3><p>Leave the keys alone and a ghost takes the lead with phrases of its own, as sparse or as busy as you set it. Any key takes it back.</p></div>
  <div class="card"><h3>Dust</h3><p>The record under it all: <b>vinyl</b> crackle and pops, <b>cassette</b> hiss, <b>reel-to-reel</b> haze or <b>radio</b> static, with hum, wow and flutter and a granular smear of the music itself.</p></div>
</div>
<h2>The pages</h2>
<figure><img src="img/lof-pages.png" width="2040" height="1112" alt="Eight LOF-1 screens: Groove, Lead, Comp, Bass, Answer, Drums, Dust and Global."></figure>
<p>One button a voice, pressed again for its second page; four knobs a page; ALGORITHM picks the page's instrument. Hold any button and a card says what it does.</p>
<h2>Four tapes</h2>
<figure><img src="img/lof-tapes.png" width="2040" height="590" alt="LOF-1 playing from vinyl, a cassette, reel to reel and an old radio."></figure>
<p>Ten presets, each a different session: a key, a tempo, a band and a tape. Store over any of them and it's yours.</p>
<h2>Playing it</h2>
<div class="pair">
  <figure><img src="img/lof-device-chords.png" width="1600" height="1161" alt="The FM-1 in Chords mode: each key plays a chord of the scale."><figcaption><b>Chords:</b> SEL + HOME and each key plays the scale's chord on that note, as rich as Jazz has it. The band follows.</figcaption></figure>
  <figure><img src="img/lof-device-fx.png" width="1600" height="1161" alt="The FM-1 in FX mode: a map of which key does what."><figcaption><b>FX:</b> every key an effect, with a map on screen: stutters, record skip, half time, tape stop, backspin, muffle, radio, mutes, solos, throws. Hold SAVE to lock them on.</figcaption></figure>
  <figure><img src="img/lof-device-seq.png" width="1600" height="1161" alt="The FM-1's drum sequencer."><figcaption><b>The sequencer:</b> hold SEQ and the white keys are the sixteen steps, OCT&minus; and OCT+ the lane.</figcaption></figure>
  <figure><img src="img/lof-device-saver.png" width="1600" height="1161" alt="The screensaver: a red panda leaning on a tree with a coffee, nodding."><figcaption><b>A minute untouched:</b> the screensaver, nodding along. Every screen comes out as if on an old TV, scanlines and all.</figcaption></figure>
</div>
<h2>Screens</h2>
<figure><img src="img/lof-moods.png" width="1536" height="1112" alt="Six LOF-1 screens: following you, chords on the keys, daydreaming, effects locked on, the sequencer, the screensaver."></figure>
<div class="shots" style="margin-top:12px">
  <figure><img src="img/lof-screen-splash.png" width="480" height="480" alt="The boot screen: a pixel red panda on the RedtailWorks name"><figcaption>Switching on</figcaption></figure>
  <figure><img src="img/lof-screen-splash-lof.png" width="480" height="480" alt="The second boot screen: LOF-1 in big pixel letters"><figcaption>&hellip;then LOF-1</figcaption></figure>
  <figure><img src="img/lof-screen-daydream.png" width="480" height="480" alt="The Daydream page"><figcaption>Daydream</figcaption></figure>
  <figure><img src="img/lof-screen-groove2.png" width="480" height="480" alt="Groove 2: Drift"><figcaption>Drift</figcaption></figure>
  <figure><img src="img/lof-screen-space.png" width="480" height="480" alt="The Space page"><figcaption>Space</figcaption></figure>
  <figure><img src="img/lof-screen-help.png" width="480" height="480" alt="REC's help card"><figcaption>Help cards</figcaption></figure>
</div>
<h2>To your DAW</h2>
<p>MIDI in: notes play the lead from any keyboard, and with Global &rsaquo; MIDI on Clock, a DAW's clock drives the tempo. MIDI out: the lead, comp, bass and answer on channels 1&ndash;4, the drums on 10. In the emulator, the six voices can come out on their own channels too.</p>
<h2>Credits</h2>
<p class="credits">LOF-1 is a RedtailWorks fork of <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal, by way of HYP-1 and ECO-1.
The platform (USB, update loader, storage, installer) is <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (@kurogedelic), Hügelton Instruments.
The sampled instruments are from Versilian Studios' CC0 libraries (VCSL, VSCO-2 Community Edition).
Screen font: Barlow Semi Condensed (SIL OFL). GPL-3.0.
M-VAVE and FM-1 are trademarks of their owners; LOF-1 is not affiliated with M-VAVE.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE. Installing firmware is at your own risk.</p>
<footer><a class="rw" href="https://www.redtail.works"><img class="panda" src="img/panda.png" width="48" height="38" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <span class="small">LOF-1 is a RedtailWorks project &middot; <a href="https://www.redtail.works">www.redtail.works</a> &middot; <a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a> &middot; <a href="__REPO__">source</a></span><span class="ver">version __VERSION__</span></footer>
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
    name = f"lof1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
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
    page = page.replace("__REPO__", REPO).replace("https://github.com/charlesvestal/fm1-amb", REPO).replace("github.com/charlesvestal/fm1-amb", REPO[8:])
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    man = out / "manual" / "index.html"                 # the build's own version, wherever the manual names one
    man.write_text(re.sub(r"LOF-1 version \d+\.\d+\.\d+", "LOF-1 version " + html.escape(version), man.read_text(encoding="utf-8")),
                   encoding="utf-8")
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build_clang.sh")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
