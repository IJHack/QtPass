# Contributing

Make sure you have read the [FAQ](FAQ.md)

Thank you for wanting to contribute to making QtPass awesome.

## This document

This document is still in a very early stage and needs a lot more work.

## Translations

* Add you language to the `qtpass.pro` file
  under TRANSLATIONS and in the `resources.qrc` file.
* Next run the command `lupdate qtpass.pro` which will create the localization files.
* Edit your file with (let's imagine your language is sv_SE (Swedish)
  `linguist localization/localization_sv_SE.ts`

Qt Linguist has very nice in-context translation options [for translators](https://doc.qt.io/qt-5/linguist-translators.html)

## IRC

For questions or brainstorming about features please join #ijhack on freenode.

## Gitter

Or if you prefer to use [gitter](https://gitter.im/IJHack/qtpass)
