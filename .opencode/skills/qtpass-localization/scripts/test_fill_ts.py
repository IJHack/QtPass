#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 IJHack
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for fill_ts: what it fills, what it refuses and what it keeps."""

import argparse
import contextlib
import importlib.util
import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

_PATH = Path(__file__).with_name("fill_ts.py")
_SPEC = importlib.util.spec_from_file_location("fill_ts", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
fill_ts = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(fill_ts)

SAMPLE = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="nl_NL">
<context>
    <name>ConfigDialog</name>
    <message>
        <location filename="../src/configdialog.cpp" line="10"/>
        <source>Password store</source>
        <translation>Wachtwoordopslag</translation>
    </message>
    <message>
        <location filename="../src/configdialog.cpp" line="11"/>
        <source>Store</source>
        <translation>Opslag</translation>
    </message>
    <message>
        <location filename="../src/configdialog.cpp" line="12"/>
        <source>Clipboard</source>
        <translation type="unfinished">Klembord?</translation>
    </message>
    <message>
        <location filename="../src/configdialog.cpp" line="20"/>
        <source>Use the &quot;%1&quot; store</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="../src/configdialog.cpp" line="30"/>
        <source>%n line(s)</source>
        <translation type="unfinished">
            <numerusform></numerusform>
            <numerusform></numerusform>
        </translation>
    </message>
</context>
</TS>
"""


def answers(*replies: str):
    """An Ask that gives replies in turn and records each prompt."""
    asked = []

    def ask(text: str, _temperature: float) -> str:
        asked.append(text)
        return replies[min(len(asked), len(replies)) - 1]

    return ask, asked


class FillTest(unittest.TestCase):
    """fill() writes only the empty unfinished translations."""

    def test_fills_plain_and_plural_keeps_the_rest(self) -> None:
        """Both empty messages are filled; every other line stays as it was."""
        seen = []

        def translate(context: str, source: str) -> str:
            seen.append((context, source))
            return "Gebruik <b>&" if "%1" in source else "%n regel(s)"

        data, filled, left = fill_ts.fill(SAMPLE, translate)
        self.assertEqual((filled, left), (2, 0))
        self.assertEqual(
            seen,
            [("ConfigDialog", 'Use the "%1" store'), ("ConfigDialog", "%n line(s)")],
        )
        want = "Gebruik &lt;b&gt;&amp;</translation>"
        self.assertIn(f'<translation type="unfinished">{want}', data)
        form = "            <numerusform>%n regel(s)</numerusform>\n"
        self.assertIn(form * 2 + "        </translation>", data)
        old = [line for line in SAMPLE.splitlines() if "<translation" not in line]
        new = [line for line in data.splitlines() if "<translation" not in line]
        self.assertEqual(
            [line for line in old if "numerusform" not in line],
            [line for line in new if "numerusform" not in line],
        )

    def test_none_leaves_the_message_empty(self) -> None:
        """A None answer keeps the file byte for byte and counts as left."""
        data, filled, left = fill_ts.fill(SAMPLE, lambda _c, _s: None)
        self.assertEqual((data, filled, left), (SAMPLE, 0, 2))


class GlossaryTest(unittest.TestCase):
    """The glossary holds finished short terms and finds them in a source."""

    def test_only_finished_terms(self) -> None:
        """Unfinished text is no established term."""
        terms = fill_ts.glossary(SAMPLE)
        want = {"password store": "Wachtwoordopslag", "store": "Opslag"}
        self.assertEqual(terms, want)

    def test_longest_first_and_still_alone(self) -> None:
        """ "store" is found where it stands alone, not inside "password store"."""
        terms = {"password store": "Wachtwoordopslag", "store": "Opslag"}
        self.assertEqual(
            fill_ts.terms_in("Open the password store", terms),
            [("password store", "Wachtwoordopslag")],
        )
        self.assertEqual(
            fill_ts.terms_in("The password store and other stores", terms),
            [("password store", "Wachtwoordopslag"), ("store", "Opslag")],
        )
        self.assertEqual(fill_ts.terms_in("Restored", terms), [])


