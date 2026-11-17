#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The save formats' lock (0.17.61, for 1.x: a piece, a set list or a setting saved on 1.0 loads the same in every 1.x).

The host writes a manifest (scenario command `fmtlock FILE`): the stored records' layout, every piece setting's place, name, range and value
names, the piece extras, the genres. This script adds the effect settings' and the flash objects' order (from amb.h and plat.h) and compares
the whole with the lock kept in tests/format/lock.txt:

  check LOCK MANIFEST AMB_H PLAT_H     every line of the lock must still hold (exit 1 if not). Allowed: new lines (a setting, an effect setting,
                                       an object, a genre added at the end), a setting's range growing at the top (new values after the old
                                       ones), a record's size growing (".size"). Not allowed: anything moved, renamed, removed or renumbered.
  update LOCK MANIFEST AMB_H PLAT_H    write the lock anew (only for a change made on purpose, with a converter for what was saved before).

A failure says what moved. Before 1.0 a last-minute change can still move things: update the lock, and say in the CHANGELOG what an older
save turns into. After 1.0: add at the end only.
"""
import re
import sys


def enum_names(text, first, last):
    """the identifiers of the enum that starts with `first`, in order, up to `last` (comments out)"""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    a = text.index(first)
    b = text.index(last, a)
    body = text[a:b]
    names = []
    for item in body.split(','):
        item = item.strip()
        if not item:
            continue
        name = item.split('=')[0].strip()
        if '=' in item:
            raise SystemExit('format_lock: %s has a value set (%s): count it here first' % (name, item))
        names.append(name)
    return names


def manifest(path, amb_h, plat_h):
    out = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if not line or line.startswith('#'):
                continue
            k, _, v = line.partition(' ')
            out[k] = v
    with open(amb_h, encoding='utf-8') as f:
        a = f.read()
    with open(plat_h, encoding='utf-8') as f:
        p = f.read()
    for i, n in enumerate(enum_names(a, 'EN_FXS_GX_LEN', 'EN_NFXS')):
        out['fxs%d' % i] = n
    for i, n in enumerate(enum_names(p, 'OBJ_PROJ', 'OBJ_NOBJ')):
        out['obj%d' % i] = n
    return out


def compare(lock, new):
    bad = []
    for k, v in lock.items():
        if k.startswith('room.'):
            continue
        if k not in new:
            bad.append('%s: gone (was %s)' % (k, v))
            continue
        w = new[k]
        if v == w:
            continue
        if k.endswith('.size'):
            if int(w) >= int(v):
                continue
            bad.append('%s: smaller (%s, was %s)' % (k, w, v))
            continue
        if re.fullmatch(r'par\d+', k):
            o = v.split('|')
            n = w.split('|')
            why = []
            if o[0] != n[0]:
                why.append('name %s, was %s' % (n[0], o[0]))
            if o[1] != n[1]:
                why.append('lowest %s, was %s (every saved value would shift)' % (n[1], o[1]))
            if int(n[2]) < int(o[2]):
                why.append('highest %s, was %s (saved values above it would be clamped)' % (n[2], o[2]))
            ot, nt = o[4:], n[4:]
            if ot and nt[:len(ot)] != ot:
                at = next(i for i in range(len(ot)) if i >= len(nt) or nt[i] != ot[i])
                why.append('value %d is now "%s", was "%s"' % (int(o[1]) + at, nt[at] if at < len(nt) else '-', ot[at]))
            if why:
                bad.append('%s (%s): %s' % (k, o[0], '; '.join(why)))
            continue
        bad.append('%s: %s, was %s' % (k, w, v))
    return bad


def main(argv):
    if len(argv) != 6 or argv[1] not in ('check', 'update'):
        print(__doc__)
        return 2
    lock_path, man = argv[2], manifest(argv[3], argv[4], argv[5])
    if argv[1] == 'update':
        with open(lock_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# The save formats\' lock (tools/format_lock.py). Lines may be added; none may change. Made by "update".\n')
            for k in sorted(man, key=lambda s: (re.sub(r'\d+$', '', s), int(re.search(r'\d*$', s).group() or -1))):
                f.write('%s %s\n' % (k, man[k]))
        print('format lock: %d lines written to %s' % (len(man), lock_path))
        return 0
    lock = {}
    with open(lock_path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line and not line.startswith('#'):
                k, _, v = line.partition(' ')
                lock[k] = v
    bad = compare(lock, man)
    room = ', '.join('%s %s' % (k[5:], man[k]) for k in sorted(man) if k.startswith('room.'))
    added = sum(1 for k in man if k not in lock)
    if bad:
        print('format lock: %d changes that would break saved pieces or settings:' % len(bad))
        for b in bad:
            print('  ' + b)
        print('If it is on purpose: convert what older builds saved, then tools/format_lock.py update (see its notes).')
        return 1
    print('format lock: ok (%d lines held, %d new; room: %s)' % (len(lock), added, room))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
