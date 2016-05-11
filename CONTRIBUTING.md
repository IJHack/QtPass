# Contributing

Make sure you have read the [FAQ](doc/FAQ.md)

Thank you for wanting to contribute to making QtPass awesome.

## This document

This document is stil in a very early stage and needs a lot more work.

## Translations

* Add you language to the `qtpass.pro` file
  under TRANSLATIONS and in the `resources.qrc` file.
* Next run the command `lupdate qtpass.pro` which will create the localization files.
* Edit your file with (let's imagine your language is sv_SE (Swedish)
  `linguist localization/localization_sv_SE.ts`

Qt Linguist has very nice in-context translation options [for translators](https://doc-snapshots.qt.io/qt5-5.6/linguist-translators.html)

## IRC

For questions or brainstorming about features please join #ijhack on freenode.