class CheckTest(unittest.TestCase):
    """problems() refuses what would break the UI; tidy() fixes small slips."""

    def test_placeholders_html_and_line_breaks(self) -> None:
        """A lost %1, a lost tag or a lost line break is refused."""
        self.assertEqual(fill_ts.problems("%1 of %2", "%1 van %2", "nl_NL"), [])
        self.assertIn("placeholders", fill_ts.problems("%1 of %2", "%1 van", "nl_NL"))
        self.assertIn("html", fill_ts.problems("<b>x</b>", "x", "nl_NL"))
        self.assertIn("line breaks", fill_ts.problems("a\n\nb", "a b", "nl_NL"))

    def test_scripts(self) -> None:
        """Letters of another script, or Latin words the source lacks, are refused."""
        self.assertEqual(fill_ts.problems("Users", "ਵਰਤੋਂਕਾਰ", "pa_IN"), [])
        hindi = fill_ts.problems("Users", "उपयोगकर्ता", "pa_IN")
        self.assertIn("script DEVANAGARI", hindi)
        self.assertIn("script CYRILLIC", fill_ts.problems("Store", "Oпслаг", "nl_NL"))
        self.assertEqual(fill_ts.problems("Use Git", "Git ਵਰਤੋ", "pa_IN"), [])
        glued = fill_ts.problems("Use Git", "nativer ਵਰਤੋ", "pa_IN")
        self.assertIn("latin words", glued)
        self.assertEqual(fill_ts.problems("Each one", "各々", "ja"), [])

    def test_locale_of(self) -> None:
        """Only a locale code counts as the suffix."""
        self.assertEqual(fill_ts.locale_of("x/localization_sr_Cyrl.ts"), "sr_Cyrl")
        self.assertEqual(fill_ts.locale_of("localization_fy_NL.ts"), "fy_NL")
        with self.assertRaises(ValueError):
            fill_ts.locale_of("localization/localization_backup.ts")

    def test_tidy(self) -> None:
        """Quotes and an extra full stop go; a dropped shortcut comes back."""
        self.assertEqual(fill_ts.tidy("Restored", '"Hersteld."', "nl_NL"), "Hersteld")
        self.assertEqual(fill_ts.tidy("Done.", "Klaar.", "nl_NL"), "Klaar.")
        self.assertEqual(fill_ts.tidy("Wait...", "Wacht...", "nl_NL"), "Wacht...")
        latin = fill_ts.tidy("&Use pass", "pass gebruiken", "nl_NL")
        self.assertEqual(latin, "&pass gebruiken")
        chinese = fill_ts.tidy("&Use pass", "使用 pass", "zh_CN")
        self.assertEqual(chinese, "使用 pass(&U)")


