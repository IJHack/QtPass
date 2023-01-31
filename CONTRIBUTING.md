# Contributing

Make sure you have read the [FAQ](FAQ.md)

Thank you for wanting to contribute to making QtPass awesome.

## This document

This document is still in a very early stage and needs a lot more work.

## Pull Request Process

1. Ensure install or build dependencies and artefacts are not commmitted.
2. When adding big new features or changes to the build system, update the [README.md](README.md) to reflect those.
3. Make sure you update all of the CI configs if need be. These are ran on every Pull Request.
3. Increase the version numbers in relevant files when applicable.
   The versioning scheme we use is [SemVer](http://semver.org/).
4. You may merge the Pull Request in once you have the sign-off of one other developer, or if you
   do not have permission to do that, you may request a reviewer to merge it for you.

## Translations

* Add you language to the `src/src.pro` file
  under TRANSLATIONS.
* Next run the command `qmake` which will create and update the localization files.
* Edit your file with (let's imagine your language is sv_SE (Swedish)
  `linguist localization/localization_sv_SE.ts`

Qt Linguist has very nice in-context translation options [for translators](https://doc.qt.io/qt-5/linguist-translators.html)

You can do online translations via [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/)

## IRC

For questions or brainstorming about features please join #ijhack on freenode.

## Gitter

Or if you prefer to use [gitter](https://gitter.im/IJHack/qtpass)

## License

QtPass is released under the GNU GPL v3.0 license.
<http://www.gnu.org/licenses/gpl-3.0.html>
