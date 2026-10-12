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

# .ts files arrive from contributors (Weblate, PRs), so parse them as
# untrusted XML: defusedxml when installed; otherwise the standard library,
# which audit() only reaches after refusing entity declarations.
try:
    from defusedxml import ElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET  # nosec B405

from collections import Counter

# Native script per locale (full locale name first, then language code).
# Latin-script locales are absent, so MIX is skipped for them.
SCRIPT = {
    "ar": "؀-ۿ",
    "fa": "؀-ۿ",
    "ur": "؀-ۿ",
    "he": "֐-׿",
    "bg": "Ѐ-ӿ",
    "ru": "Ѐ-ӿ",
    "uk": "Ѐ-ӿ",
    "sr_Cyrl": "Ѐ-ӿ",
    "el": "Ͱ-Ͽ",
    "hi": "ऀ-ॿ",
    "mr": "ऀ-ॿ",
    "bn": "ঀ-৿",
    "pa": "਀-੿",
    "ta": "஀-௿",
    "te": "ఀ-౿",
    "si": "඀-෿",
    "th": "฀-๿",
    "ko": "가-힯",
    "ja": "぀-ヿ一-鿿",
    "zh": "一-鿿",
}

# Source strings that are deliberately identical in most locales.
VERBATIM_OK = {
    "QtPass",
    "GPG",
    "Git",
    "OTP",
    "PWGen",
    "pass",
    "Pass",
    "Email",
    "%1",
    "%2",
    "…",
    "⌕",
    "Aa",
    "LTR",
    "Ctrl+G",
    "Ctrl+N",
    "PGP",
    "GnuPG",
    "YubiKey",
    "Push",
    "Git push",
    "Git pull",
    "gpg",
    "git",
    "Git:",
    "qrencode",
    "pwgen",
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
    """Count the HTML tags in text as (closing slash, name) pairs."""
    return Counter((m.group(1), m.group(2).lower()) for m in TAG_RE.finditer(text))


def first(forms, bad):
    """The first plural form for which bad(form) holds, or None."""
    return next((t for t in forms if bad(t)), None)


def check_placeholders(src, forms):
    """%1, %2 ... must match in every form; %n must appear in at least one
    form when the source has it (a singular form may spell the number out)."""
    if (N_RE.search(src) is not None) != any(N_RE.search(t) for t in forms):
        return forms[0]
    want = sorted(PH_RE.findall(src))

    def bad(t):
        return sorted(PH_RE.findall(t)) != want or SPACED_RE.search(t)

    return first(forms, bad)


def check_mnemonic(src, forms):
    """A source with an & shortcut needs one in every form."""
    if not MNEMONIC_RE.search(src):
        return None
    return first(forms, lambda t: not MNEMONIC_RE.search(t))


def check_alphabet(src, forms):
    """The generator alphabet may gain letters, never lose any or add separators."""
    if src != ALPHABET:
        return None
    return first(forms, lambda t: not set(src) <= set(t) or re.search(r"[\s,;-]", t))


def check_html(src, forms):
    """The same tags, as many times, as in the source; reports the difference."""
    src_tags = tags(src)
    t = first(forms, lambda t: tags(t) != src_tags)
    if t is None:
        return None
    diff = (src_tags - tags(t)) + (tags(t) - src_tags)
    return " ".join(f"<{c}{n}>" for c, n in diff)


def check_mixed_script(src, forms, script):
    """Latin glued to native script is normal for tokens kept from the source
    (gpg, git, QtPass + a particle), so only Latin runs the source lacks."""
    if not script:
        return None
    glued = re.compile(f"(?<=[{script}])[A-Za-z]+|[A-Za-z]+(?=[{script}])")
    src_lower = src.lower()

    def foreign_latin(t):
        runs = glued.finditer(re.sub(r"%(?:\d+|n)", " ", t))
        return any(m.group().lower() not in src_lower for m in runs)

    return first(forms, foreign_latin)


def check(src, forms, script):
    """Yield (code, detail) for each structural problem."""
    for code, found in (
        ("PH", check_placeholders(src, forms)),
        ("MN", check_mnemonic(src, forms)),
        ("ALPHA", check_alphabet(src, forms)),
        ("HTML", check_html(src, forms)),
        ("MIX", check_mixed_script(src, forms, script)),
    ):
        if found is not None:
            yield code, found


def translated_forms(msg):
    """The non-empty translated forms of a <message>, or [] to skip it."""
    tr = msg.find("translation")
    if tr is None or tr.get("type") in ("vanished", "obsolete"):
        return []
    if msg.get("numerus") == "yes":
        forms = [f.text or "" for f in tr.findall("numerusform")]
    else:
        forms = [tr.text or ""]
    return [f for f in forms if f.strip()]


def optional_findings(src, forms, args):
    """The --verbatim (SAME) and --leak (LEAK) findings for one message."""
    found = []
    verbatim = all(f.strip() == src.strip() for f in forms)
    wanted = args.verbatim and len(src) > 2 and "href" not in src
    if wanted and src not in VERBATIM_OK and verbatim:
        found.append(("SAME", forms[0]))
    if args.leak and any(w in " ".join(forms).lower() for w in args.leak):
        found.append(("LEAK", forms[0]))
    return found


def audit(path, args):
    """Audit one .ts file, print its findings, return (locale, counts)."""
    with open(path, "rb") as f:
        data = f.read()
    # A real .ts file has only <!DOCTYPE TS>; entity declarations are how
    # expansion bombs work, so refuse them whichever parser is in use.
    if b"<!ENTITY" in data:
        print(f"{path}: XML: declares entities, refused unparsed")
        return os.path.basename(path)[13:-3], Counter({"XML": 1})
    root = ET.fromstring(data)  # nosec B314
    locale = root.get("language") or os.path.basename(path)[13:-3]
    script = SCRIPT.get(locale) or SCRIPT.get(locale.split("_")[0])
    counts = Counter()
    for msg in root.iter("message"):
        src = msg.findtext("source") or ""
        forms = translated_forms(msg)
        if not forms:
            continue
        found = list(check(src, forms, script)) + optional_findings(src, forms, args)
        for code, detail in found:
            counts[code] += 1
            if not args.summary:
                print(f"{locale}: [{code}] {src[:60]!r}\n    -> {detail[:100]!r}")
    return locale, counts


def main():
    """Parse the arguments, audit every file, exit 1 on any finding."""
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("files", nargs="*")
    p.add_argument("--summary", action="store_true", help="per-file counts only")
    p.add_argument("--verbatim", action="store_true", help="translation == source")
    p.add_argument(
        "--skip",
        type=lambda s: s.split(","),
        default=[],
        help="comma-separated locales to leave out, e.g. sl for --leak",
    )
    p.add_argument(
        "--leak",
        type=lambda s: [w.lower() for w in s.split(",") if w],
        help="comma-separated words from an unintended language",
    )
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
            line = " ".join(f"{k}={v}" for k, v in sorted(counts.items()))
            print(f"{locale:8s} {line}")
        total += counts
    line = " ".join(f"{k}={v}" for k, v in sorted(total.items()))
    print("total: " + (line or "clean"))
    failed = any(total[k] for k in ("PH", "MN", "HTML", "MIX", "ALPHA", "XML"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