class LlmTest(unittest.TestCase):
    """cmd_llm asks again on a bad answer and keeps answers in the cache."""

    def run_llm(self, ask, names, cache=None):
        """Run cmd_llm over copies of SAMPLE named localization_<name>.ts."""
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for name in names:
                path = os.path.join(tmp, f"localization_{name}.ts")
                Path(path).write_text(SAMPLE, encoding="utf-8")
                paths.append(path)
            options = {"skip": "", "cache": cache, "model": "m", "url": "u"}
            args = argparse.Namespace(files=paths, **options)
            with contextlib.redirect_stdout(io.StringIO()):
                fill_ts.cmd_llm(args, ask)
            return [Path(p).read_text(encoding="utf-8") for p in paths]

    def test_retries_then_accepts(self) -> None:
        """A dropped %1 is asked again; the glossary and the dialog are in the prompt."""
        replies = ("Gebruik de opslag", 'Gebruik de opslag "%1"', "%n regel(s)")
        ask, asked = answers(*replies)
        (data,) = self.run_llm(ask, ["nl_NL"])
        self.assertIn("Gebruik de opslag &quot;%1&quot;</translation>", data)
        self.assertEqual(len(asked), 3)
        self.assertIn("store = Opslag", asked[0])
        self.assertIn("the settings dialog", asked[0])
        self.assertNotIn("password store = ", asked[0])

    def test_gives_up_after_three(self) -> None:
        """Three bad answers leave the message empty."""
        ask, asked = answers("geen plaatshouder")
        (data,) = self.run_llm(ask, ["nl_NL"])
        self.assertEqual(data, SAMPLE)
        self.assertEqual(len(asked), 6)

    def test_backend_failure_is_not_cached(self) -> None:
        """A server that does not answer leaves the entry for the next run."""
        asked = []

        def down(text: str, _temperature: float) -> str:
            asked.append(text)
            raise OSError("connection refused")

        with (
            tempfile.TemporaryDirectory() as tmp,
            mock.patch.object(fill_ts.time, "sleep"),
        ):
            cache = os.path.join(tmp, "cache.json")
            (data,) = self.run_llm(down, ["nl_NL"], cache)
            self.assertEqual(data, SAMPLE)
            self.assertEqual(len(asked), 6)
            self.assertFalse(os.path.exists(cache))
            ask, _ = answers("Gebruik %1", "%n regels")
            self.run_llm(ask, ["nl_NL"], cache)
            stored = json.loads(Path(cache).read_text(encoding="utf-8"))
            self.assertEqual(len(stored), 2)
            self.assertFalse(os.path.exists(cache + ".tmp"))

    def test_english_locales_and_cache(self) -> None:
        """en_US is skipped, en_GB copies the source, and the cache answers a rerun."""
        ask, asked = answers("Gebruik %1", "%n regels")
        with tempfile.TemporaryDirectory() as tmp:
            cache = os.path.join(tmp, "cache.json")
            us, gb, nl = self.run_llm(ask, ["en_US", "en_GB", "nl_NL"], cache)
            self.assertEqual(us, SAMPLE)
            self.assertIn("Use the &quot;%1&quot; store</translation>", gb)
            self.assertIn("Gebruik %1</translation>", nl)
            self.assertEqual(len(asked), 2)
            stored = json.loads(Path(cache).read_text(encoding="utf-8"))
            self.assertEqual(len(stored), 2)
            (again,) = self.run_llm(ask, ["nl_NL"], cache)
            self.assertEqual(again, nl)
            self.assertEqual(len(asked), 2)


class ApplyTest(unittest.TestCase):
    """apply fills from JSON and refuses a broken translation."""

    def test_apply(self) -> None:
        """A good entry is written, a bad one refused with exit status 1."""
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "localization_nl_NL.ts")
            Path(path).write_text(SAMPLE, encoding="utf-8")
            given = os.path.join(tmp, "nl.json")
            Path(given).write_text(
                json.dumps({'Use the "%1" store': "Gebruik %1", "%n line(s)": "x"}),
                encoding="utf-8",
            )
            args = argparse.Namespace(file=path, translations=given)
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                status = fill_ts.cmd_apply(args)
            data = Path(path).read_text(encoding="utf-8")
        self.assertEqual(status, 1)
        self.assertIn("Gebruik %1</translation>", data)
        self.assertIn("<numerusform></numerusform>", data)
        self.assertIn("placeholders", err.getvalue())


