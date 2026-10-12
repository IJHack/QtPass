# Translation status and runtime checks

## Per-locale completion

Re-derive the locale list at the moment of work; new locales land regularly:

```bash
ls localization/localization_*.ts | sed 's|.*localization_||;s|\.ts$||'
```

Finished, unfinished-with-text and empty counts per locale:

```bash
python3 -c "
import re, glob, os
for f in sorted(glob.glob('localization/localization_*.ts')):
    loc = os.path.basename(f)[len('localization_'):-len('.ts')]
    fin = ut = ue = 0
    for m in re.finditer(r'<translation([^>]*)>(.*?)</translation>', open(f).read(), re.DOTALL):
        a, b = m.group(1), m.group(2)
        if 'vanished' in a or 'obsolete' in a: continue
        if 'unfinished' in a:
            if b.strip(): ut += 1
            else: ue += 1
        else: fin += 1
    print(f'{loc:8s}  {fin:3d}/{fin+ut+ue}  ut={ut:3d}  empty={ue}')
"
```

`ut` (unfinished with text) means a translation exists but waits for
native-speaker review on Weblate. For one file, `lrelease6 <file> -qm
"$(mktemp --suffix=.qm)"` prints the same finished/unfinished split.

Note: `numerusform` plurals are counted once per message here, so totals can
differ slightly from what Weblate shows.

## See a translation in the running app

QtPass has no language setting: `main/main.cpp` loads the `.qm` for
`QLocale::system()` from the embedded `:/localization` resource. Override the
locale from the environment:

```bash
LANGUAGE=nl LC_ALL=nl_NL.UTF-8 ./main/qtpass
```

(On macOS the binary is `main/QtPass.app`; use the system language settings.)
A right-to-left locale also flips the layout, because the translation of the
`LTR` string is `RTL`.

## String still shows in English

1. The source is not wrapped in `tr()`: fix the code, then `make lupdate`.
2. The locale is missing from `TRANSLATIONS +=` in `src/src.pro`, so no `.qm`
   is embedded.
3. The entry is empty or still `vanished` in the `.ts`.
4. The binary predates the `.ts` change: rebuild, since `.qm` files are
   generated at build time.
5. The environment locale is not the one you expect: check `locale`.

`tests/auto/locale/tst_locale` covers exact-match loading, language fallback
(for example `ar_MA` to `ar`), unknown locales and layout direction.
