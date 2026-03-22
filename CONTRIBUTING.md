# Contributing

Make sure you have read the [FAQ](FAQ.md)

Thank you for wanting to contribute to making QtPass awesome.

## Pull Request Process

1. Ensure install or build dependencies and artifacts are not committed.
2. When adding big new features or changes to the build tool, update the [README.md](README.md) to reflect those.
3. Make sure you update all of the CI configs if needed. These run on every Pull Request.
4. Increase the version numbers in relevant files when applicable.
   The versioning scheme we use is [SemVer](https://semver.org/).
5. You may merge the Pull Request once you have the sign-off of one other developer, or if you
   do not have permission to do that, you may request a reviewer to merge it for you.

## Translations

QtPass uses [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/) for translations.

To add a new language:

- Add your language code to `src/qtpass.pro` under TRANSLATIONS
- Run `qmake` to generate the translation files
- Edit the `.ts` file with Qt Linguist: `linguist localization/qtpass_xx_YY.ts`

Qt Linguist has helpful [in-context translation options](https://doc.qt.io/qt-5/linguist-translators.html).

## Getting Help

- Open an [issue](https://github.com/IJHack/QtPass/issues) for bugs or feature requests
- Email [help@qtpass.org](mailto:help@qtpass.org) for general questions

## License

QtPass is released under the GNU GPL v3.0 license.
<https://www.gnu.org/licenses/gpl-3.0.html>
