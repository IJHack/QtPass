#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 IJHack
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fill empty translations in QtPass Qt Linguist .ts files.

Only empty <translation type="unfinished"> elements are filled, and the
result stays type="unfinished" so Weblate reviewers still see it. Nothing
else in a file changes: <source> lines, locations and finished translations
are kept byte for byte.

Every translation must pass the same checks before it is written: the same
%1, %2 and %n placeholders, the same HTML tags and line breaks, a kept &
shortcut, no trailing full stop the source does not have, and only letters
of the locale's own script (plus Latin words the source has). A translation
that fails stays empty.

Subcommands (run from the repository root; FILE defaults to every
localization/localization_*.ts):

  pending [FILE ...]
      Print the empty entries as JSON, with each locale's glossary terms,
      for a human or an agent to translate.
  apply FILE TRANSLATIONS.json
      Fill FILE from a JSON object that maps source text to its
      translation (the output of "pending", with translations added, also
      works).
  llm [--url URL] [--model MODEL] [--cache CACHE.json] [--skip l1,l2] [FILE ...]
      Translate with a local model behind an OpenAI-compatible API, such as
      FastFlowLM on an NPU (the default, port 52625) or ollama. The prompt
      names the dialog the string comes from and lists the terms the
      locale's file already uses for words in it (store, clipboard, ...),
      so the new text follows the file's own vocabulary.
  review [--since REF] [--back] [--revert CODES] [--skip l1,l2] [FILE ...]
      Pedantic review of every unfinished translation that has text (with
      --since, only those that were empty at git REF): the fill checks,
      plus the glossary term, the file's formal or informal address,
      quotes, numbers, sentences and the ending of the source. With --back
      the model translates each one back to English, and a lost or added
      negation, words such as "error" or "wrong" the source lacks, and text
      that drifted from the source are findings too. The back-translation
      uses another model than the fill (qwen3.5:9b; --fill-model names the
      one that wrote the text, which review refuses). --revert empties the
      entries with a finding whose code starts with one of CODES (or all).
      Exit status 1 when anything is found.

