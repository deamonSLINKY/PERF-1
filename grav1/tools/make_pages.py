#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the GRAV-1 site: HYP-1's site has it under grav/ (the HYP-1 workflow runs this, then
tools/site/switcher.py adds the firmware dropdown to every page):

  index.html                  what GRAV-1 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        GRAV-1 in the browser (build/emu, from web/emu/build_clang.sh)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/grav1-VERSION.fwsc   the package the installer writes; also the download

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

REPO = os.environ.get("REPO_URL", "https://github.com/RustyPawz/fm1-HYP1/tree/main/grav1")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>GRAV-1</title>
<meta name="description" content="GRAV-1: a star and its planets play the M-VAVE FM-1. Orbits, gravity, drums and weather make the music. By RedtailWorks.">
<meta name="theme-color" content="#07090f">
<link rel="icon" type="image/png" href="img/favicon.png">
<meta property="og:title" content="GRAV-1: a star and its planets play the M-VAVE FM-1">
<meta property="og:image" content="img/grav-hero.png">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@400;500;600;700&display=swap">
<style>
/* deep space: a night-blue ground, the star's gold, ice for the far planets, and the red panda's rust and
 * cream for RedtailWorks (the same on every page: landing, installer, manual) */
:root { --ground: #07090f; --card: #121726; --card-2: #1b2134; --ink: #e4e7f0; --muted: #9aa1b8; --rule: #2c3450;
        --purple: #c9a24a; --purple-2: #f2c96b; --orange: #e8873a; --rust: #c85420; --cream: #f6ecde; --ice: #9cc3ff; color-scheme: dark; }
* { box-sizing: border-box; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 28px 64px;
       font: 400 18px/1.6 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 1040px; margin: 0 auto; }
h1, h2, h3, .ways strong, .tag { font-family: "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
header.site { display: flex; flex-wrap: wrap; align-items: baseline; gap: .4rem 1.4rem; margin: 0 0 1.2rem; }
header.site { align-items: center; }
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
h2::before { content: ""; width: 34px; height: 5px; background: linear-gradient(90deg, var(--orange) 0 62%, var(--purple) 62%); }
h3 { font-size: 1.1rem; font-weight: 600; letter-spacing: .03em; margin: 0 0 .3rem; color: var(--purple-2); }
.ways { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 12px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 10px; padding: 14px 16px; transition: border-color .15s, background .15s; }
.ways a:first-child { background: linear-gradient(135deg, #3a2f12, #121726); border-color: var(--purple); }
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
.video { margin: 0 0 1.8rem; }
.video .frame { position: relative; aspect-ratio: 16 / 9; border-radius: 12px; overflow: hidden; border: 1px solid var(--rule); background: #000; }
.video iframe { position: absolute; inset: 0; width: 100%; height: 100%; border: 0; }
.video figcaption { display: flex; flex-wrap: wrap; gap: .3rem 1rem; justify-content: space-between; }
.video figcaption a { color: var(--orange); }
footer .ver { font-family: "Barlow Semi Condensed", sans-serif; color: var(--muted); font-size: .9rem; }
a.kofi { display: inline-flex; align-items: center; gap: .4em; background: var(--rust, #c85420); color: #fff8f0 !important; text-decoration: none; border-radius: 999px; padding: .15em .8em; font-weight: 600; } a.kofi:hover { background: var(--orange, #e8873a); color: #120d03 !important; }
.support { display: flex; flex-wrap: wrap; align-items: center; gap: .8rem 1.4rem; background: var(--card); border: 1px solid var(--rust);
           border-radius: 10px; padding: 16px 18px; margin: 2.4rem 0 0; }
.support p { margin: 0; color: var(--muted); flex: 1 1 22rem; }
.support p b { color: var(--ink); font-weight: 600; }
.support a.kofi { font-size: 1.05rem; padding: .5em 1.2em; }
.exp { font: 700 .72rem "Barlow Semi Condensed", sans-serif; letter-spacing: .12em; text-transform: uppercase; color: #1a0f05;
       background: #e8873a; border-radius: 999px; padding: .15em .7em; margin-left: .2rem; }
.expnote { background: #2a1d10; border: 1px solid #e8873a; border-radius: 10px; padding: 12px 16px; margin: 0 0 1.4rem; color: #f3dcc4; }
.expnote b { color: #ffb070; }
@media (max-width: 560px) { .lede { font-size: 1.15rem; } h2 { font-size: 1.25rem; } }
</style>
</head>
<body>
<main>
<header class="site"><a class="home" href="./"><img class="panda" src="img/panda.png" width="64" height="50" alt="">GRAV-1<span class="exp">Experimental</span></a>
  <nav><a href="emu/">Emulator</a><a href="install/">Install</a><a href="manual/">Manual</a><a href="__REPO__">Source</a><a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a><a href="https://www.redtail.works">RedtailWorks</a></nav></header>
<div class="expnote"><b>Experimental.</b> GRAV-1 is new: built and tested in the simulator and in the emulator here, not yet on a real FM-1. Install it at your own risk; HYP-1, or M-VAVE's own firmware, goes back on the same way.</div>
<img class="hero" src="img/grav-hero.png" width="1600" height="1161" alt="An M-VAVE FM-1 running GRAV-1: a gold star with six planets on their orbits, the System page's Planets, Tempo, Wander and Phrase below it.">
<p class="lede"><b>GRAV-1</b> turns the M-VAVE FM-1 into a small solar system that plays slow, dark, spacey ambient.
A star in the middle hisses and roars in coloured noise. Up to six planets orbit it, each a voice that plays a short
phrase as it goes round, warm and dry near the star, cold and wet far out. A drum sequencer sets their years, larger
planets swell smaller ones as they pass, and dust clouds, asteroids and comets drift through.</p>
<nav class="ways" aria-label="Get GRAV-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; eight channels and MIDI to your DAW.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Experimental: at your own risk.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Every control, every page, every system, on one page.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
</nav>
<h2>The system</h2>
<div class="grid">
  <div class="card"><h3>The star</h3><p>A bed of noise in eight colours, drawn in its own colour: a <b>pink star</b> for pink noise, a <b>brown dwarf</b> for brown, a <b>blue giant</b>, a crackling <b>black hole</b>. It roars now and then, and flares, never above its level.</p></div>
  <div class="card"><h3>Planets</h3><p>One to six voices from sixteen: <b>Drone, Glass bell, Pluck, Soft keys, Choir, Laser, Ether, Ring hiss</b>, and darker ones: <b>Abyss, Dark choir, Cello, Gong, Toll, Whisper, Cold pad, Dusk organ</b>. Each plays its phrase round its orbit, kept to the scale, a note or two changing every year.</p></div>
  <div class="card"><h3>Pulse</h3><p>Six drum lanes (<b>Kick, Pulsar, Sonar, Geiger, Rim, Shaker</b>), one a planet. A lane's hits set its planet's year and, by Kepler, how close it orbits. A lane left empty leaves its planet dormant.</p></div>
  <div class="card"><h3>Gravity</h3><p>A bigger planet swells a smaller one as it passes, peaking at the closest approach. Dormant worlds wake only then. The outermost planet's year is the <b>Great Year</b>: the chord moves.</p></div>
  <div class="card"><h3>Weather</h3><p><b>Dust clouds</b> soften and warm the whole system, <b>asteroids</b> cross it pulling at the planets, and <b>comets</b> fall ringing into the star and burn up. Black keys 1&ndash;3 call them.</p></div>
  <div class="card"><h3>Space</h3><p>A dotted-eighth echo, Dattorro's plate, a radio of hiss and falling whistlers, and the stereo width the planets spread across.</p></div>
</div>
<h2>Ten systems</h2>
<figure><img src="img/grav-systems.png" width="2544" height="1112" alt="Ten GRAV-1 screens: Sol, Lumen, Rosette, Ember, Rigel, Veil, Cinder, Horizon, Kuiper and Nova, each its own star and planets."></figure>
<p>One for each of the star's colours and two more. Each keeps its own settings; SAVE on the Setup page keeps all ten.</p>
<div class="pair">
  <figure><img src="img/grav-device-pulse.png" width="1600" height="1161" alt="The FM-1 in the sequencer: six lanes of 16 steps, the white keys lit with the lane's steps."><figcaption><b>The sequencer:</b> hold SEQ for eight banks of six lanes. The white keys are the lane's steps, black keys or OCT pick the lane, SELECT the bank. More hits, a faster year, a closer orbit.</figcaption></figure>
  <figure><img src="img/grav-device-weather.png" width="1600" height="1161" alt="The FM-1 in a dust cloud, an asteroid crossing."><figcaption><b>Weather:</b> dust, asteroids and comets come by on their own, or when you call them.</figcaption></figure>
</div>
<h2>Themes</h2>
<figure><img src="img/grav-themes.png" width="2040" height="1112" alt="Seven GRAV-1 screens in its themes: Deep space, Nebula, Aurora, Red planet, Phosphor, Redtail, Mono."></figure>
<p>Seven screen themes on the Display page. The planets keep their warm-to-cold colours, molten near the star to ice far out, and the star its own.</p>
<h2>To your DAW</h2>
<div class="pair">
  <figure><img src="img/grav-device-clock.png" width="1600" height="1161" alt="The FM-1 on MIDI clock: EXT and the tempo in the header, the Setup page with Sync on Clock."><figcaption><b>MIDI clock in:</b> START plays from step one, STOP rests it, and the steps and the orbits follow the clock.</figcaption></figure>
  <figure><img src="img/grav-device-midi.png" width="1600" height="1161" alt="The FM-1 on the MIDI out page."><figcaption><b>MIDI out:</b> each planet's notes on its own channel, the drums on another, and the system as CCs: each planet's tide, the roar, the dust, the Great Year, the flares.</figcaption></figure>
</div>
<h2>Screens</h2>
<div class="shots">
  <figure><img src="img/grav-screen-splash.png" width="480" height="480" alt="The boot screen: a pixel red panda on the RedtailWorks name"><figcaption>Switching on</figcaption></figure>
  <figure><img src="img/grav-screen-splash-grav.png" width="480" height="480" alt="The second boot screen: GRAV-1 in big pixel letters"><figcaption>&hellip;then GRAV-1</figcaption></figure>
  <figure><img src="img/grav-screen-system.png" width="480" height="480" alt="The System page"><figcaption>System</figcaption></figure>
  <figure><img src="img/grav-screen-star.png" width="480" height="480" alt="The Star page"><figcaption>Star</figcaption></figure>
  <figure><img src="img/grav-screen-planet.png" width="480" height="480" alt="A planet's page, its orbit picked out"><figcaption>A planet</figcaption></figure>
  <figure><img src="img/grav-screen-gravity.png" width="480" height="480" alt="The Gravity page"><figcaption>Gravity</figcaption></figure>
  <figure><img src="img/grav-screen-pulse.png" width="480" height="480" alt="The Pulse page"><figcaption>Pulse</figcaption></figure>
  <figure><img src="img/grav-screen-help.png" width="480" height="480" alt="REC's help card"><figcaption>Help cards</figcaption></figure>
</div>
<h2>Credits</h2>
<p class="credits">GRAV-1 is a fork of <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal, by way of HYP-1 and ECO-1.
The platform (USB, update loader, storage, installer) is <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (@kurogedelic), Hügelton Instruments.
Screen font: Barlow Semi Condensed (SIL OFL). GPL-3.0.
M-VAVE and FM-1 are trademarks of their owners; GRAV-1 is not affiliated with M-VAVE.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE. Installing firmware is at your own risk.</p>
<footer><a class="rw" href="https://www.redtail.works"><img class="panda" src="img/panda.png" width="48" height="38" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <span class="small">GRAV-1 is a RedtailWorks project &middot; <a href="https://www.redtail.works">www.redtail.works</a> &middot; <a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a> &middot; <a href="__REPO__">source</a></span><span class="ver">version __VERSION__</span></footer>
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
    name = f"grav1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
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
    man.write_text(re.sub(r"GRAV-1 version \d+\.\d+\.\d+", "GRAV-1 version " + html.escape(version), man.read_text(encoding="utf-8")),
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
