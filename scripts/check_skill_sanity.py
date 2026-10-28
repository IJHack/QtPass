#!/usr/bin/env python3
"""Deterministic sanity checks for QtPass agent skills.

subprocess only runs fixed argument lists here, never a shell (bandit B404,
B603).
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import stat
import subprocess  # nosec B404
import sys
from collections.abc import Iterable
from pathlib import Path

try:
    import yaml
except ImportError:  # reported when a frontmatter needs parsing
    yaml = None

MAX_SKILL_LINES = 220
FENCE_RE = re.compile(r"^(```|~~~).*?^\1", re.MULTILINE | re.DOTALL)
CODE_SPAN_RE = re.compile(r"`[^`\n]*`")
# [text](destination "optional title"); the destination is either <...> or
# runs up to the first whitespace or closing parenthesis.
# Each part stays on one line, and the text holds no "[": a run of "[" then
# costs a scan up to the next one, linear over the file, not a rescan of the
# rest of the line from every bracket.
LINK_TEXT = r"(?<!!)\[[^\[\]\n]*\]"
# The title starts with a non-blank, so its blanks and the separator's never
# compete for the same characters (a run of blanks with no ")" stays linear).
LINK_DEST = r"\([ \t]*(<[^>\n]*>|[^)\s]+)(?:[ \t]+[^)\s][^)\n]*)?\)"
LINK_RE = re.compile(LINK_TEXT + LINK_DEST)
PATH_MENTION_RE = re.compile(
    r"(?<![`\w/.-])((?:references|scripts)/[A-Za-z0-9_./-]+\."
    r"(?:md|py|sh|bash|txt|json|yml|yaml))"
)
URL_RE = re.compile(r"^[a-z][a-z0-9+.-]*:", re.IGNORECASE)
SHEBANG_SHELLS = ("bash", "sh", "dash", "zsh")


class CheckResult:
    """Collects errors, warnings and a per-skill summary."""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.skills: list[tuple[str, str, int, str]] = []

    def error(self, message: str) -> None:
        """Record a failure."""
        self.errors.append(message)

    def warning(self, message: str) -> None:
        """Record something worth a look that does not fail the check."""
        self.warnings.append(message)


def repo_relative(root: Path, path: Path) -> str:
    """The path relative to the repository root, with forward slashes."""
    return str(path.resolve().relative_to(root)).replace(os.sep, "/")


def strip_code(text: str) -> str:
    """Markdown without fenced blocks and code spans, where links are not links."""
    return CODE_SPAN_RE.sub("", FENCE_RE.sub("", text))


def parse_frontmatter(text: str, rel: str, result: CheckResult) -> dict:
    """The YAML frontmatter as a mapping, or {} after recording why not."""
    if not text.startswith("---\n"):
        result.error(f"{rel}: missing YAML frontmatter")
        return {}
    parts = text.split("---\n", 2)
    if len(parts) < 3:
        result.error(f"{rel}: unterminated YAML frontmatter")
        return {}
    if yaml is None:
        result.error("PyYAML is not installed: cannot parse frontmatter")
        return {}
    try:
        # No aliases: merged references can expand exponentially while the
        # mapping is built, before anything here looks at it. Checked on the
        # event stream, which builds nothing.
        if any(isinstance(event, yaml.AliasEvent) for event in yaml.parse(parts[1])):
            raise yaml.YAMLError("aliases are not allowed")
        data = yaml.safe_load(parts[1])
    except yaml.YAMLError as error:
        result.error(f"{rel}: invalid YAML frontmatter: {error}")
        return {}
    if not isinstance(data, dict):
        result.error(f"{rel}: frontmatter is not a mapping")
        return {}
    return data


def inside(path: Path, directory: Path) -> bool:
    """Whether path, symlinks resolved, lies within directory."""
    try:
        path.resolve().relative_to(directory.resolve())
    except ValueError:
        return False
    return True


def check_markdown_links(
    root: Path,
    skill_md: Path,
    text: str,
    result: CheckResult,
) -> None:
    """Every relative link resolves to a file inside the repository."""
    rel = repo_relative(root, skill_md)
    for match in LINK_RE.finditer(strip_code(text)):
        raw_target = match.group(1).strip("<>")
        target = raw_target.split("#", 1)[0]
        # Skip empty targets, URLs and protocol-relative //host links.
        if not target or URL_RE.match(target) or target.startswith("//"):
            continue
        # A leading slash is the repository root, as on GitHub.
        base = root if target.startswith("/") else skill_md.parent
        target_path = base / target.lstrip("/")
        if not inside(target_path, root):
            result.error(f"{rel}: relative link escapes repository: {raw_target}")
        elif not target_path.exists():
            result.error(f"{rel}: broken relative link: {raw_target}")


def check_referenced_paths(
    root: Path,
    skill_md: Path,
    text: str,
    result: CheckResult,
) -> None:
    """Every references/ or scripts/ path mentioned exists in the skill."""
    rel = repo_relative(root, skill_md)
    skill_dir = skill_md.parent
    for match in PATH_MENTION_RE.finditer(strip_code(text)):
        target = match.group(1)
        target_path = skill_dir / target
        if not inside(target_path, skill_dir):
            result.error(f"{rel}: referenced path leaves the skill: {target}")
        elif not target_path.exists():
            result.error(f"{rel}: referenced path missing: {target}")


def string_field(value: object, field: str, rel: str, result: CheckResult) -> str:
    """value when it is a string; "" (and an error if set at all) otherwise."""
    if value is None or isinstance(value, str):
        return value or ""
    result.error(f"{rel}: {field} must be a string, not {type(value).__name__}")
    return ""


def metadata_version(data: dict, rel: str, result: CheckResult) -> str:
    """metadata.version from the frontmatter, or an empty string."""
    metadata = data.get("metadata")
    if not isinstance(metadata, dict):
        return ""
    return string_field(metadata.get("version"), "metadata.version", rel, result)


def check_skill(root: Path, skill_md: Path, result: CheckResult) -> None:
    """Frontmatter, size, links and referenced paths of one SKILL.md."""
    rel = repo_relative(root, skill_md)
    text = skill_md.read_text(encoding="utf-8")
    lines = text.splitlines()
    data = parse_frontmatter(text, rel, result)

    name = string_field(data.get("name"), "name", rel, result)
    description = string_field(data.get("description"), "description", rel, result)
    version = metadata_version(data, rel, result)
    summary = (name or "<missing name>", rel, len(lines), version or "<missing>")
    result.skills.append(summary)

    if not name:
        result.error(f"{rel}: missing name")
    elif name != skill_md.parent.name:
        result.error(f"{rel}: name {name!r} differs from its directory")
    if not description:
        result.error(f"{rel}: missing description")
    elif "Use when" not in description:
        result.error(f'{rel}: description needs an explicit "Use when" trigger')
    if not version:
        result.error(f"{rel}: missing metadata.version")
    if len(lines) > MAX_SKILL_LINES:
        limit = f"{len(lines)} lines; keep it <= {MAX_SKILL_LINES}"
        result.error(f"{rel}: SKILL.md has {limit}")

    check_markdown_links(root, skill_md, text, result)
    check_referenced_paths(root, skill_md, text, result)


def shell_for(script: Path) -> str:
    """The shell named in the script's shebang, bash when it names none."""
    with script.open(encoding="utf-8") as handle:
        first = handle.readline()
    if first.startswith("#!"):
        names = [Path(word).name for word in first[2:].split()]
        for shell in SHEBANG_SHELLS:
            if shell in names:
                return shell
    return "bash"


