---
name: qtpass-localization-audit
description: QtPass localization audit - structural checks on .ts files (placeholders, HTML balance, mnemonics, mixed-script artifacts)
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: localization
---

# QtPass Localization Audit

Structural audits for `.ts` translation files. Use this **before** merging large translation contributions (machine-translated batches from Qwen, GPT, Gemini, etc.) and **after** reviewer-supplied refinements that could disturb non-obvious bits like placeholder format or HTML structure.

The audit catches issues that produce broken UI but read as plausible translations:

- Missing or malformed Qt placeholders (`%1`, `%2`).
- HTML tag imbalance — translation drops or adds `<br>`/`<strong>`/`<a>`/`<p>` etc.
- Missing keyboard mnemonics where source has `&Letter`.
- Mixed-script artifacts (Latin letters glued onto a non-Latin word mid-character).
- Wrong-string mappings (translation block doesn't match its source — copypaste error).
- Cross-language leaks (Slovenian text in a Sinhala file, etc.).

## Quick audit (single file)

The Quick audit covers **placeholders, mnemonics, HTML tag balance, and mixed-script artifacts** only.

It does **not** detect wrong-string mappings (translation block paired with the wrong source) or cross-language leaks. For those, run the scripts in [Targeted patterns](#targeted-patterns) — `Find wrong-string mappings` and `Find Slovenian leaks (or any cross-locale leak)` — or the full standard cleanup workflow below.

```bash
python3 <<'EOF'
import re, os
LOC = 'localization/localization_lv_LV.ts'   # change as needed

with open(LOC) as f:
    content = f.read()

mnemonic_sources = {
    '&amp;Use pass', 'Nati&amp;ve Git/GPG',
    '&amp;Show', '&amp;Hide', '&amp;Restore', '&amp;Quit',
    'Mi&amp;nimize', 'Ma&amp;ximize',
}

# Locale-to-script map for mixed-script detection. Add ranges as needed.
LOCALE_TO_SCRIPT_RANGES = {
    'si': '඀-෿',           # Sinhala
    'ta': '஀-௿',           # Tamil
    'hi': 'ऀ-ॿ',           # Devanagari
    'ru': 'Ѐ-ӿ', 'uk': 'Ѐ-ӿ', 'bg': 'Ѐ-ӿ', 'sr': 'Ѐ-ӿ',
    'el': 'Ͱ-Ͽ',           # Greek
    'zh': '一-鿿', 'ja': '぀-ゟ゠-ヿ一-鿿', 'ko': '가-힯',
    'ar': '؀-ۿ', 'he': '֐-׿',
}
lang = os.path.basename(LOC).replace('localization_', '').replace('.ts', '').split('_')[0]
script_range = LOCALE_TO_SCRIPT_RANGES.get(lang)  # None for Latin-script locales

ph = mn = html = mixed = wrong = 0
for m in re.finditer(
    r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>',
    content, re.DOTALL
):
    src, attrs, trans = m.group(1), m.group(2), m.group(3)
    if 'vanished' in attrs or not trans.strip():
        continue

    # 1. Placeholder integrity. %1, %2, ... must match exactly.
    # %n is the Qt plural placeholder: if source uses it, the translation
    # must use it too (once per <numerusform>), but the count is allowed
    # to differ across plural forms.
    s_ph = sorted(re.findall(r'%\d+', src))
    t_ph = sorted(re.findall(r'%\d+', trans))
    s_has_n = bool(re.search(r'%n\b', src))
    t_has_n = bool(re.search(r'%n\b', trans))
    if s_ph != t_ph or s_has_n != t_has_n or re.search(r'%\s+(?:\d|n)', trans):
        ph += 1
        print(f'  [PH]   {src[:60]}\n         -> {trans[:80]}')

    # 2. Missing mnemonics on known mnemonic-bearing sources
    if src in mnemonic_sources and not re.search(r'&amp;[^\s&;]', trans):
        mn += 1
        print(f'  [MN]   {src} -> {trans[:60]}')

    # 3. HTML tag balance (per-tag count must match source)
    for tag in ['html','body','p','br','strong','a','h3','pre','code','b','ol','li']:
        s_o = len(re.findall(rf'&lt;{tag}[ /&gt;]', src))
        s_c = len(re.findall(rf'&lt;/{tag}&gt;', src))
        t_o = len(re.findall(rf'&lt;{tag}[ /&gt;]', trans))
        t_c = len(re.findall(rf'&lt;/{tag}&gt;', trans))
        if s_o != t_o or s_c != t_c:
            html += 1
            print(f'  [HTML] {src[:40]} | <{tag}> s={s_o}/{s_c} t={t_o}/{t_c}')
            break

    # 4. Mixed-script (Latin letter glued to a non-Latin word, or vice versa).
    # Skipped automatically for Latin-script locales (script_range is None).
    if script_range:
        mixed_re = re.compile(f'[{script_range}]+[a-z]+|[a-z]+[{script_range}]+')
        if mixed_re.search(trans):
            mixed += 1
            print(f'  [MIX]  {src[:40]} | {trans[:60]}')

print(f'\nplaceholder={ph} mnemonic={mn} html={html} mixed-script={mixed}')
EOF
```

## All-files sweep

For a quick across-the-board check (no per-issue printing, just counts):

```bash
python3 <<'EOF'
import re, os, glob
mnemonic_sources = {
    '&amp;Use pass','Nati&amp;ve Git/GPG','&amp;Show','&amp;Hide',
    '&amp;Restore','&amp;Quit','Mi&amp;nimize','Ma&amp;ximize',
}
total_ph = total_mn = total_html = 0
for f in sorted(glob.glob('localization/localization_*.ts')):
    loc = os.path.basename(f).replace('localization_','').replace('.ts','')
    content = open(f).read()
    ph = mn = html = 0
    for m in re.finditer(r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        src,a,t = m.group(1), m.group(2), m.group(3)
        if 'vanished' in a or not t.strip(): continue
        s_ph = sorted(re.findall(r'%\d+', src))
        t_ph = sorted(re.findall(r'%\d+', t))
        s_n = bool(re.search(r'%n\b', src)); t_n = bool(re.search(r'%n\b', t))
        if s_ph != t_ph or s_n != t_n or re.search(r'%\s+(?:\d|n)', t): ph += 1
        if src in mnemonic_sources and not re.search(r'&amp;[^\s&;]', t): mn += 1
        for tag in ['html','body','p','br','strong','a','h3','pre','code','b','ol','li']:
            s_o = len(re.findall(rf'&lt;{tag}[ /&gt;]', src))
            s_c = len(re.findall(rf'&lt;/{tag}&gt;', src))
            t_o = len(re.findall(rf'&lt;{tag}[ /&gt;]', t))
            t_c = len(re.findall(rf'&lt;/{tag}&gt;', t))
            if s_o != t_o or s_c != t_c:
                html += 1; break
    if ph or mn or html:
        print(f'{loc:8s}  ph={ph:3d}  mn={mn:3d}  html={html:3d}')
    total_ph += ph; total_mn += mn; total_html += html
print(f'---\ntotal: ph={total_ph} mn={total_mn} html={total_html}')
EOF
```

## Common machine-translation artifact patterns

The audit finds _structural_ breakage; _semantic_ problems need eye review. Patterns we've repeatedly caught from machine-translated contributions:

- **Wrong-meaning calques**: `Atgriezt paroli` (= "Return password") for "Repeat password" in lv_LV; `Iekrētāja lekts` (≈ "Mr. [Non-word] sheet") for "Clipboard cleared".
- **Placeholder corruption**: `% 1` (with space) instead of `%1` — Qt won't substitute and renders the literal.
- **Cross-language loanwords**: Croatian `sadržju` (= "content") in a Slovenian file; Latvian `Parole` for "password" appearing in Lithuanian.
- **Made-up verb forms**: `Pārencēšana` (made-up Latvian for "re-encryption" — should be `Pāršifrēšana`); `būtošana` (made-up from `būt` = "to be") for "status".
- **Wrong word-association**: `පොලීසිය` (Sinhala for "police") for "search"; `මුදල්`/`මූල්‍ය` ("money"/"financial") for unrelated concepts; `හාර්ය` ("wife") for "key".
- **Repetition glitches**: `kopkopu` ("duplicate-duplicate") in lv_LV for "backup"; same fragment repeated 3× in a single string for sl_SI.
- **Wrong sigil for mnemonic**: `%顯示` instead of `&amp;顯示` for `&Show`.
- **JSON delimiter leak**: translation ends with stray `},{` characters (machine-translation tooling fragment).
- **Stray Latin words mid-script**: `&lt;b&gt;Clipboard&lt;/b&gt;` left untranslated inside an otherwise-Sinhala paragraph.
- **Alphabet character set translated**: the password generator's `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789` source must NOT be translated. Some MT pipelines insert hyphens (`A-Z-...`) or substitute regional letters (`abcdeļš` for the middle of the alphabet) — both break password generation.

## Standard cleanup workflow for MT contributions

When a friend-or-bot contributes a batch of machine translations:

1. **Commit the contribution as-is** (one commit, attribute properly):

   ```bash
   git checkout -b LangFromContributor upstream/main
   git checkout -- localization/localization_<lang>.ts  # in case of unrelated working-tree noise
   git add localization/localization_<lang>.ts
   git commit -S -m "i18n(<lang>): translations from <Contributor>"
   ```

2. **Run the audit** (single-file form above).
3. **For each finding, decide**:
   - Placeholder mismatch → either fix the placeholder or revert to empty `<translation type="unfinished"></translation>` for native review.
   - HTML imbalance on a long paragraph → revert to empty.
   - Mixed-script artifact → revert to empty unless one obvious fix applies.
   - Alphabet character-set mistranslation → restore source verbatim.
   - Missing mnemonic → add per Qt mnemonic conventions (see `qtpass-localization` skill).
4. **Commit the cleanup** as a second commit on the same branch (this keeps the contribution diff reviewable).
5. **Re-run the audit** to confirm 0 placeholder / 0 HTML / 0 mnemonic / 0 mixed-script.
6. **`lrelease6` smoke test**:

   ```bash
   lrelease6 localization/localization_<lang>.ts | tail -3
   rm -f localization/localization_<lang>.qm
   ```

7. **Push and PR**, attributing the contributor in the commit body / PR description.

## Targeted patterns

The patterns below cover checks the [Quick audit](#quick-audit-single-file) intentionally skips — wrong-string mappings, cross-language leaks, and verbatim-untranslated source fallback. Run them when reviewing a translation contribution or before merging a Weblate sync.

### Find verbatim-untranslated entries (translation == source)

Useful for scanning whether a file has actual coverage or is mostly source-fallback. Filter for proper-noun / technical-token false positives:

```python
import re, glob
ignore = {
    'QtPass','GPG','Git','OTP','PWGen','pass','Pass','Email','%1','%2','…','⌕',
    'Aa','LTR','Ctrl+G','Ctrl+N','PGP','GnuPG','YubiKey','Push','Git push','Git pull',
    'gpg','git','Git:','qrencode','pwgen',
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789',
    'login\nURL\ne-mail',
    'QProcess::FailedToStart','QProcess::Crashed','QProcess::Timedout',
    'QProcess::ReadError','QProcess::WriteError','QProcess::UnknownError',
}
for f in sorted(glob.glob('localization/localization_*.ts')):
    content = open(f).read()
    for m in re.finditer(r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        src,a,t = m.group(1), m.group(2), m.group(3)
        if 'vanished' in a or not t.strip(): continue
        if src.strip() == t.strip() and src not in ignore and 'href' not in src and len(src) > 2:
            print(f'{f}: {src[:60]!r}')
```

### Find Slovenian leaks (or any cross-locale leak)

Replace the keyword set with distinguishing words from the _unintended_ language:

```python
# Detect Slovenian leaks in non-sl_SI files
slovenian = ['mogoče','prepričani','želite','izbrisati','odložišče','počiščeno',
             'ključa','obesku','zagon','uspel','nepričakovano']
import re, glob
for f in sorted(glob.glob('localization/localization_*.ts')):
    if 'sl_SI' in f: continue
    content = open(f).read()
    for m in re.finditer(r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        src,a,t = m.group(1), m.group(2), m.group(3)
        if 'vanished' in a or not t.strip(): continue
        for w in slovenian:
            if w in t.lower():
                print(f'{f}: {src[:50]} -> {t[:60]}')
                break
```

### Find wrong-string mappings (copypaste errors)

Look for translations whose `%`-placeholder set doesn't match the source's. Already covered by the main placeholder check, but a separate scan focused on mismatched length / radically different translation can also help:

```python
import re, glob
for f in sorted(glob.glob('localization/localization_*.ts')):
    content = open(f).read()
    for m in re.finditer(r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        src,a,t = m.group(1), m.group(2), m.group(3)
        if 'vanished' in a or not t.strip(): continue
        # Suspicious: translation 3× longer than source, or vice versa
        if len(src) > 10 and (len(t) > 3 * len(src) or len(t) * 3 < len(src)):
            print(f'{f}: len-ratio {len(src)}->{len(t)}: {src[:50]!r}')
```

## When to skip the audit

- Pure terminology rename PRs (e.g., `krātuve→glabātuve`) on a single locale: structure is preserved by definition.
- Single-string typo fixes from a known reviewer.
- Removing `type="unfinished"` (finalising) on previously-validated translations.

For everything else — especially MT-source contributions — running the audit takes ~5 seconds and reliably catches issues that would otherwise need a second PR.
