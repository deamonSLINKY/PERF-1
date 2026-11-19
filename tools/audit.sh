#!/bin/sh
# The song-length piece audit (tests/host/gaudit.c): every factory piece for SECS seconds (180), one process a genre, into build/audit/audit.csv.
#   sh tools/audit.sh [secs]      (in WSL, from the repository's root)
set -e
SECS=${1:-180}
mkdir -p build/audit
gcc -O2 -ffp-contract=off -std=c99 -DEN_HOST -Ifirmware/src/dsp -o build/audit/gaudit tests/host/gaudit.c firmware/src/dsp/amb.c -lm
cd build/audit
for g in $(seq 0 21); do ./gaudit "$SECS" "$g" > "g$g.csv" & done
wait
head -1 g0.csv > audit.csv
for g in $(seq 0 21); do tail -n +2 "g$g.csv" >> audit.csv; done
echo "build/audit/audit.csv: $(($(wc -l < audit.csv) - 1)) pieces"
