#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Add a piece or a set to the site's library (web/library/library.json): its code, or a link with the code after #p=, and today's date.

    python -I tools/library_add.py "https://deamonslinky.github.io/PERF-1/pieces/#p=P1.xxxx"      (or the code itself)
    python -I tools/library_add.py --remove P1.xxxx                                               (take one out again)

The page reads the title, maker and note from the code itself; the newest are shown first. Publish the site afterwards for it to show."""
import datetime
import json
import os
import re
import sys

LIB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web", "library", "library.json")


def main(argv):
    remove = "--remove" in argv
    args = [a for a in argv if a != "--remove"]
    if not args:
        sys.exit(__doc__)
    m = re.search(r"P1\.[A-Za-z0-9_-]+", args[0])
    if not m:
        sys.exit("no piece code (P1.…) in that")
    code = m.group(0)
    lib = json.load(open(LIB, encoding="utf-8"))
    have = [e for e in lib if e.get("code") == code]
    if remove:
        lib = [e for e in lib if e.get("code") != code]
        print("removed" if have else "it was not in the library")
    elif have:
        print("already in the library (added %s)" % have[0].get("added"))
    else:
        lib.append({"code": code, "added": datetime.date.today().isoformat()})
        print("added: %d in the library" % len(lib))
    with open(LIB, "w", encoding="utf-8", newline="\n") as f:
        json.dump(lib, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main(sys.argv[1:])