en_US is never filled (it falls back to the source); en_GB gets the source.
"""

import argparse
import glob
import html
import json
import os
import re
import subprocess  # nosec B404
import sys
import time
import unicodedata
import urllib.request
from collections.abc import Callable, Iterator

DEFAULT_URL = "http://localhost:52625/v1"
DEFAULT_MODEL = "translategemma:4b"
# review --back must not ask the model that wrote the text: it would read
# its own mistakes back the way it meant them.
DEFAULT_REVIEW_MODEL = "qwen3.5:9b"

# English names the model understands, per locale file suffix.
LANGUAGE = {
    "af": "Afrikaans",
    "ar": "Arabic",
    "bg": "Bulgarian",
    "bn": "Bengali",
    "ca": "Catalan",
    "cs": "Czech",
    "cy": "Welsh",
    "da": "Danish",
    "de_DE": "German",
    "de_LU": "German",
    "el": "Greek",
    "es_AR": "Spanish (Argentina)",
    "es_EC": "Spanish (Ecuador)",
    "es_ES": "Spanish (Spain)",
    "es_MX": "Spanish (Mexico)",
    "es_UY": "Spanish (Uruguay)",
    "et": "Estonian",
    "fa": "Persian",
    "fi": "Finnish",
    "fr_BE": "French",
    "fr_FR": "French",
    "fr_LU": "French",
    "fy_NL": "West Frisian",
    "gl": "Galician",
    "he": "Hebrew",
    "hi": "Hindi",
    "hr": "Croatian",
    "hu": "Hungarian",
    "id": "Indonesian",
    "it": "Italian",
    "ja": "Japanese",
    "ko": "Korean",
    "lb_LU": "Luxembourgish",
    "lt": "Lithuanian",
    "lv": "Latvian",
    "mr": "Marathi",
    "nb": "Norwegian Bokmål",
    "nl_BE": "Dutch (Flemish)",
    "nl_NL": "Dutch",
    "pa_IN": "Punjabi (Gurmukhi script)",
    "pl": "Polish",
    "pt_BR": "Brazilian Portuguese",
    "pt_PT": "European Portuguese",
    "ro": "Romanian",
    "ru": "Russian",
    "si": "Sinhala",
    "sk": "Slovak",
    "sl": "Slovenian",
    "sq": "Albanian",
    "sr_Cyrl": "Serbian (Cyrillic script)",
    "sr_RS": "Serbian (Latin script)",
    "sv": "Swedish",
    "sw": "Swahili",
    "ta": "Tamil",
    "te": "Telugu",
    "th": "Thai",
    "tr": "Turkish",
    "uk": "Ukrainian",
    "ur": "Urdu",
    "vi": "Vietnamese",
    "zh_CN": "Simplified Chinese",
    "zh_Hant": "Traditional Chinese",
}

# Unicode script names (first word of the character name) a locale writes
# in besides Latin. Locales not listed write Latin only.
SCRIPTS = {
    "ar": {"ARABIC"},
    "fa": {"ARABIC"},
    "ur": {"ARABIC"},
    "he": {"HEBREW"},
    "bg": {"CYRILLIC"},
    "ru": {"CYRILLIC"},
    "uk": {"CYRILLIC"},
    "sr_Cyrl": {"CYRILLIC"},
    "el": {"GREEK"},
    "hi": {"DEVANAGARI"},
    "mr": {"DEVANAGARI"},
    "bn": {"BENGALI"},
    "pa": {"GURMUKHI"},
    "ta": {"TAMIL"},
    "te": {"TELUGU"},
    "si": {"SINHALA"},
    "th": {"THAI"},
    "ko": {"HANGUL", "CJK"},
    "ja": {"CJK", "HIRAGANA", "KATAKANA", "KATAKANA-HIRAGANA", "IDEOGRAPHIC"},
    "zh": {"CJK"},
}

# Where a message context appears, in words a translator can use.
CONTEXT = {
    "ConfigDialog": "the settings dialog",
    "MainWindow": "the main window",
    "PasswordDialog": "the password editor",
    "UsersDialog": "the dialog that chooses who can decrypt the passwords",
    "KeygenDialog": "the key generation dialog",
    "ImportKeyDialog": "the key import dialog",
    "ExportPublicKeyDialog": "the public key export dialog",
    "FirstRunWizard": "the first-run setup wizard",
    "StoreWizardPage": "the first-run setup wizard",
    "KeyWizardPage": "the first-run setup wizard",
    "ProgramsWizardPage": "the first-run setup wizard",
    "DoneWizardPage": "the first-run setup wizard",
    "TrayIcon": "the system tray menu",
    "ProcessOutputPanel": "the process output panel",
}

# Words that address the reader, informal then formal. German formal words
# only count with their capital, which tells "Sie" (you) from "sie" (they).
REGISTER = {
    "nl": ({"je", "jij", "jou", "jouw", "jullie"}, {"u", "uw"}),
    "de": (
        {"du", "dich", "dir", "dein", "deine", "deinen", "deinem", "deiner"},
        {"Sie", "Ihnen", "Ihr", "Ihre", "Ihren", "Ihrem", "Ihrer"},
    ),
    "fr": ({"tu", "toi", "ton", "ta", "tes"}, {"vous", "votre", "vos"}),
    "es": ({"tú", "tus", "ti"}, {"usted", "ustedes"}),
}

# In a back-translation but not in the source, these say the text now
# reports a fault: "That errs on the hidden side" came back as "That is an
# error" in three languages.
ALARM = ("error", "bug", "wrong", "fault", "problem", "broken", "mistake", "incorrect")
NEGATIONS = r"\b(?:not|no|never|none|nothing|cannot|without|unless)\b|n't\b"
NEGATION = re.compile(NEGATIONS, re.IGNORECASE)
QUOTES = '"“”„«»「」『』‘’‚‹›'
SENTENCE_END = re.compile(r"[.!?。！？।۔](?=\s|$)")
DIGITS = re.compile(r"\d+")

PLACEHOLDER = re.compile(r"%(?:\d+|n)")
TAG = re.compile(r"</?[A-Za-z][^>]*>")
MNEMONIC = re.compile(r"&(?=[^&\s])")
WORD = re.compile(r"[^\W\d_]+")
SOURCE = re.compile(r"<source>(.*?)</source>", re.DOTALL)
EMPTY = re.compile(
    r'<translation type="unfinished">((?:\s*<numerusform></numerusform>)*\s*)'
    r"</translation>"
)
UNFINISHED = re.compile(
    r'<translation type="unfinished">(.*?)</translation>',
    re.DOTALL,
)
FORM = re.compile(r"<numerusform>(.*?)</numerusform>", re.DOTALL)
THINK = re.compile(r"<think>.*?</think>", re.DOTALL)
MESSAGE = re.compile(r"(<message(?: numerus=\"yes\")?>)(.*?)(</message>)", re.DOTALL)
CONTEXT_BLOCK = re.compile(r"<context>\s*<name>(.*?)</name>(.*?)</context>", re.DOTALL)
FINISHED = re.compile(r"<source>(.*?)</source>\s*<translation>(.*?)</translation>")

Ask = Callable[[str, float], str]
Terms = list[tuple[str, str]]
Translate = Callable[[str, str], str | None]


def locale_of(path: str) -> str:
    """The locale suffix of a localization_<locale>.ts path."""
    name = os.path.basename(path)
    match = re.match(r"localization_([a-z]{2,3}(?:_[A-Za-z]{2,4})?)\.ts$", name)
    if not match:
        raise ValueError(f"not a localization file: {path}")
    return match.group(1)


def scripts_for(locale: str) -> set:
    """The non-Latin scripts the locale writes in."""
    return SCRIPTS.get(locale, SCRIPTS.get(locale.split("_")[0], set()))


def ts_escape(text: str) -> str:
    """Escape text the way lupdate writes it."""
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return text.replace('"', "&quot;").replace("'", "&apos;")


def without_mnemonic(text: str) -> str:
    """text without its & shortcut marker (a literal && stays)."""
    return MNEMONIC.sub("", text.replace("&&", "\0")).replace("\0", "&&")


def term_key(text: str) -> str:
    """A glossary key: no shortcut, no trailing colon or ellipsis, lower case."""
    return without_mnemonic(text).strip().rstrip(":….").strip().lower()


def glossary(data: str) -> dict[str, str]:
    """Short finished source -> translation pairs of one .ts file's text."""
    terms: dict[str, str] = {}
    for src, tr in FINISHED.findall(data):
        src, tr = html.unescape(src), html.unescape(tr)
        key = term_key(src)
        short = 1 <= len(key.split()) <= 3 and len(key) >= 4
        plain = not PLACEHOLDER.search(src) and "\n" not in src
        if not tr.strip() or not short or not plain or not WORD.search(key):
            continue
        terms.setdefault(key, without_mnemonic(tr).strip().rstrip(":….").strip())
    return terms


