#!/usr/bin/env python3
"""Deterministic sanity checks for QtPass agent skills."""

from __future__ import annotations

import argparse
import ast
import os
import re
import stat
import subprocess
import sys
from pathlib import Path
from typing import Iterable


MAX_SKILL_LINES = 220
LINK_RE = re.compile(r"(?<!\!)\[[^\]]+\]\(([^)]+)\)")
PATH_MENTION_RE = re.compile(
    r"(?<![`\w/.-])((?:references|scripts)/[A-Za-z0-9_./-]+\."
    r"(?:md|py|sh|txt|json|yml|yaml))"
)
URL_RE = re.compile(r"^[a-z][a-z0-9+.-]*:", re.IGNORECASE)


class CheckResult:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.skills: list[tuple[str, str, int, str]] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warning(self, message: str) -> None:
        self.warnings.append(message)


def repo_relative(root: Path, path: Path) -> str:
    return str(path.resolve().relative_to(root)).replace(os.sep, "/")


def parse_frontmatter(text: str, rel: str, result: CheckResult) -> dict[str, str]:
    if not text.startswith("---\n"):
        result.error(f"{rel}: missing YAML frontmatter")
        return {}

    parts = text.split("---\n", 2)
    if len(parts) < 3:
        result.error(f"{rel}: unterminated YAML frontmatter")
        return {}

    data: dict[str, str] = {}
    in_metadata = False
    metadata_indent = None

    for raw_line in parts[1].splitlines():
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue
        indent = len(raw_line) - len(raw_line.lstrip(" "))
        line = raw_line.strip()
        if indent == 0:
            in_metadata = line == "metadata:"
            metadata_indent = None
            if ":" not in line or in_metadata:
                continue
            key, value = line.split(":", 1)
            data[key.strip()] = value.strip().strip('"\'')
            continue
        if in_metadata:
            if metadata_indent is None:
                metadata_indent = indent
            if indent >= metadata_indent and ":" in line:
                key, value = line.split(":", 1)
                data[f"metadata.{key.strip()}"] = value.strip().strip('"\'')

    return data


def is_external_link(target: str) -> bool:
    return bool(URL_RE.match(target)) or target.startswith("mailto:")


def check_markdown_links(
    root: Path,
    skill_md: Path,
    text: str,
    result: CheckResult,
) -> None:
    rel = repo_relative(root, skill_md)
    for match in LINK_RE.finditer(text):
        raw_target = match.group(1).strip()
        target = raw_target.split("#", 1)[0]
        if not target or is_external_link(target):
            continue
        target_path = (skill_md.parent / target).resolve()
        try:
            target_path.relative_to(root)
        except ValueError:
            result.error(f"{rel}: relative link escapes repository: {raw_target}")
            continue
        if not target_path.exists():
            result.error(f"{rel}: broken relative link: {raw_target}")


def check_referenced_paths(
    root: Path,
    skill_md: Path,
    text: str,
    result: CheckResult,
) -> None:
    rel = repo_relative(root, skill_md)
    for match in PATH_MENTION_RE.finditer(text):
        target = match.group(1)
        if not (skill_md.parent / target).exists():
            result.error(f"{rel}: referenced path missing: {target}")


def check_skill(root: Path, skill_md: Path, result: CheckResult) -> None:
    rel = repo_relative(root, skill_md)
    text = skill_md.read_text(encoding="utf-8")
    lines = text.splitlines()
    data = parse_frontmatter(text, rel, result)

    name = data.get("name", "")
    description = data.get("description", "")
    version = data.get("metadata.version", "")
    result.skills.append(
        (name or "<missing name>", rel, len(lines), version or "<missing>")
    )

    if not name:
        result.error(f"{rel}: missing name")
    if not description:
        result.error(f"{rel}: missing description")
    elif "Use when" not in description:
        result.error(f'{rel}: description must contain an explicit "Use when" trigger')
    if not version:
        result.error(f"{rel}: missing metadata.version")
    if len(lines) > MAX_SKILL_LINES:
        result.error(
            f"{rel}: SKILL.md has {len(lines)} lines; keep it <= {MAX_SKILL_LINES}"
        )

    check_markdown_links(root, skill_md, text, result)
    check_referenced_paths(root, skill_md, text, result)


def check_scripts(root: Path, skill_root: Path, result: CheckResult) -> None:
    for script in sorted(skill_root.glob("*/scripts/*")):
        if not script.is_file():
            continue
        rel = repo_relative(root, script)
        mode = script.stat().st_mode
        if script.suffix in {".py", ".sh"} and not (mode & stat.S_IXUSR):
            result.error(f"{rel}: executable script is missing owner execute bit")
        if script.suffix == ".py":
            try:
                ast.parse(script.read_text(encoding="utf-8"), filename=str(script))
            except SyntaxError as error:
                result.error(f"{rel}: Python syntax error: {error}")


def run_localization_audit(root: Path, result: CheckResult) -> None:
    audit_script = (
        root / ".opencode/skills/qtpass-localization-audit/scripts/audit_ts.py"
    )
    localization_files = sorted((root / "localization").glob("*.ts"))
    if not audit_script.exists() or not localization_files:
        result.warning(
            "localization audit skipped: script or localization/*.ts files missing"
        )
        return

    command = [sys.executable, str(audit_script), "--summary"] + [
        str(path) for path in localization_files
    ]
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)
    if completed.returncode != 0:
        result.error(f"localization audit failed with exit code {completed.returncode}")


def check_all(root: Path, include_localization_audit: bool) -> CheckResult:
    result = CheckResult()
    skill_root = root / ".opencode" / "skills"
    skills = sorted(skill_root.glob("*/SKILL.md"))
    if not skills:
        result.error("No .opencode/skills/*/SKILL.md files found")
        return result

    for skill_md in skills:
        check_skill(root, skill_md, result)
    check_scripts(root, skill_root, result)
    if include_localization_audit:
        run_localization_audit(root, result)
    return result


def print_result(result: CheckResult) -> None:
    print(f"Skills found: {len(result.skills)}")
    for name, rel, line_count, version in sorted(result.skills):
        print(f"- {name}: {rel} ({line_count} lines, metadata.version={version})")

    if result.warnings:
        print("\nWarnings:")
        for warning in result.warnings:
            print(f"WARN: {warning}")
    if result.errors:
        print("\nErrors:")
        for error in result.errors:
            print(f"ERROR: {error}")
    else:
        print("\nSkill sanity checks passed.")


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root")
    parser.add_argument(
        "--skip-localization-audit",
        action="store_true",
        help="skip audit_ts.py --summary localization/*.ts",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    result = check_all(
        root,
        include_localization_audit=not args.skip_localization_audit,
    )
    print_result(result)
    return 1 if result.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
