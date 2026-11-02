# fill_ts.py review codes

`python3 .opencode/skills/qtpass-localization/scripts/fill_ts.py review` prints
one line per entry with a finding, then the translation (`->`) and, with
`--back`, the model's English back-translation (`<-`). `--revert CODES` empties
the entries with a finding that starts with one of the codes (`all` for every
finding); the rest are only reported.

`--back` asks a different model than the one that filled the text:
`qwen3.5:9b` by default, about 3 s a string on FastFlowLM. The model that
wrote a translation reads its own mistakes back the way it meant them, so
review refuses it (`--fill-model` names the fill model, `translategemma:4b` by
default). Even so, a clean review is a machine check, not a reading: in a PR,
call only the locales a person read "read".

| Code           | Fires when                                                                |
| -------------- | ------------------------------------------------------------------------- |
| `term`         | a glossary term of the source is missing (store is Opslag, not winkel)    |
| `register`     | formal address in an informal file, or the other way (nl, de, fr, es)     |
| `quotes`       | fewer quotation marks than the source has                                 |
| `numbers`      | other numbers than the source has                                         |
| `ending`       | the source ends with `:`, `…` or `?` and the translation does not, or so  |
| `sentences`    | another number of sentences                                               |
| `untranslated` | the source copied, three words or more                                    |
| `negation`     | `--back`: the back-translation lost or gained a "not"                     |
| `alarm`        | `--back`: it says "error", "wrong" or "problem" where the source does not |
| `drift`        | `--back`: a source of four words or more kept less than half of them      |

The fill checks (`placeholders`, `html`, `line breaks`, `shortcut`,
`script`, `latin words`) are reported too. A back-translation is noisy on
short strings ("Klembord hersteld" came back as "Whiteboard repaired"
from both models), so
read the `<-` line before you revert on `drift`. `alarm` is the one to trust:
"That errs on the hidden side" became "That is an error" in three languages.

## When to revert

- `alarm`, `negation`: revert unless the `<-` line shows the model, not the
  translation, is wrong. These turn a hint into a warning or the opposite.
- `term`, `register`: fix when you read the language, else leave them for
  Weblate; an inflected term (Finnish, Polish) can be a false finding.
- `drift`: read the `<-` line first. A 4B model back-translates loosely.
- `sentences`, `quotes`, `ending`, `numbers`: usually a small slip; fix it by
  hand when the fix is obvious.