def terms_in(source: str, terms: dict[str, str], limit: int = 10) -> Terms:
    """The glossary terms that occur in source as whole words, longest first."""
    text = without_mnemonic(source).lower()
    found = []
    for key in sorted(terms, key=lambda k: (-len(k.split()), -len(k))):
        pattern = r"(?<!\w)" + re.escape(key) + r"(?:s|es)?(?!\w)"
        if re.search(pattern, text):
            found.append((key, terms[key]))
            # "store" still counts after "password store", but only where
            # it stands on its own.
            text = re.sub(pattern, "\0", text)
        if len(found) == limit:
            break
    return found


def is_latin(word: str) -> bool:
    """Whether every letter of word is Latin."""
    return all(unicodedata.name(c, "?").startswith("LATIN") for c in word)


def problems(source: str, out: str, locale: str) -> list[str]:
    """Why out cannot stand as the translation of source (empty when it can)."""
    found = []
    if sorted(PLACEHOLDER.findall(source)) != sorted(PLACEHOLDER.findall(out)):
        found.append("placeholders")
    if sorted(TAG.findall(source)) != sorted(TAG.findall(out)):
        found.append("html")
    if out.count("\n") != source.count("\n"):
        found.append("line breaks")
    if not out.strip() or len(out) > 4 * len(source) + 40:
        found.append("length")
    has_shortcut = MNEMONIC.search(source.replace("&&", ""))
    if has_shortcut and not MNEMONIC.search(out.replace("&&", "")):
        found.append("shortcut")
    allowed = scripts_for(locale) | {"LATIN"}
    letters = {unicodedata.name(c, "?").split()[0] for c in out if c.isalpha()}
    if letters - allowed:
        found.append("script " + ",".join(sorted(letters - allowed)))
    if scripts_for(locale):
        source_words = {w.lower() for w in WORD.findall(source)}
        latin = [w for w in WORD.findall(out) if is_latin(w)]
        if any(w.lower() not in source_words for w in latin):
            found.append("latin words")
    return found


