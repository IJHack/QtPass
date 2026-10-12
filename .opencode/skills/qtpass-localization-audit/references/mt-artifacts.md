# Machine-translation artifact patterns

The audit script finds structural breakage. These semantic problems pass it and
need a reader. All of them were caught in real machine-translated contributions
to QtPass.

| Pattern                      | Example                                                                                                                                           |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| Wrong-meaning calque         | lv `Atgriezt paroli` ("Return password") for "Repeat password"; `Iekrētāja lekts` (a non-word) for "Clipboard cleared"                            |
| Placeholder corruption       | `% 1` with a space: Qt does not substitute it and shows the literal text (the script reports this as PH)                                          |
| Cross-language loanword      | Croatian `sadržju` ("content") in the Slovenian file; Latvian `Parole` in the Lithuanian file                                                     |
| Made-up word forms           | lv `Pārencēšana` for "re-encryption" (correct: `Pāršifrēšana`); `būtošana` (built from `būt`, "to be") for "status"                               |
| Wrong word association       | si `පොලීසිය` ("police") for "search"; `මුදල්` / `මූල්‍ය` ("money", "financial") for unrelated concepts; `හාර්ය` ("wife") for "key"                |
| Repetition glitch            | lv `kopkopu` ("duplicate-duplicate") for "backup"; one fragment repeated three times in a single sl string                                        |
| Wrong mnemonic sigil         | `%顯示` instead of `&amp;顯示` for `&Show` (the script reports this as MN)                                                                        |
| Tooling delimiter leak       | translation ends with a stray `},{` from the MT pipeline                                                                                          |
| Untranslated word mid-script | `&lt;b&gt;Clipboard&lt;/b&gt;` left in English inside an otherwise Sinhala paragraph                                                              |
| Broken generator alphabet    | hyphens inserted (`A-Z-...`) or source letters replaced (`abcdeļš...`); the script reports both as ALPHA                                          |
| Word from a sibling language | Slovenian and Croatian or Serbian share many words, so a `--leak` hit in hr or sr is often correct; confirm with a native reader before reverting |

## Notes on the generator alphabet

The source `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789` is
the default character set of the password generator. Many finished locales
(da, de_DE, fr_FR, pl, ru, uk and others) append their own letters, which is
accepted. A translation must still contain every source character and no
separators.

## Reading a suspicious string

1. Compare the translation with the source and its `<location>` in the code
   (`grep -n` the source text under `src/`) to see where it shows up.
2. Back-translate unknown words with a dictionary, not with the same MT engine
   that produced them.
3. When unsure, revert to an empty `<translation type="unfinished"></translation>`
   so a Weblate translator picks it up. An empty string falls back to English,
   which is better than a wrong word.
