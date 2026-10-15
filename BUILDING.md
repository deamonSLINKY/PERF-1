# Building HYP-1

This covers HYP-1 version 1.0.21. Every push to `main` builds a release on GitHub (see
[Releases on GitHub](#releases-on-github)); building locally is only needed to work on the firmware.

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `amb.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `amb.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 1.0.21` makes a release build; the package is `build/amb-1.0.21.fwsc` (the site
publishes it as `hyp1-1.0.21.fwsc`).

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `EN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument (`tests/host/amb_test.c`:
Euclidean patterns, scales and chords, voice leading, the render's levels, drift, evolving loops, freeze); flash
storage; Felucca's update-path tests against `build/amb.fwsc`; and the whole app in the simulator
(`tests/scenarios/*.amb`), with its screenshots and audio in `build/scenarios/`.

`host/build_host.sh` builds the simulator alone; `build/host/amb_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/amb_host.c`).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/amb.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Releases on GitHub

Every push to `main` runs `.github/workflows/firmware.yml`:

1. It fetches JieLi's toolchain and the AC79 SDK (cached between runs, with retries).
2. It builds a release numbered from the workflow's run number N: `1.0.N` up to run 32, then `1.5.0`
   at run 33 and up from there (`1.5.(N - 33)`, a new minor version every hundred), so each push is a new
   version (the FM-1 reports it as FM-1_7XXYYZZ, which the installer checks).
3. It runs the tests, and fails the build on a size, pool or soft-double problem (shown as
   annotations on the run).
4. It builds the site with `tools/make_pages.py` (landing page, manual, PDF manual, browser emulator,
   web installer, the board, firmware download) and deploys it to GitHub Pages at
   <https://rustypawz.github.io/fm1-HYP1/>. The firmware is also kept as the run's artifact.

The board (`web/board.html`) shows the repository's GitHub Discussions with [giscus](https://giscus.app):
the workflow looks up the Discussions categories each build (`board/config.json`), so it switches itself
on once Discussions is enabled (Settings > General > Features) and the giscus app is installed on the
repository. Feature requests go in the **Ideas** category, comments in **General**; moderate them in
Discussions.

The manual's source is `docs/MANUAL.md` (`tools/manual_html.py` turns it into the page; the site
stamps the build's version into it). The PDF manual, `docs/HYP-1_Manual.pdf`, is built separately and
committed.

## The site's pictures and videos

All made from the firmware itself, run in the simulator:

```
host/build_host.sh
build/host/amb_host tools/site/shots.amb build/site_shots      # the screens
python3 tools/site/mockups.py                                  # docs/img: the device, screens, themes
python3 tools/site/panda.py                                    # the red panda mark and favicon
python3 tools/site/video.py [--vertical]                       # the tour; --seq, --voices, --links,
                                                               # --song for the other videos
```

`tools/publish_pages.sh` (pushing a site to a `gh-pages` branch by hand) is AMB-1's older route and
isn't needed with the workflow.