def tidy(source: str, out: str, locale: str) -> str:
    """Strip quotes the model wrapped around out, drop a full stop the source
    lacks, and put the & shortcut back when the model dropped it."""
    out = out.strip()
    for left, right in (('"', '"'), ("“", "”"), ("«", "»"), ("„", "“")):
        if out.startswith(left) and out.endswith(right) and not source.startswith(left):
            inner = out.removeprefix(left).removesuffix(right)
            out = inner.strip()
    if not source.rstrip().endswith((".", "。")) and re.search(r"[^.][.。]$", out):
        out = out[:-1]
    shortcut = MNEMONIC.search(source.replace("&&", "\0\0"))
    if shortcut and not MNEMONIC.search(out.replace("&&", "")):
        key = source[shortcut.end()]
        letter = re.search(r"\w", out)
        if scripts_for(locale) or not letter:
            out = f"{out}(&{key.upper()})"
        else:
            at = letter.start()
            out = out[:at] + "&" + out[at:]
    return out


def prompt(locale: str, context: str, source: str, terms: Terms) -> str:
    """The translation request for one string (TranslateGemma's own wording,
    with the place in QtPass, the glossary and the rules added)."""
    lang = LANGUAGE[locale]
    code = locale.split("_")[0]
    where = CONTEXT.get(context, "a message")
    parts = [
        (
            f"You are a professional English (en) to {lang} ({code}) translator. "
            f"Your goal is to accurately convey the meaning and nuances of the "
            f"original English text while adhering to {lang} grammar, vocabulary, "
            f"and cultural sensitivities. The text is a user interface string "
            f"from {where} of QtPass, a password manager."
        ),
    ]
    if terms:
        parts.append(
            "Use these established translations for terms that occur in it:\n"
            + "\n".join(f"{k} = {v}" for k, v in terms)
        )
    parts.append(
        "Keep placeholders such as %1, %2 and %n, HTML tags, and the words "
        f"Git, GPG, pass and .gpg-id unchanged. Produce only the {lang} "
        f"translation, without any additional explanations or commentary. "
        f"Please translate the following English text into {lang}:\n\n\n{source}"
    )
    return "\n".join(parts)


def reply_text(content: str) -> str:
    """A model's answer without the <think> block a reasoning model adds."""
    return THINK.sub("", content).strip()


