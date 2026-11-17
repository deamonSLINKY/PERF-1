# The save formats, locked (0.17.61)

The promise for 1.x: a piece, a set list or a setting saved on 1.0 loads the same in every 1.x. Two checks hold it (both in `tests/run_tests.sh`):

- **`lock.txt`**: every piece setting's place, name, range and value names (a pluck sound, a kit, a mode), the effect settings' and the flash
  objects' order, the genres' order, the records' layout. `tools/format_lock.py check` fails when any of it moves, is renamed, removed or
  shrinks. New things at the end are fine: a setting, an effect setting, a value at the top of a list, an object, a genre, a field at the end
  of the set list or the settings record, a byte at the end of a piece's extras.
- **`v0.15.2/`, `v0.16.2/`, `v0.17.0/`, `v0.17.60/`**: the flash objects those releases saved (`make.amb`: pieces in three genres, the theme
  and the tune; 0.17.60 the effect settings too), and `state.txt`, what this build made of them. `tests/scenarios/fmt_*.amb` load them and
  compare (`fmtcheck`): each line must still be there, the same.

Room left (the lock check prints it): **no byte for a new piece setting in the record** (a genre's object is full: 3,836 of 3,840 bytes).
A new per-piece setting goes in the piece's extras (`PX_FXS` in project.c, 3 bytes spare; `PX_LEN` can grow, older objects are read
at their own length since 0.17.61), the way the bass answer did in 0.17.30.

**A change on purpose** (before 1.0 there may be some): convert what older builds saved (as `pluck_old` and `ans_old` in project.c do),
run `tools/format_lock.py update tests/format/lock.txt <manifest> firmware/src/dsp/amb.h firmware/src/app/plat.h`, and if a fixture's
loaded values change, look at what changed in `state.txt` before taking it (it is what players' pieces turn into). After 1.0: add at the
end only.

**A new release's fixture:** `git archive vX.Y.Z -o build/fmtx/vX.Y.Z.tar` (Windows), then in WSL `sh tools/format_fixture.sh
build/fmtx/vX.Y.Z.tar vX.Y.Z`.

Not covered here: the sample kits' flash (`web/fm1kit.js`, its own header), the pieces page's `.piece` / `.perfset` files (`tests/web_piece_test.js`).
