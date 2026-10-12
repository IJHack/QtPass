#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 IJHack
# SPDX-License-Identifier: GPL-3.0-or-later
"""Structural audit of QtPass Qt Linguist .ts files.

Default checks (exit status 1 if any fire):
  PH    placeholder mismatch: %1, %2 ... per form, %n presence, "% 1" spacing
  MN    source has a mnemonic (&X) but the translation has none
  HTML  per-tag open/close counts differ between source and translation
  MIX   Latin letters glued to a word in the locale's own script
  ALPHA password generator character set lost a source character or
        gained a separator (appending local letters is fine)

Optional, report-only:
  --verbatim        translation identical to source (possible MT fallback)
  --leak w1,w2,...  translation contains a word from another language
  --skip l1,l2,...  leave these locales out (the language the --leak words
                    belong to, and its close relatives)

Run from the repository root:
  python3 .opencode/skills/qtpass-localization-audit/scripts/audit_ts.py [options] [FILE ...]
With no FILE, every localization/localization_*.ts is audited.
"""

import argparse
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter

# Native script per locale (full locale name first, then language code).
# Latin-script locales are absent, so MIX is skipped for them.
SCRIPT = {
    "ar": "؀-ۿ", "fa": "؀-ۿ", "ur": "؀-ۿ",
    "he": "֐-׿",
    "bg": "Ѐ-ӿ", "ru": "Ѐ-ӿ", "uk": "Ѐ-ӿ",
    "sr_Cyrl": "Ѐ-ӿ",
    "el": "Ͱ-Ͽ",
    "hi": "ऀ-ॿ", "mr": "ऀ-ॿ",
    "bn": "ঀ-৿", "pa": "਀-੿", "ta": "஀-௿",
    "te": "ఀ-౿", "si": "඀-෿", "th": "฀-๿",
    "ko": "가-힯",
    "ja": "぀-ヿ一-鿿", "zh": "一-鿿",
}

# Source strings that are deliberately identical in most locales.
VERBATIM_OK = {
    "QtPass", "GPG", "Git", "OTP", "PWGen", "pass", "Pass", "Email", "%1",
    "%2", "…", "⌕", "Aa", "LTR", "Ctrl+G", "Ctrl+N", "PGP", "GnuPG",
    "YubiKey", "Push", "Git push", "Git pull", "gpg", "git", "Git:",
    "qrencode", "pwgen",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "login\nURL\ne-mail",
}

ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
PH_RE = re.compile(r"%\d+")
N_RE = re.compile(r"%n(?![A-Za-z])")
SPACED_RE = re.compile(r"%\s+(?:\d|n(?![A-Za-z]))")
# &X where X is not a space, a second & (literal ampersand) or the start
# of an HTML entity such as &gt; or &middot;.
MNEMONIC_RE = re.compile(r"&(?![\s&]|[A-Za-z]+;|#\d+;)")
TAG_RE = re.compile(r"<(/?)([A-Za-z][A-Za-z0-9]*)")


def tags(text):
    return Counter((m.group(1), m.group(2).lower()) for m in TAG_RE.finditer(text))


def check(src, forms, script):
    """Yield (code, detail) for each structural problem."""
    # %1, %2 ... must match in every form; %n must appear in at least one
    # form when the source has it (a singular form may spell the number out).
    plural = N_RE.search(src) is not None
    if plural != any(N_RE.search(t) for t in forms):
        yield "PH", forms[0]
    else:
        for t in forms:
            if sorted(PH_RE.findall(src)) != sorted(PH_RE.findall(t)) or SPACED_RE.search(t):
                yield "PH", t
                break
    for t in forms:
        if MNEMONIC_RE.search(src) and not MNEMONIC_RE.search(t):
            yield "MN", t
            break
    if src == ALPHABET:
        for t in forms:
            if not set(src) <= set(t) or re.search(r"[\s,;-]", t):
                yield "ALPHA", t
                break
    src_tags = tags(src)
    for t in forms:
        if tags(t) != src_tags:
            diff = (src_tags - tags(t)) + (tags(t) - src_tags)
            yield "HTML", " ".join(f"<{c}{n}>" for c, n in diff)
            break
    if script:
        # Latin glued to native script is normal for tokens kept from the
        # source (gpg, git, QtPass + a particle), so only flag Latin runs
        # that the source does not contain.
        glued = re.compile(f"(?<=[{script}])[A-Za-z]+|[A-Za-z]+(?=[{script}])")
        for t in forms:
            t_clean = re.sub(r"%(?:\d+|n)", " ", t)
            if any(m.group().lower() not in src.lower() for m in glued.finditer(t_clean)):
                yield "MIX", t
                break


def audit(path, args):
    root = ET.parse(path).getroot()
    locale = root.get("language") or os.path.basename(path)[13:-3]
    script = SCRIPT.get(locale) or SCRIPT.get(locale.split("_")[0])
    counts = Counter()
    for msg in root.iter("message"):
        src = msg.findtext("source") or ""
        tr = msg.find("translation")
        if tr is None or tr.get("type") in ("vanished", "obsolete"):
            continue
        if msg.get("numerus") == "yes":
            forms = [f.text or "" for f in tr.findall("numerusform")]
        else:
            forms = [tr.text or ""]
        forms = [f for f in forms if f.strip()]
        if not forms:
            continue
        found = list(check(src, forms, script))
        if args.verbatim and len(src) > 2 and "href" not in src:
            if src not in VERBATIM_OK and all(f.strip() == src.strip() for f in forms):
                found.append(("SAME", forms[0]))
        if args.leak:
            low = " ".join(forms).lower()
            if any(w in low for w in args.leak):
                found.append(("LEAK", forms[0]))
        for code, detail in found:
            counts[code] += 1
            if not args.summary:
                print(f"{locale}: [{code}] {src[:60]!r}\n    -> {detail[:100]!r}")
    return locale, counts


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("files", nargs="*")
    p.add_argument("--summary", action="store_true", help="per-file counts only")
    p.add_argument("--verbatim", action="store_true", help="report translation == source")
    p.add_argument("--skip", type=lambda s: s.split(","), default=[],
                   help="comma-separated locales to leave out, e.g. sl for --leak")
    p.add_argument("--leak", type=lambda s: [w.lower() for w in s.split(",") if w],
                   help="comma-separated words from an unintended language")
    args = p.parse_args()
    files = args.files or sorted(glob.glob("localization/localization_*.ts"))
    if not files:
        sys.exit("no .ts files found: run from the repository root or pass paths")
    total = Counter()
    for path in files:
        if os.path.basename(path)[13:-3] in args.skip:
            continue
        locale, counts = audit(path, args)
        if args.summary and counts:
            print(f"{locale:8s} " + " ".join(f"{k}={v}" for k, v in sorted(counts.items())))
        total += counts
    print("total: " + (" ".join(f"{k}={v}" for k, v in sorted(total.items())) or "clean"))
    sys.exit(1 if any(total[k] for k in ("PH", "MN", "HTML", "MIX", "ALPHA")) else 0)


if __name__ == "__main__":
    main()
