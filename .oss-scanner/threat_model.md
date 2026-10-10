<!-- SPDX-FileCopyrightText: 2026 Anne Jan Brouwer -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Threat model

## What QtPass does and where untrusted input enters

QtPass is a Qt desktop GUI for [pass](https://www.passwordstore.org/): a
password store is a directory of GnuPG-encrypted files, usually a Git repository
shared between people. QtPass either runs `pass` (RealPass) or drives `gpg` and
`git` itself (ImitatePass). Secrets are decrypted on demand and shown,
copied to the clipboard or edited.

The user and their account are trusted. Untrusted input comes from:

- **The contents of a shared store** pulled with Git: other writers control
  file and folder names, symlinks and junctions inside the store, `.gpg-id`
  recipient lists (and `.gpg-id.sig`), `.templates`, and the decrypted text of
  entries (fields, URLs, `otpauth://` URIs).
- **Output of `gpg`, `git` and `pass`**, which QtPass parses (key listings, status
  lines, error text) and partly displays.
- **The local single-instance socket** (another process running as the user
  can send commands).
- **Settings files** only to the extent that a shared or synced config could be
  written by someone else; normally they are the user's own.

## Components that matter most

- Building command lines for `gpg`, `git`, `pass` and WSL (`src/imitatepass.cpp`,
  `src/realpass.cpp`, `src/pass.cpp`, `src/executor.cpp`): argument injection,
  option injection through filenames, shell use.
- The store boundary and writes into the store (`src/pathvalidator.cpp`,
  `src/util.cpp` staged writes, `src/storemodel.cpp`): following a link out of the
  store, writing or deleting outside it, TOCTOU swaps.
- Recipient handling (`src/imitatepass.cpp`, `src/gpgidsigner.cpp`,
  `src/gpgidgeneration.cpp`): encrypting to keys the user did not choose,
  accepting an unsigned or rolled-back `.gpg-id` when signing is configured.
- Rendering decrypted content (`src/passworddisplaypanel.cpp`, `src/qtpass.cpp`):
  HTML/link injection in rich text, opening non-http(s) URLs.
- Where secrets go: clipboard and its autoclear (`src/clipboardmanager.cpp`), logs
  and the process output panel (secrets must never be logged or shown there),
  temporary files.

Less important: layout and theming code, translations (`localization/`), the
site and documentation, CI configuration and release scripts.

## How to exercise it

`qmake6 -r && make -j && make -k check` builds everything and runs 36 Qt Test
suites under `tests/auto/`, offscreen (`QT_QPA_PLATFORM=offscreen`). The
integration and imitatepass suites run real `gpg` with a throwaway `GNUPGHOME`
and fake `git`/`gpg` scripts; `tests/auto/integration` drives whole flows. Run the
tests as a normal user, not root: root ignores the permission and path failures
several tests set up. The application binary is `main/qtpass`.

## How we rate severity

- Critical: decrypted secrets or private-key material leaving the user's control
  (to another user, a remote, a world-readable file, logs), command execution
  triggered by store contents, encrypting entries to an attacker's key without
  the user choosing it.
- High: writing, overwriting or deleting files outside the store from store
  contents; bypassing `.gpg-id` signature or generation checks when signing is
  configured; a typed entry silently lost or saved to the wrong place.
- Medium: secrets left on the clipboard or on disk longer than configured;
  rich-text or link injection without code execution; crashes or hangs caused by
  a malicious shared store.
- Low: crashes from malformed local settings, issues needing the attacker to
  already control the user's account or home directory (out of scope beyond
  that).

Memory-safety bugs count by impact; without a demonstrated path from untrusted
input they are at most medium.

## Anything to leave alone

- Attacks that need the user's own account, a compromised `gpg`, `git` or `pass` binary,
  or a malicious gpg-agent.
- The 1.8 maintenance branch only receives backports; scan `main`.
