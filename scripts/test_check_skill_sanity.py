#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for check_skill_sanity: the link pattern and the frontmatter."""

import importlib.util
import time
import unittest
from pathlib import Path

_SPEC = importlib.util.spec_from_file_location(
    "check_skill_sanity", Path(__file__).with_name("check_skill_sanity.py")
)
assert _SPEC is not None and _SPEC.loader is not None
sanity = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(sanity)


def targets(text: str) -> list:
    """The link destinations LINK_RE finds in text."""
    return [match.group(1) for match in sanity.LINK_RE.finditer(text)]


class LinkPatternTest(unittest.TestCase):
    """LINK_RE finds every link form and stays fast on bad input."""

    def test_plain_and_angle_destinations(self) -> None:
        """A bare and an angle-bracketed destination are both found."""
        self.assertEqual(
            targets("[a](doc.md) [b](<my doc.md>)"), ["doc.md", "<my doc.md>"]
        )

    def test_title(self) -> None:
        """A title after the destination does not hide the link."""
        self.assertEqual(targets('[a](doc.md "Title here")'), ["doc.md"])

    def test_trailing_blanks(self) -> None:
        """Blanks before ")" do not hide the link, with or without a title."""
        self.assertEqual(targets("[a](missing   )"), ["missing"])
        self.assertEqual(targets("[a](\tmissing\t)"), ["missing"])
        self.assertEqual(targets('[a](doc.md "t"  )'), ["doc.md"])

    def test_blank_runs_stay_linear(self) -> None:
        """Long blank runs without a closing ")" are rejected quickly."""
        for text in (
            "[a](x" + " " * 20000,
            "[a](x " + "y " * 10000,
            "[a](x y" + " " * 20000,
        ):
            start = time.monotonic()
            self.assertEqual(targets(text), [])
            self.assertLess(time.monotonic() - start, 1.0)


class FrontmatterTest(unittest.TestCase):
    """parse_frontmatter refuses YAML aliases before building anything."""

    def test_alias_is_refused(self) -> None:
        """An alias in the frontmatter is an error, not a mapping."""
        if sanity.yaml is None:
            self.skipTest("PyYAML is not installed")
        result = sanity.CheckResult()
        text = "---\na: &x [1]\nb: *x\n---\nbody\n"
        self.assertEqual(sanity.parse_frontmatter(text, "SKILL.md", result), {})
        self.assertTrue(any("aliases" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main()
