#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""List (context, source) pairs present in our sources but without an active translation in zh.ts.

Usage: find_missing.py ours.ts zh.ts

A plural message counts only when every numerusform is filled, and a translation marked vanished does
not count even when its text is present: lrelease drops vanished messages, so the string falls back to
English while the catalog still reports zero untranslated.
"""

import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

ours_path, zh_path = sys.argv[1], sys.argv[2]


def entries(path):
    out = []
    root = ET.parse(path).getroot()
    for ctx in root.findall("context"):
        name = ctx.findtext("name") or ""
        for msg in ctx.findall("message"):
            source = msg.findtext("source") or ""
            numerus = msg.get("numerus") == "yes"
            tr = msg.find("translation")
            vanished = tr is not None and tr.get("type") == "vanished"
            if numerus:
                forms = [t.text or "" for t in msg.findall("translation/numerusform")]
                finished = bool(forms) and all(f.strip() for f in forms)
            else:
                finished = (msg.findtext("translation") or "").strip() != ""
            out.append((name, source, numerus, finished and not vanished))
    return out


zh = defaultdict(list)
for name, source, numerus, finished in entries(zh_path):
    zh[(name, source)].append(finished)

missing = []
for name, source, numerus, _ in entries(ours_path):
    if not any(zh.get((name, source), [])):
        missing.append((name, source, numerus))

print("total missing:", len(missing))
for name, source, numerus in missing:
    tag = " [numerus]" if numerus else ""
    print("%s\t%s%s" % (name, source.replace("\n", "\\n"), tag))
