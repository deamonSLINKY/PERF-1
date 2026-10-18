#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The site's front page: all four RedtailWorks firmwares for the FM-1 in one place (the workflow runs this
after every firmware's pages are built and before the dropdown, tools/site/switcher.py).

HYP-1's landing moves from the root to hyp/ (its emulator, installer, manual and board stay where they are,
so no link or bookmark to them breaks), and its pages' "home" links follow it there. The root becomes the
front page: the four firmwares in one picture (docs/img/fw-scene.png, from tools/site/scene.py), the pack's
video, and a card for each with its ways in. A firmware that didn't build this time is left off, as on
HYP-1's landing.

  python3 tools/site/hub.py build/pages"""
import os
import shutil
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[2]
VIDEO = "XxvSo3m2nZs"
VIDEO_START = 55
HYP_SECTIONS = ("emu", "install", "manual", "board")

KOFI_SVG = ('<svg aria-hidden="true" viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M3 6h14v6a6 6 0 0 1-6 '
            '6H9a6 6 0 0 1-6-6V6Zm14 1h1.5a3.5 3.5 0 0 1 0 7H17v-2h1.5a1.5 1.5 0 0 0 0-3H17V7ZM2 20h16v2H2v-2Zm8.2-11.3c-.9-.9-2.4-.3-2.4 '
            '1 0 1.5 2.4 3.2 2.4 3.2s2.4-1.7 2.4-3.2c0-1.3-1.5-1.9-2.4-1Z"/></svg>')

# key, name, colour, status, device render (under the site), screen alt, what it is, how it plays
FIRMWARES = [
    ("hyp", "HYP-1", "#c18cff", "Stable", "img/hyp-device.png",
     "the FM-1 running HYP-1: an Am7 chord, the drum pattern, sixteen keys glowing purple",
     "Hypnotic techno and acid",
     "A breathing pad, a deep drone, a generative 303 line and a bed of grain over drums you sequence. "
     "Shape the voices like a small modular, break it down and build it back up."),
    ("lof", "LOF-1", "#ff9a6e", "Experimental", "lof/img/lof-device.png",
     "the FM-1 running LOF-1: a record turning, a Cmaj7 chord, the keys glowing peach",
     "A lofi groovebox",
     "You play the lead and a tired jazz band follows: comp, bass and drums move with your chords, an Answer "
     "voice replies, and a record, cassette, reel or radio sits under it all."),
    ("eco", "ECO-1", "#a6e68a", "Experimental", "eco/img/eco-device.png",
     "the FM-1 running ECO-1: a fern on a forest floor, the keys glowing green",
     "A plant plays it",
     "Three leaves fire notes as light and water charge them, a stem drones beneath, and rain, wind, gusts and "
     "a day and night play the plant. Eight plants, each in its own place."),
    ("grav", "GRAV-1", "#f8d990", "Experimental", "grav/img/grav-device.png",
     "the FM-1 running GRAV-1: a gold star and its planets on their orbits, the keys glowing gold",
     "A star and its planets",
     "Slow, dark, spacey ambient: a star of coloured noise and up to six planets, each a voice playing round "
     "its orbit. Bigger planets swell smaller ones; dust, asteroids and comets drift through."),
]

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FM-1 firmware by RedtailWorks</title>
<meta name="description" content="Four free, open-source generative firmwares for the M-VAVE FM-1: HYP-1, LOF-1, ECO-1 and GRAV-1. By RedtailWorks.">
<meta name="theme-color" content="#141318">
<link rel="icon" type="image/png" href="img/favicon.png">
<meta property="og:title" content="Four firmwares for the M-VAVE FM-1, by RedtailWorks">
<meta property="og:image" content="img/fw-scene.png">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600;700&family=Barlow+Semi+Condensed:wght@400;500;600&display=swap">
<style>
:root { --ground: #141318; --card: #1f1c27; --card-2: #2a2534; --ink: #f1ecf4; --muted: #aaa1b8; --rule: #3e3a4c;
        --purple: #a44cff; --purple-2: #c18cff; --orange: #ff8020; --rust: #c85420; --cream: #f6ecde; color-scheme: dark; }
* { box-sizing: border-box; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 28px 64px;
       font: 400 18px/1.6 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 1040px; margin: 0 auto; }
h1, h2, h3, .go strong { font-family: "Chakra Petch", "Helvetica Neue", Arial, sans-serif; }
a { color: var(--ink); text-decoration-color: var(--purple); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { color: var(--purple-2); }
header.site { display: flex; flex-wrap: wrap; align-items: center; gap: .4rem 1.4rem; margin: 0 0 1.4rem; }
header.site .home { font: 700 1.3rem "Chakra Petch", sans-serif; letter-spacing: .08em; text-decoration: none;
                    display: inline-flex; align-items: center; gap: .6rem; }
header.site .home span:nth-of-type(1) { color: var(--purple-2); } header.site .home span:nth-of-type(2) { color: var(--orange); }
header.site nav { margin-left: auto; display: flex; flex-wrap: wrap; gap: .3rem 1.1rem; font-size: 1rem; }
header.site nav a.yt, footer a.yt { color: var(--orange); }
img.panda { image-rendering: pixelated; display: block; height: auto; }
a.kofi { display: inline-flex; align-items: center; gap: .4em; background: var(--rust); color: #fff8f0 !important; text-decoration: none;
         border-radius: 999px; padding: .15em .8em; font-weight: 600; }
a.kofi:hover { background: var(--orange); color: #1a0c03 !important; }
.intro { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1.05fr); gap: 28px; align-items: center; margin: 0 0 2.2rem; }
.intro img { width: 100%; height: auto; display: block; border-radius: 14px; border: 1px solid var(--rule); }
h1 { font-size: 2.3rem; line-height: 1.15; letter-spacing: .03em; margin: 0 0 .8rem; }
h1 em { font-style: normal; color: var(--orange); }
.lede { font-size: 1.22rem; line-height: 1.5; margin: 0 0 1.1rem; color: var(--ink); }
.lede + p { color: var(--muted); margin: 0 0 1.2rem; }
.pick { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem .8rem; }
.pick label { font: 600 .95rem "Chakra Petch", sans-serif; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
.pick select { font: 600 1.05rem "Barlow Semi Condensed", sans-serif; color: var(--ink); background: var(--card-2);
               border: 1px solid var(--purple); border-radius: 8px; padding: .45em .7em; min-height: 44px; max-width: 100%; }
.pick select:focus-visible { outline: 2px solid var(--orange); outline-offset: 2px; }
h2 { font-size: 1.45rem; font-weight: 700; letter-spacing: .04em; text-transform: uppercase; margin: 2.8rem 0 1rem;
     display: flex; align-items: center; gap: .7rem; }
h2::before { content: ""; width: 34px; height: 5px; background: linear-gradient(90deg, var(--orange) 0 62%, var(--purple) 62%); }
.video .frame { position: relative; aspect-ratio: 16 / 9; border-radius: 12px; overflow: hidden; border: 1px solid var(--rule); background: #000; }
.video iframe { position: absolute; inset: 0; width: 100%; height: 100%; border: 0; }
.video figcaption { display: flex; flex-wrap: wrap; gap: .3rem 1rem; justify-content: space-between; color: var(--muted); font-size: .95rem; margin-top: .4rem; }
.video figcaption a { color: var(--orange); }
figure { margin: 0; }
.fws { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
.fw { background: var(--card); border: 1px solid var(--rule); border-top: 4px solid var(--c); border-radius: 12px;
      padding: 14px 16px 16px; display: flex; flex-direction: column; gap: .5rem; }
.fw > a img { width: 100%; height: auto; display: block; border-radius: 8px; }
.fw h3 { margin: .3rem 0 0; font-size: 1.5rem; letter-spacing: .05em; color: var(--c); display: flex; align-items: center; gap: .6rem; flex-wrap: wrap; }
.fw h3 a { color: inherit; text-decoration: none; }
.fw .status { font: 700 .72rem "Chakra Petch", sans-serif; letter-spacing: .1em; text-transform: uppercase; border-radius: 999px;
              padding: .15em .7em; border: 1px solid var(--c); color: var(--c); }
.fw .what { font: 600 1.08rem "Barlow Semi Condensed", sans-serif; margin: 0; }
.fw p { margin: 0; color: var(--muted); }
.go { display: flex; flex-wrap: wrap; gap: 8px; margin-top: auto; padding-top: .5rem; }
.go a { display: inline-flex; align-items: center; min-height: 44px; padding: .3em 1em; border-radius: 8px; text-decoration: none;
        background: var(--card-2); border: 1px solid var(--rule); font-weight: 600; }
.go a:first-child { border-color: var(--c); }
.go a:hover, .go a:focus-visible { border-color: var(--c); color: var(--c); outline: none; }
.support { display: flex; flex-wrap: wrap; align-items: center; gap: .8rem 1.4rem; background: var(--card); border: 1px solid var(--rust);
           border-radius: 10px; padding: 16px 18px; margin: 2.6rem 0 0; }
.support p { margin: 0; color: var(--muted); flex: 1 1 22rem; }
.support p b { color: var(--ink); font-weight: 600; }
.support a.kofi { font-size: 1.05rem; padding: .5em 1.2em; }
p.small { color: var(--muted); font-size: .95rem; }
a.open1 { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 20px; align-items: center; text-decoration: none;
          background: var(--card); border: 1px solid var(--rule); border-left: 4px solid var(--orange); border-radius: 12px; padding: 14px 18px 14px 14px; }
a.open1:hover, a.open1:focus-visible { border-color: var(--orange); color: var(--ink); outline: none; }
a.open1 img { width: 100%; height: auto; display: block; border-radius: 8px; }
a.open1 span { color: var(--muted); }
a.open1 b { font: 700 1.6rem "Chakra Petch", sans-serif; color: var(--orange); letter-spacing: .04em; }
a.open1 em { font: 700 .72rem "Chakra Petch", sans-serif; font-style: normal; letter-spacing: .1em; text-transform: uppercase; color: var(--orange);
             border: 1px solid var(--orange); border-radius: 999px; padding: .15em .7em; vertical-align: middle; margin-left: .4rem; }
a.open1 u { color: var(--ink); text-decoration-color: var(--orange); text-decoration-thickness: 2px; text-underline-offset: 3px; font-weight: 600; }
footer { margin-top: 3rem; padding-top: 1.2rem; border-top: 1px solid var(--rule); display: flex; flex-wrap: wrap; gap: .6rem 1.6rem; align-items: center; }
footer .rw { font: 700 1.05rem "Chakra Petch", sans-serif; letter-spacing: .08em; text-decoration: none; display: inline-flex; align-items: center; gap: .55rem; }
footer .rw span:nth-of-type(1) { color: var(--purple-2); } footer .rw span:nth-of-type(2) { color: var(--orange); }
@media (max-width: 760px) { .intro, .fws, a.open1 { grid-template-columns: 1fr; } h1 { font-size: 1.8rem; } }
</style>
</head>
<body>
<!--rw-hub-->
<main>
<header class="site"><a class="home" href="./"><img class="panda" src="img/panda.png" width="64" height="50" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <nav><a href="open1/">OPEN-1</a><a href="__REPO__">Source</a><a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a><a href="https://www.redtail.works">RedtailWorks</a><a class="kofi" href="https://ko-fi.com/redtailworks">__KOFI__Support</a></nav></header>

<section class="intro">
  <img src="img/fw-scene.png" width="2000" height="2000" alt="Four M-VAVE FM-1s on a dark desk, each running a RedtailWorks firmware: HYP-1 with purple keys, LOF-1 with peach keys and a turning record, ECO-1 with green keys and a fern, GRAV-1 with gold keys and a star and its planets.">
  <div>
    <h1>Four firmwares for the <em>M&#8209;VAVE FM&#8209;1</em></h1>
    <p class="lede">Generative music machines that turn the FM-1 into something new each time you install one: hypnotic techno, a lofi band, a plant, a solar system.</p>
    <p>All free and open source. Try any of them in your browser first, then install from Chrome or Edge with the FM-1 plugged in; the installer can always put M-VAVE's own firmware back.</p>
    <div class="pick"><label for="fw-pick">Go to</label>
      <select id="fw-pick" onchange="if (this.value) location.href = this.value">
        <option value="">Choose a firmware…</option>
__OPTIONS__
      </select></div>
  </div>
</section>

<h2>Watch the pack</h2>
<figure class="video">
  <div class="frame"><iframe src="https://www.youtube-nocookie.com/embed/__VIDEO__?start=__START__&amp;rel=0" title="M-VAVE FM-1 Ambient Firmware Pack, on the RedtailWorks YouTube channel" loading="lazy"
    allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe></div>
  <figcaption><span>The firmware pack, played on the FM-1.</span><a href="https://www.youtube.com/watch?v=__VIDEO__&amp;t=__START__s">Watch on YouTube</a></figcaption>
</figure>

<h2>The firmwares</h2>
<div class="fws">
__CARDS__
</div>

<h2>What's next: OPEN-1</h2>
<a class="open1" href="open1/"><img src="open1/media/open1-hyp1.jpg" width="1260" height="600" alt="Concept render of OPEN-1, a 37-key synth, running HYP-1" loading="lazy">
  <span><b>OPEN-1</b> <em>Ideas phase</em><br>An idea for an open-firmware synth under $200: 37 keys, pads, audio in and out, and a microSD library of firmware. Goals, target specs, the pitch video and the build log. <u>Follow along</u></span></a>

<div class="support">
  <p><b>All four are free.</b> If they're useful to you, a coffee keeps new firmware coming, and the videos too.</p>
  <a class="kofi" href="https://ko-fi.com/redtailworks">__KOFI__Support on Ko-fi</a>
</div>
<p class="small">The experimental firmwares are built and tested in the simulator and the browser emulator, not yet on every FM-1; they're numbered 0.x until they have been. HYP-1 grew from <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal. Installing is at your own risk.</p>

<footer><a class="rw" href="https://www.redtail.works"><img class="panda" src="img/panda.png" width="48" height="38" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <span class="small">FM-1 firmware by RedtailWorks &middot; <a href="https://www.redtail.works">www.redtail.works</a> &middot; <a class="yt" href="https://www.youtube.com/@RedtailWorks">YouTube</a> &middot; <a href="https://ko-fi.com/redtailworks">Ko-fi</a> &middot; <a href="__REPO__">source on GitHub</a></span></footer>
</main>
</body>
</html>
"""


