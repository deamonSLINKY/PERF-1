#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The save formats' fixtures (0.17.61): what a release saved, kept so every later build is checked against it.
#   tools/format_fixture.sh REF NAME     (from the repo root, in WSL)
# builds the host simulator of REF (a tag: v0.17.0) in a temporary copy, runs tests/format/make.amb there (pieces saved in three genres,
# the player's three), keeps the flash objects in tests/format/NAME/, then has THIS build load them and keeps its dump as
# tests/format/NAME/state.txt, and writes tests/scenarios/fmt_NAME.amb, which checks every build against that dump.
# Look at the dump before committing it: it is what a player's pieces from that release turn into, from now on.
set -e
ref=$1
name=$2
[ -n "$ref" ] && [ -n "$name" ] || { echo "usage: tools/format_fixture.sh REF NAME"; exit 2; }
root=$(pwd)
tmp=$(mktemp -d /tmp/fmtfix.XXXXXX)
if [ -f "$ref" ]; then
    tar -x -C "$tmp" -f "$ref"                   # a tar of the release (git archive v0.17.0 -o x.tar: WSL's git can't read a worktree made on Windows)
else
    git archive "$ref" | tar -x -C "$tmp"
fi
cd "$tmp"
if grep -q '"fxpageset"' host/amb_host.c; then
    # (0.17.19 on: the settings pages as tests/format/make_set.amb walks them) the effect settings too, before the objects are written
    sed -n '1,/^dumpobj/p' "$root/tests/format/make.amb" | sed '$d' > make.amb
    cat "$root/tests/format/make_set.amb" >> make.amb
    sed -n '/^dumpobj/,$p' "$root/tests/format/make.amb" >> make.amb
else
    cp "$root/tests/format/make.amb" make.amb
fi
if ! grep -q '"dumpobj"' host/amb_host.c; then
    python3 "$root/tools/format_fixture_patch.py" host/amb_host.c   # (0.15.x has no dumpobj: the later releases' few lines, put in)
fi
sh host/build_host.sh >/dev/null
mkdir -p build/fmtfix
build/host/amb_host make.amb build/fmtout > "$tmp/make.log" || { cat "$tmp/make.log"; exit 1; }
mkdir -p "$root/tests/format/$name"
rm -f "$root/tests/format/$name"/o*.bin
for f in build/fmtfix/o*.bin; do
    [ -s "$f" ] && cp "$f" "$root/tests/format/$name/"
done
cd "$root"
rm -rf "$tmp"
s=tests/scenarios/fmt_$name.amb
{
    echo "# The save formats (tools/format_fixture.sh): what release $name saved (tests/format/$name/) loads in this build as it did when the fixture"
    echo "# was made (tests/format/$name/state.txt; a line this build adds is fine, a line that changed is not)."
    echo "seed 0"
    echo "wait 500"
    for f in tests/format/$name/o*.bin; do
        o=$(basename "$f" .bin | tr -d o)
        echo "loadobj $o $f"
    done
    echo "reboot"
    echo "wait 3000"
} > "$s"
cp "$s" /tmp/fmtdump.amb
echo "fmtdump tests/format/$name/state.txt" >> /tmp/fmtdump.amb
build/host/amb_host /tmp/fmtdump.amb build/fmtout >/dev/null
echo "fmtcheck tests/format/$name/state.txt" >> "$s"
echo "format fixture $name: $(ls tests/format/$name/o*.bin | wc -l) objects, $(wc -l < tests/format/$name/state.txt) lines in state.txt, $s"