class ReviewTextTest(unittest.TestCase):
    """review_text is pedantic about what the fill checks let through."""

    def review(self, source: str, out: str, register: str | None = None):
        """The findings for out in a Dutch file with the SAMPLE glossary."""
        terms = fill_ts.glossary(SAMPLE)
        return fill_ts.review_text(source, out, "nl_NL", terms, register)

    def test_clean(self) -> None:
        """A faithful translation has no findings."""
        self.assertEqual(self.review('Open "the store"?', 'Open "de opslagen"?'), [])

    def test_each_finding(self) -> None:
        """Each check fires on its own slip."""
        want = ["term store=Opslag"]
        self.assertEqual(self.review("Open the store", "Open de winkel"), want)
        self.assertEqual(self.review('Use "x"', "Gebruik x"), ["quotes"])
        self.assertEqual(self.review("Keep 10 lines", "Houd 12 regels"), ["numbers"])
        self.assertEqual(self.review("Name:", "Naam"), ["ending"])
        self.assertEqual(self.review("Wait…", "Wacht..."), [])
        self.assertEqual(self.review("Wait…", "Wacht"), ["ending"])
        self.assertEqual(self.review("One. Two.", "Een, twee."), ["sentences"])
        self.assertEqual(self.review("Use this one", "Use this one"), ["untranslated"])
        self.assertEqual(self.review("Pass", "Pass"), [])

    def test_register(self) -> None:
        """A formal word in an informal file is a finding, and the other way."""
        self.assertEqual(
            self.review("Check it", "Controleer uw lijst", "informal"),
            ["register (the file is informal)"],
        )
        self.assertEqual(self.review("Check it", "Controleer je lijst", "informal"), [])
        self.assertEqual(
            self.review("Check it", "Controleer je lijst", "formal"),
            ["register (the file is formal)"],
        )

    def test_register_of(self) -> None:
        """The file's own finished text decides; German needs the capital."""
        line = "<source>a</source>\n<translation>{}</translation>\n"
        dutch = line.format("Kies je sleutel") * 3
        self.assertEqual(fill_ts.register_of(dutch, "nl_NL"), "informal")
        self.assertIsNone(fill_ts.register_of(dutch[: len(dutch) // 3], "nl_NL"))
        self.assertIsNone(fill_ts.register_of(dutch, "sv"))
        german = line.format("Wählen Sie, was sie sehen") * 3
        self.assertEqual(fill_ts.register_of(german, "de_DE"), "formal")
        casual, polite = fill_ts.register_words("sie sehen es", "de")
        self.assertEqual((casual, polite), (0, 0))


class MeaningTest(unittest.TestCase):
    """meaning() compares the source with the back-translation."""

    def test_findings(self) -> None:
        """A lost negation, a new alarm word and drift are findings."""
        hidden = "That errs on the hidden side"
        self.assertEqual(fill_ts.meaning(hidden, "That errs to the hidden side"), [])
        back = "That is an error on the hidden side"
        self.assertEqual(fill_ts.meaning(hidden, back), ["alarm error"])
        self.assertEqual(
            fill_ts.meaning("Do not show it", "Show it"),
            ["negation"],
        )
        long = "The name and the passphrase cannot contain line breaks"
        back = "The name and password phrase should not contain any violations of the rules"
        drift = fill_ts.meaning(long, back)
        self.assertEqual(drift, ["drift 2/6"])
        self.assertEqual(fill_ts.meaning("Show the error", "Show the error"), [])
        half = fill_ts.meaning("alpha bravo charlie delta", "alpha bravo echo golf")
        self.assertEqual(half, [])
        less = fill_ts.meaning("alpha bravo charlie delta hotel", "alpha bravo xray")
        self.assertEqual(less, ["drift 2/5"])
        short = fill_ts.meaning("Clipboard restored", "Whiteboard repaired")
        self.assertEqual(short, [])


class ReviewFileTest(unittest.TestCase):
    """review --since looks only at what was filled, --revert empties it."""

    def setUp(self) -> None:
        """A git repository holding SAMPLE, with no user or system config."""
        self.tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(self.tmp.cleanup)
        dialogs = {"SSH_ASKPASS", "GIT_ASKPASS", "DISPLAY", "WAYLAND_DISPLAY"}
        self.env = {k: v for k, v in os.environ.items() if k not in dialogs}
        self.env.update(
            GIT_CONFIG_GLOBAL=os.devnull,
            GIT_CONFIG_NOSYSTEM="1",
            GIT_TERMINAL_PROMPT="0",
        )
        self.path = os.path.join(self.tmp.name, "localization_nl_NL.ts")
        Path(self.path).write_text(SAMPLE, encoding="utf-8")
        for command in (
            ["init", "-q"],
            ["add", "."],
            ["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "x"],
        ):
            git = ["git", *command]
            subprocess.run(git, cwd=self.tmp.name, env=self.env, check=True)
        filled = SAMPLE.replace(
            '<translation type="unfinished"></translation>',
            '<translation type="unfinished">Gebruik &quot;%1&quot;</translation>',
        )
        plural = "<numerusform>%n regels.</numerusform>"
        filled = filled.replace("<numerusform></numerusform>", plural)
        Path(self.path).write_text(filled, encoding="utf-8")

    def run_review(self, ask=None, **options):
        """cmd_review on the file from inside the repository."""
        args = {"since": "HEAD", "back": False, "revert": "", "skip": ""}
        args.update(cache=None, model="m", fill_model="f", url="u", files=[self.path])
        args.update(options)
        out = io.StringIO()
        with (
            contextlib.chdir(self.tmp.name),
            contextlib.redirect_stdout(out),
            mock.patch.dict(os.environ, self.env, clear=True),
        ):
            status = fill_ts.cmd_review(argparse.Namespace(**args), ask)
        return status, out.getvalue(), Path(self.path).read_text(encoding="utf-8")

    def test_since_and_revert(self) -> None:
        """Only the newly filled entry is reviewed, and only it is emptied."""
        status, out, data = self.run_review()
        self.assertEqual(status, 1)
        self.assertIn("[term store=Opslag]", out)
        self.assertNotIn("Klembord", out)
        self.assertIn("Gebruik &quot;%1&quot;", data)
        _, everything, _ = self.run_review(since=None)
        self.assertIn("Klembord", everything)
        _, out, data = self.run_review(revert="numbers")
        self.assertIn("Gebruik &quot;%1&quot;", data)
        _, out, data = self.run_review(revert="term")
        self.assertIn("emptied 1", out)
        self.assertNotIn("Gebruik", data)
        self.assertIn("[sentences]", out)
        _, out, data = self.run_review(revert="all")
        self.assertIn("emptied 1", out)
        self.assertEqual(data, SAMPLE)

    def test_back(self) -> None:
        """--back asks once per entry, through the cache, and its findings count."""
        ask, asked = answers('Use the "%1" error')
        cache = os.path.join(self.tmp.name, "cache.json")
        _, out, _ = self.run_review(ask, back=True, cache=cache)
        self.assertIn("alarm error", out)
        self.assertIn("<- 'Use the \"%1\" error'", out)
        self.assertEqual(len(asked), 2)
        self.assertIn("Dutch (nl) to English (en)", asked[0])
        self.run_review(ask, back=True, cache=cache)
        self.assertEqual(len(asked), 2)
        _, out, _ = self.run_review(ask)
        self.assertNotIn("alarm", out)

    def test_back_refuses_the_fill_model(self) -> None:
        """The model that wrote the text may not check it."""
        ask, asked = answers("x")
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            status, _, _ = self.run_review(ask, back=True, model="f")
        self.assertEqual((status, asked), (2, []))
        self.assertIn("wrote the text", err.getvalue())
        self.assertNotEqual(fill_ts.DEFAULT_REVIEW_MODEL, fill_ts.DEFAULT_MODEL)
        argv = ["review", "--back", "--fill-model", fill_ts.DEFAULT_REVIEW_MODEL]
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(fill_ts.main([*argv, self.path]), 2)

    def test_reply_text(self) -> None:
        """A reasoning model's <think> block is not part of the answer."""
        self.assertEqual(fill_ts.reply_text("<think>hmm\nok</think>\n Yes "), "Yes")


if __name__ == "__main__":
    unittest.main()
