# Contributing

Make sure you have read the [FAQ](FAQ.md)

Thank you for wanting to contribute to making QtPass awesome.

## Pull Request Process

1. **Sign your commits** - All commits must be signed with GPG using `git commit -S -m "description"`.
2. Ensure install or build dependencies and artifacts are not committed.
3. When adding big new features or changes to the build tool, update the [README.md](README.md) to reflect those.
4. Make sure you update all the CI configs if needed. These run on every Pull Request.
5. Increase the version numbers in relevant files when applicable.
   The versioning scheme we use is [PrideVer](https://pridever.org/).
6. You may merge the Pull Request once you have the sign-off of one other developer, or if you
   do not have permission to do that, you may request a reviewer to merge it for you.

## AI assistance

Using an AI coding assistant is fine. The rules:

- **Disclose it.** Add a `Co-Authored-By:` trailer naming the tool or model to each assisted commit (for example `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`), or say so in the pull request description.
- **You are the author.** Read, understand and test everything you submit; do not open a pull request with output you cannot explain. The DCO sign-off applies to assisted commits exactly as to hand-written ones.
- **Keep secrets out.** Never paste password-store contents, GPG key material or other private data into an assistant.
- **AI review findings are advisory.** CodeRabbit and Copilot comment on pull requests; verify each finding against the code, fix what is real, and answer false positives with a short explanation instead of a change.
- **Machine-drafted translations stay `type="unfinished"`** in the `.ts` files so native speakers can finalise them on Weblate.

Agents working on this repository should read [AGENTS.md](AGENTS.md) and the skills under `.opencode/skills/`.

## Translations

QtPass uses [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/) for translations.

To add a new language:

- Add your language code to `src/src.pro` under TRANSLATIONS
- If you have an existing build, run `make distclean` first (prevents stale generated files like `ui_*.h` from being included)
- Determine which qmake command your Qt 6 installation provides: run `qmake6 -v` (or `qmake -v` if `qmake6` is unavailable) and confirm it shows Qt version 6.2 or newer.
- Run that same command (`qmake6` or `qmake`) **at the repository root**; it runs `lupdate` over both `src/` and `main/` and updates every `.ts` file. Do not run `lupdate` on `src/src.pro` alone — that misses the strings in `main/`.
- Edit the `.ts` file with Qt Linguist: `linguist localization/qtpass_xx_YY.ts`

Qt Linguist has helpful [in-context translation options](https://doc.qt.io/qt-6/linguist-translators.html).

## Windows Developers: Symlink Setup

This repository contains a symlink (`.claude` -> `.opencode`). Windows developers need to enable symlink support:

**Option 1:** Enable Developer Mode on Windows 10+ (recommended)

- Go to Settings > Update & Security > For developers
- Enable "Developer Mode"

**Option 2:** Configure Git to use symlinks

```bash
git config core.symlinks true
```

Set this before cloning the repository.

**Warning:** Without symlink support enabled, `.claude` will be checked out as a regular text file containing the path ".opencode" instead of a proper symlink.

**Troubleshooting:** After cloning:

- Run `ls -l .claude` (Git Bash) or `dir .claude` (CMD) to verify it shows as a symlink, not a regular file.
- If it appears as a regular file, set `core.symlinks=true` in your Git config, then re-checkout the file or re-clone the repository to restore the proper symlink.

## Getting Help

- Open an [issue](https://github.com/IJHack/QtPass/issues) for bugs or feature requests
- Email [help@qtpass.org](mailto:help@qtpass.org) for general questions

## License

QtPass is released under the GNU GPL v3.0 license.
<https://www.gnu.org/licenses/gpl-3.0.html>