def page_of(key):
    return "hyp/" if key == "hyp" else f"{key}/"


def section_of(key, sec):
    return f"{sec}/" if key == "hyp" else f"{key}/{sec}/"


def main(root):
    root = Path(root)
    landing = root / "index.html"
    hyp = root / "hyp" / "index.html"
    if landing.exists() and "<!--rw-hub-->" not in landing.read_text(encoding="utf-8"):
        t = landing.read_text(encoding="utf-8")
        t = t.replace("<head>", '<head>\n<base href="../">', 1)            # its links stay as they were, from one level down
        t = t.replace('<a class="home" href="./">', '<a class="home" href="hyp/">', 1)
        hyp.parent.mkdir(parents=True, exist_ok=True)
        hyp.write_text(t, encoding="utf-8")
    for sec in HYP_SECTIONS:                                               # HYP-1's pages: "home" is HYP-1's landing
        f = root / sec / "index.html"
        if f.exists():
            f.write_text(f.read_text(encoding="utf-8").replace('href="../"', 'href="../hyp/"'), encoding="utf-8")
    shutil.copy(SRC / "docs" / "img" / "fw-scene.png", root / "img" / "fw-scene.png")

    built = [fw for fw in FIRMWARES if fw[0] == "hyp" or (root / fw[0] / "index.html").exists()]
    options, cards = [], []
    for key, name, col, status, img, alt, what, about in built:
        options.append(f'        <option value="{page_of(key)}">{name} · {what.lower()}</option>')
        ways = [f'<a href="{page_of(key)}">About {name}</a>']
        for sec, label in (("emu", "Try in browser"), ("install", "Install")):
            if (root / section_of(key, sec)).exists():
                ways.append(f'<a href="{section_of(key, sec)}">{label}</a>')
        cards.append(f'<!--{key}--><article class="fw" style="--c:{col}">'
                     f'<a href="{page_of(key)}"><img src="{img}" width="1600" height="1071" alt="{alt}" loading="lazy"></a>'
                     f'<h3><a href="{page_of(key)}">{name}</a><span class="status">{status}</span></h3>'
                     f'<p class="what">{what}</p><p>{about}</p><div class="go">{"".join(ways)}</div></article><!--/{key}-->')

    repo = os.environ.get("REPO_URL", "https://github.com/RustyPawz/fm1-HYP1")
    page = (PAGE.replace("__OPTIONS__", "\n".join(options)).replace("__CARDS__", "\n".join(cards))
            .replace("__VIDEO__", VIDEO).replace("__START__", str(VIDEO_START)).replace("__REPO__", repo)
            .replace("__KOFI__", KOFI_SVG))
    landing.write_text(page, encoding="utf-8")
    print(f"hub: index.html ({', '.join(fw[1] for fw in built)}), HYP-1's landing now at hyp/")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
