# Qt mnemonic conventions

A `&` before a letter marks a keyboard accelerator: `&Quit` underlines Q, so
Alt+Q triggers it. In `.ts` XML it appears as `&amp;`.

| Script family                   | Form                                                            | Example                  |
| ------------------------------- | --------------------------------------------------------------- | ------------------------ |
| Latin (de, fr, es, en, ...)     | In-word `&Letter`                                               | `&File`, `&Quit`         |
| CJK (zh, ja, ko)                | Parentheses at the start, `(&L) text`; never a native character | `(&F) 文件`, `(&Q) 退出` |
| RTL (ar, he)                    | `(&L) text` (preferred, portable) or a native-letter `&letter`  | `(&Q) <text>`            |
| Cyrillic, Greek, Tamil, Sinhala | In-word native letter if keyboards support it, else `(&L) text` | `&Покажи`                |

Rules:

- Within one menu, mnemonics must be unique.
- Keep the source letter when feasible, so the same shortcut works across
  locales. Otherwise pick a non-conflicting letter from the translated text.
- A `%` sigil instead of `&amp;` (for example `%顯示` for `&Show`) is a common
  machine-translation mistake.

To find translations that lost their mnemonic, run the Quick audit in
[qtpass-localization-audit](../../qtpass-localization-audit/SKILL.md); it holds
the list of mnemonic-bearing source strings.
