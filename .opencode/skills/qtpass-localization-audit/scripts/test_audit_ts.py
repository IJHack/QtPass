#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 IJHack
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for audit_ts: the SCRIPT check and which script a file gets."""

import argparse
import contextlib
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

_PATH = Path(__file__).with_name("audit_ts.py")
_SPEC = importlib.util.spec_from_file_location("audit_ts", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
audit_ts = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(audit_ts)

TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="{language}">
<context>
    <name>ConfigDialog</name>
    <message>
        <source>{source}</source>
        <translation>{translation}</translation>
    </message>
</context>
</TS>
"""


def audit(name: str, language: str, source: str, translation: str):
    """The finding counts of a one-message localization_<name>.ts."""
    text = TEMPLATE.format(language=language, source=source, translation=translation)
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, f"localization_{name}.ts")
        Path(path).write_text(text, encoding="utf-8")
        args = argparse.Namespace(summary=True, verbatim=False, leak=None)
        with contextlib.redirect_stdout(io.StringIO()):
            return audit_ts.audit(path, args)[1]


class ScriptTest(unittest.TestCase):
    """SCRIPT fires on letters of a script the locale does not write in."""

    def test_wrong_script(self) -> None:
        """Devanagari Punjabi and Cyrillic in Dutch are findings."""
        self.assertEqual(audit("pa_IN", "pa_IN", "Users", "ਵਰਤੋਂਕਾਰ"), {})
        self.assertEqual(audit("pa_IN", "pa_IN", "Users", "उपयोगकर्ता")["SCRIPT"], 1)
        self.assertEqual(audit("nl_NL", "nl_NL", "Store", "Oпслаг")["SCRIPT"], 1)

    def test_latin_and_source_letters(self) -> None:
        """Accented Latin, ordinals and letters the source has are fine."""
        self.assertEqual(audit("es_ES", "es_ES", "1st", "1º sí"), {})
        self.assertEqual(audit("nl_NL", "nl_NL", "Ελληνικά", "Ελληνικά"), {})

    def test_file_name_picks_the_script(self) -> None:
        """localization_sr_Cyrl.ts says sr_RS but is Cyrillic."""
        self.assertEqual(audit("sr_Cyrl", "sr_RS", "Store", "Складиште"), {})
        self.assertEqual(audit("sr_RS", "sr_RS", "Store", "Складиште")["SCRIPT"], 1)

    def test_exit_status(self) -> None:
        """A SCRIPT finding fails the run, as the structural checks do."""
        text = TEMPLATE.format(language="nl_NL", source="Store", translation="Oпслаг")
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "localization_nl_NL.ts")
            Path(path).write_text(text, encoding="utf-8")
            run = [sys.executable, str(_PATH), "--summary", path]
            done = subprocess.run(run, capture_output=True, text=True, check=False)
        self.assertEqual(done.returncode, 1)
        self.assertIn("SCRIPT=1", done.stdout)


if __name__ == "__main__":
    unittest.main()
