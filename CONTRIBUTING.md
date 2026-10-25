# Contributing to PERF-1

Everyone is welcome to help: ideas and bug reports as much as code.

- **Ideas, comments and bug reports:** [open an issue](https://github.com/deamonSLINKY/PERF-1/issues/new/choose). For a bug, say the version (the boot card), the genre and piece, and what you did; for crackles or dropouts, attach a status log (`tools/fm1log.py`).
- **Code, documentation, sounds:** fork the repository and open a pull request. Small, focused changes are easiest to review. Say what you changed and how you checked it.
- **Building and testing:** `BUILDING.md`. `tests/run_tests.sh` runs everything (about 70 scripted scenarios of the whole app in a simulator, plus tests of the maths, the instrument, the flash code and the editor); please keep it passing. A change to the screen is checked with the screenshots the scenarios write (`tools/make_screenshots.py` refreshes the manual's).
- **Real-time rules:** the audio render has an 11.6 ms budget per buffer on a 240 MHz core with a single-precision FPU, so no `double`, no per-sample divides you can hoist, and nothing that allocates or waits in the audio path. The build fails on a soft-double routine.
- **Licence:** PERF-1 is GPL-3.0-only. By sending a change you agree that it is released under that licence. Keep the copyright headers in the source files (`LICENSING.md` lists where each part comes from).