def chat(url: str, model: str) -> Ask:
    """An Ask that sends a prompt to an OpenAI-compatible chat endpoint."""

    def ask(text: str, temperature: float) -> str:
        body = json.dumps(
            {
                "model": model,
                "temperature": temperature,
                "messages": [{"role": "user", "content": text}],
            }
        ).encode()
        request = urllib.request.Request(
            url.rstrip("/") + "/chat/completions",
            body,
            {"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(request, timeout=300) as reply:  # nosec B310
            return reply_text(json.load(reply)["choices"][0]["message"]["content"])

    return ask


def entries(data: str) -> Iterator[tuple[str, str]]:
    """(context, source) of every empty unfinished message in a file's text."""
    for name, body in CONTEXT_BLOCK.findall(data):
        for message in MESSAGE.finditer(body):
            if EMPTY.search(message.group(2)):
                src = SOURCE.search(message.group(2))
                if src:
                    yield name, html.unescape(src.group(1))


def translation_element(inner: str, out: str) -> str:
    """The filled <translation> for an empty one whose content was inner:
    every plural form gets out, in the file's own indentation."""
    if "<numerusform>" not in inner:
        return f'<translation type="unfinished">{ts_escape(out)}</translation>'
    indent = re.search(r"\n([ \t]*)<numerusform>", inner)
    pad = indent.group(1) if indent else ""
    close = inner.rsplit("</numerusform>", 1)[1]
    form = f"\n{pad}<numerusform>{ts_escape(out)}</numerusform>"
    forms = form * inner.count("<numerusform>")
    return f'<translation type="unfinished">{forms}{close}</translation>'


def edit_messages(data: str, edit: Callable[[str, str], str]) -> str:
    """data with each <message> body replaced by edit(context, body)."""

    def message(context: str, match: "re.Match[str]") -> str:
        return match.group(1) + edit(context, match.group(2)) + match.group(3)

    def context(block: "re.Match[str]") -> str:
        name = block.group(1)
        body = MESSAGE.sub(lambda m: message(name, m), block.group(2))
        start, end = block.start(2) - block.start(), block.end(2) - block.start()
        return block.group(0)[:start] + body + block.group(0)[end:]

    return CONTEXT_BLOCK.sub(context, data)


def fill(data: str, translate: Translate) -> tuple[str, int, int]:
    """Fill each empty unfinished message of a file's text with
    translate(context, source); None leaves it empty. Returns the new text
    and the numbers filled and left."""
    counts = [0, 0]

    def edit(context: str, body: str) -> str:
        empty = EMPTY.search(body)
        src = SOURCE.search(body)
        if not empty or not src:
            return body
        out = translate(context, html.unescape(src.group(1)))
        if out is None:
            counts[1] += 1
            return body
        counts[0] += 1
        start, end = empty.span()
        return body[:start] + translation_element(empty.group(1), out) + body[end:]

    return edit_messages(data, edit), counts[0], counts[1]


def files(names: list[str]) -> list[str]:
    """The named files, or every localization file."""
    return names or sorted(glob.glob("localization/localization_*.ts"))


def read(path: str) -> str:
    """A file's text."""
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def write(path: str, data: str) -> None:
    """Replace a file's text."""
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(data)


def checked(locale: str, source: str, out: str | None) -> str | None:
    """out tidied, or None when it fails a check."""
    if out is None:
        return None
    out = tidy(source, out, locale)
    return None if problems(source, out, locale) else out


def cmd_pending(args: argparse.Namespace) -> int:
    """Print the empty entries of the files as JSON."""
    result = []
    for path in files(args.files):
        locale = locale_of(path)
        if locale == "en_US":
            continue
        data = read(path)
        terms = glossary(data)
        for context, source in entries(data):
            result.append(
                {
                    "locale": locale,
                    "context": context,
                    "source": source,
                    "glossary": dict(terms_in(source, terms)),
                    "translation": "",
                }
            )
    json.dump(result, sys.stdout, ensure_ascii=False, indent=1)
    print()
    return 0


def cmd_apply(args: argparse.Namespace) -> int:
    """Fill one file from a JSON mapping of source to translation."""
    locale = locale_of(args.file)
    with open(args.translations, encoding="utf-8") as handle:
        given = json.load(handle)
    if isinstance(given, list):
        mine = [e for e in given if e.get("locale", locale) == locale]
        given = {e["source"]: e["translation"] for e in mine if e.get("translation")}
    rejected = []

    def translate(_context: str, source: str) -> str | None:
        out = checked(locale, source, given.get(source))
        if source in given and out is None:
            why = problems(source, tidy(source, given[source], locale), locale)
            rejected.append(f"{', '.join(why)}: {source[:60]!r}")
        return out

    data, filled, left = fill(read(args.file), translate)
    write(args.file, data)
    print(f"{locale}: filled {filled}, left {left}")
    for line in rejected:
        print(f"  refused ({line})", file=sys.stderr)
    return 1 if rejected else 0


def load_cache(path: str | None) -> dict[str, str | None]:
    """The answers kept in a cache file, or none."""
    if not path or not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def save_cache(path: str | None, cache: dict[str, str | None]) -> None:
    """Keep the answers for the next run."""
    if path:
        # Write aside and rename, so an interrupted run keeps the old file.
        with open(path + ".tmp", "w", encoding="utf-8") as handle:
            json.dump(cache, handle, ensure_ascii=False, indent=0)
        os.replace(path + ".tmp", path)


def translate_one(
    ask: Ask, locale: str, context: str, source: str, terms: dict[str, str]
) -> tuple[str | None, list[str]]:
    """Ask the model up to three times; the first answer that passes wins."""
    text = without_mnemonic(source)
    request = prompt(locale, context, text, terms_in(source, terms))
    why: list[str] = []
    for temperature in (0.0, 0.3, 0.6):
        try:
            out = tidy(source, ask(request, temperature), locale)
        except OSError as error:
            why = [f"backend: {error}"]
            time.sleep(5)
            continue
        why = problems(source, out, locale)
        if not why:
            return out, []
    return None, why


def llm_file(
    path: str,
    locale: str,
    ask: Ask,
    cache: dict[str, str | None],
    args: argparse.Namespace,
) -> None:
    """Fill one file with a local model, through the answer cache."""
    data = read(path)
    terms = glossary(data)

    def translate(context: str, source: str) -> str | None:
        if locale == "en_GB":
            return source
        key = f"{args.model}\x1f{locale}\x1f{context}\x1f{source}"
        if key not in cache:
            out, why = translate_one(ask, locale, context, source, terms)
            if why:
                print(f"  {locale}: left empty ({', '.join(why)}): {source[:60]!r}")
            if why[-1:] and why[-1].startswith("backend"):
                return None  # not an answer: ask again on the next run
            cache[key] = out
            save_cache(args.cache, cache)
        return checked(locale, source, cache[key])

    data, filled, left = fill(data, translate)
    write(path, data)
    print(f"{locale}: filled {filled}, left {left}", flush=True)


def cmd_llm(args: argparse.Namespace, ask: Ask | None = None) -> int:
    """Fill the files with a local model."""
    ask = ask or chat(args.url, args.model)
    skip = set(filter(None, args.skip.split(",")))
    cache = load_cache(args.cache)
    for path in files(args.files):
        locale = locale_of(path)
        if locale in skip or (locale not in LANGUAGE and locale != "en_GB"):
            print(f"{locale}: skipped")
            continue
        llm_file(path, locale, ask, cache, args)
    return 0


def stem(word: str) -> str:
    """The first two thirds of a word, enough to survive an inflection."""
    return word.lower()[: max(3, len(word) * 2 // 3)]


def register_words(text: str, language: str) -> tuple[int, int]:
    """How many informal and formal words of address text has."""
    informal, formal = REGISTER[language]
    words = WORD.findall(text)
    casual = sum(w.lower() in informal for w in words)
    polite = sum((w if language == "de" else w.lower()) in formal for w in words)
    return casual, polite


def register_of(data: str, locale: str) -> str | None:
    """How a file's finished translations address the reader: "informal",
    "formal", or None when the language has no table or too few hits."""
    language = locale.split("_")[0]
    if language not in REGISTER:
        return None
    casual = polite = 0
    for _src, tr in FINISHED.findall(data):
        more_casual, more_polite = register_words(html.unescape(tr), language)
        casual, polite = casual + more_casual, polite + more_polite
    if max(casual, polite) < 3:
        return None
    return "informal" if casual > polite else "formal"


def ending(text: str) -> str:
    """The colon, ellipsis or question mark text ends with, in any script."""
    text = text.rstrip().replace("...", "…").replace("：", ":")
    text = text.replace("？", "?").replace("؟", "?")
    return text[-1:] if text[-1:] in (":", "…", "?") else ""


def numbers(text: str) -> list[int]:
    """The numbers in text, in any script's digits."""
    return sorted(int(d) for d in DIGITS.findall(text))


def review_text(
    source: str,
    out: str,
    locale: str,
    terms: dict[str, str],
    register: str | None,
) -> list[str]:
    """Pedantic findings on one translation: the fill checks and more."""
    found = problems(source, out, locale)
    text = without_mnemonic(out).lower()
    for key, value in terms_in(source, terms):
        if not all(stem(w) in text for w in WORD.findall(value)):
            found.append(f"term {key}={value}")
    if sum(out.count(q) for q in QUOTES) < source.count('"'):
        found.append("quotes")
    if numbers(source) != numbers(out):
        found.append("numbers")
    if ending(source) != ending(out):
        found.append("ending")
    # "..." is an ellipsis, not three sentence ends.
    ends = [SENTENCE_END.findall(t.replace("...", "…")) for t in (source, out)]
    if locale != "th" and len(ends[0]) != len(ends[1]):
        found.append("sentences")
    if register:
        casual, polite = register_words(out, locale.split("_")[0])
        if (polite if register == "informal" else casual) > 0:
            found.append(f"register (the file is {register})")
    if out.strip() == source.strip() and len(WORD.findall(source)) > 2:
        found.append("untranslated")
    return found


def meaning(source: str, back: str) -> list[str]:
    """Findings from comparing the source with a back-translation."""
    found = []
    src, bck = source.lower(), back.lower()
    if bool(NEGATION.search(src)) != bool(NEGATION.search(bck)):
        found.append("negation")
    alarm = [w for w in ALARM if w in bck and w not in src]
    if alarm:
        found.append("alarm " + ",".join(alarm))
    words = {w[:5] for w in WORD.findall(src) if len(w) >= 4}
    kept = words & {w[:5] for w in WORD.findall(bck)}
    # Short strings come back as synonyms ("Default" -> "Standard"), so
    # only a longer one that lost half its words counts.
    if len(words) >= 4 and len(kept) < len(words) / 2:
        found.append(f"drift {len(kept)}/{len(words)}")
    return found


def back_prompt(locale: str, text: str) -> str:
    """TranslateGemma's request, the other way round: into English."""
    lang = LANGUAGE[locale]
    code = locale.split("_")[0]
    return (
        f"You are a professional {lang} ({code}) to English (en) translator. "
        f"Your goal is to accurately convey the meaning and nuances of the "
        f"original {lang} text while adhering to English grammar, vocabulary, "
        f"and cultural sensitivities. Produce only the English translation, "
        f"without any additional explanations or commentary. Please translate "
        f"the following {lang} text into English:\n\n\n{text}"
    )


def back_translate(
    ask: Ask,
    locale: str,
    text: str,
    cache: dict[str, str | None],
    args: argparse.Namespace,
) -> str | None:
    """text in English, through the cache; None when the backend fails."""
    key = f"back\x1f{args.model}\x1f{locale}\x1f{text}"
    if key not in cache:
        try:
            cache[key] = ask(back_prompt(locale, without_mnemonic(text)), 0.0).strip()
        except OSError as error:
            print(f"  {locale}: no back-translation ({error})", file=sys.stderr)
            return None
        save_cache(args.cache, cache)
    return cache[key]


def empty_since(path: str, ref: str) -> set[tuple[str, str]] | None:
    """(context, source) of the messages that were empty at git ref, or None
    when the file did not exist there."""
    spec = f"{ref}:./{os.path.relpath(path)}"
    old = subprocess.run(  # nosec B603 B607
        ["git", "show", spec],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return set(entries(old.stdout)) if old.returncode == 0 else None


def emptied(inner: str) -> str:
    """The empty unfinished <translation> for one whose content was inner."""
    if "<numerusform>" in inner:
        inner = FORM.sub("<numerusform></numerusform>", inner)
    else:
        inner = ""
    return f'<translation type="unfinished">{inner}</translation>'


def review_file(
    path: str,
    locale: str,
    ask: Ask | None,
    cache: dict[str, str | None],
    args: argparse.Namespace,
) -> int:
    """Review one file, print the findings, empty the reverted entries;
    returns the number of entries with a finding."""
    data = read(path)
    terms = glossary(data)
    register = register_of(data, locale)
    fresh = empty_since(path, args.since) if args.since else None
    revert = set(filter(None, args.revert.split(",")))
    count = [0, 0]

    def edit(context: str, body: str) -> str:
        unfinished = UNFINISHED.search(body)
        src = SOURCE.search(body)
        if not unfinished or not src:
            return body
        source = html.unescape(src.group(1))
        inner = unfinished.group(1)
        forms = [f for f in FORM.findall(inner) or [inner] if f.strip()]
        forms = [html.unescape(f) for f in forms]
        if not forms or (fresh is not None and (context, source) not in fresh):
            return body
        found = []
        for form in forms:
            found += review_text(source, form, locale, terms, register)
        back = back_translate(ask, locale, forms[0], cache, args) if ask else None
        if back is not None:
            found += meaning(source, back)
        if not found:
            return body
        found = list(dict.fromkeys(found))
        count[0] += 1
        print(f"{locale}: [{'; '.join(found)}] {source[:70]!r}")
        print(f"    -> {forms[0][:110]!r}")
        if back is not None:
            print(f"    <- {back[:110]!r}")
        matched = {c for c in revert for f in found if f.startswith(c)}
        if "all" not in revert and not matched:
            return body
        count[1] += 1
        start, end = unfinished.span()
        return body[:start] + emptied(inner) + body[end:]

    new = edit_messages(data, edit)
    if new != data:
        write(path, new)
    if count[1]:
        print(f"{locale}: emptied {count[1]}")
    return count[0]


def cmd_review(args: argparse.Namespace, ask: Ask | None = None) -> int:
    """Review the unfinished translations of the files."""
    if not args.back:
        ask = None
    elif args.model == args.fill_model:
        print(
            f"review: {args.model} wrote the text; back-translate with another "
            "model (--model) or name the one that filled it (--fill-model)",
            file=sys.stderr,
        )
        return 2
    elif ask is None:
        ask = chat(args.url, args.model)
    skip = set(filter(None, args.skip.split(","))) | {"en_US", "en_GB"}
    cache = load_cache(args.cache)
    total = 0
    for path in files(args.files):
        locale = locale_of(path)
        if locale in skip or locale not in LANGUAGE:
            continue
        total += review_file(path, locale, ask, cache, args)
    print(f"total: {total} with findings" if total else "total: clean")
    return 1 if total else 0


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", maxsplit=1)[0])
    sub = parser.add_subparsers(dest="command", required=True)
    pending = sub.add_parser("pending", help="print the empty entries as JSON")
    pending.add_argument("files", nargs="*")
    apply = sub.add_parser("apply", help="fill one file from JSON")
    apply.add_argument("file")
    apply.add_argument("translations")
    llm = sub.add_parser("llm", help="fill with a local model")
    llm.add_argument("--url", default=DEFAULT_URL)
    llm.add_argument("--model", default=DEFAULT_MODEL)
    llm.add_argument("--cache", help="JSON file that keeps answers between runs")
    llm.add_argument("--skip", default="", help="locales to leave alone (a,b,...)")
    llm.add_argument("files", nargs="*")
    review = sub.add_parser("review", help="pedantic review of unfinished text")
    review.add_argument("--since", help="only entries that were empty at this git ref")
    review.add_argument("--back", action="store_true", help="back-translate too")
    review.add_argument("--revert", default="", help="empty entries with these codes")
    review.add_argument("--url", default=DEFAULT_URL)
    review.add_argument("--model", default=DEFAULT_REVIEW_MODEL)
    review.add_argument("--fill-model", default=DEFAULT_MODEL, help="who wrote it")
    review.add_argument("--cache", help="JSON file that keeps answers between runs")
    review.add_argument("--skip", default="", help="locales to leave alone (a,b,...)")
    review.add_argument("files", nargs="*")
    args = parser.parse_args(argv)
    if args.command == "pending":
        return cmd_pending(args)
    if args.command == "apply":
        return cmd_apply(args)
    if args.command == "review":
        return cmd_review(args)
    return cmd_llm(args)


if __name__ == "__main__":
    sys.exit(main())