def check_shell_syntax(rel: str, script: Path, result: CheckResult) -> None:
    """Parse a shell script with its own shell's -n, without running it."""
    wanted = shell_for(script)
    shell = shutil.which(wanted)
    if shell is None:
        result.warning(f"{rel}: {wanted} not found, syntax not checked")
        return
    # Fixed argv, no shell.
    completed = subprocess.run(  # nosec B603
        [shell, "-n", str(script)],
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        result.error(f"{rel}: shell syntax error: {completed.stderr.strip()}")


def check_scripts(root: Path, skill_root: Path, result: CheckResult) -> None:
    """Scripts anywhere under a skill's scripts/ are executable and compile."""
    for script in sorted(skill_root.glob("*/scripts/**/*")):
        if not script.is_file() or script.suffix not in {".py", ".sh", ".bash"}:
            continue
        rel = repo_relative(root, script)
        if not script.stat().st_mode & stat.S_IXUSR:
            result.error(f"{rel}: executable script is missing owner execute bit")
        if script.suffix in {".sh", ".bash"}:
            check_shell_syntax(rel, script, result)
            continue
        try:
            compile(script.read_text(encoding="utf-8"), str(script), "exec")
        except SyntaxError as error:
            result.error(f"{rel}: Python does not compile: {error}")


def run_localization_audit(root: Path, result: CheckResult) -> None:
    """Run audit_ts.py --summary over localization/*.ts; failure is an error."""
    skill = root / ".opencode/skills/qtpass-localization-audit"
    audit_script = skill / "scripts/audit_ts.py"
    localization_files = sorted((root / "localization").glob("*.ts"))
    if not audit_script.exists() or not localization_files:
        result.error("localization audit missing audit_ts.py or localization/*.ts")
        return

    command = [sys.executable, str(audit_script), "--summary"]
    command += [str(path) for path in localization_files]
    # Fixed argv, no shell.
    completed = subprocess.run(  # nosec B603
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
        result.error(f"localization audit failed (exit {completed.returncode})")


def check_all(root: Path, include_localization_audit: bool) -> CheckResult:
    """Run every check over .opencode/skills and collect the results."""
    result = CheckResult()
    skill_root = root / ".opencode" / "skills"
    skill_dirs = sorted(d for d in skill_root.glob("*") if d.is_dir())
    if not skill_dirs:
        result.error("No skill directories under .opencode/skills")
        return result

    for skill_dir in skill_dirs:
        skill_md = skill_dir / "SKILL.md"
        if skill_md.is_file():
            check_skill(root, skill_md, result)
        else:
            result.error(f"{repo_relative(root, skill_dir)}: missing SKILL.md")
    check_scripts(root, skill_root, result)
    if include_localization_audit:
        run_localization_audit(root, result)
    return result


def print_result(result: CheckResult) -> None:
    """Print the per-skill summary, then warnings and errors."""
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
    """Parse the arguments, run the checks, return 1 on any error."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repo root")
    parser.add_argument(
        "--skip-localization-audit",
        action="store_true",
        help="skip audit_ts.py --summary localization/*.ts",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    skip = args.skip_localization_audit
    result = check_all(root, include_localization_audit=not skip)
    print_result(result)
    return 1 if result.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
