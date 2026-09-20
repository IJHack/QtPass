# Changelog

## [Unreleased] — 2.0

First batch of the verified 2.0 backlog ([#1682](https://github.com/IJHack/QtPass/issues/1682), umbrella [#908](https://github.com/IJHack/QtPass/issues/908)).

### Upgrade Notes

- **Qt 6.8 or newer.** Qt 5 support ended with 1.8; `qmake` refuses older Qt
  with a clear message. Packagers building 1.8.x against Qt 5 need the
  `1.8` branch for that.
- **Ctrl+Q quits.** It used to close the window, which with "hide on close"
  meant hiding to the tray; that is Ctrl+W (File ▸ Close window) now. The
  window close button and Alt+F4 behave as before.
- **The menu bar is off by default** so the window looks as it did; Ctrl+M
  or Configuration ▸ General ▸ Show menu bar turns it on and the choice is
  remembered. On macOS the bar is the system's and always there.
- **Generating a GPG key asks for a passphrase.** Leaving the fields empty
  used to produce an unprotected private key silently; now OK waits until a
  passphrase is typed twice or "No passphrase" is ticked on purpose.
- **A link inside the store is not part of it.** A symbolic link or NTFS
  junction found in the store (a shared repository can carry one) is skipped
  by re-encryption and search and refused by show, edit, add, move, copy and
  re-key, with a message; deleting one removes the link. The configured
  store root itself may still be a link. See SECURITY.md.
- **Debug output** comes from the `qtpass` logging category in every build:
  `QT_LOGGING_RULES="qtpass.debug=true"` turns it on, no debug build needed
  (see the FAQ). The `#ifdef QT_DEBUG` tracing is gone.
- **Linux AppImage**: new, attached to every release; needs glibc 2.39 or
  newer (Ubuntu 24.04, Debian 13, Fedora 40). It bundles Qt only and uses the
  system's `pass`, `gpg` and `git`.
- **Packagers**: the AppStream metainfo installs as
  `org.qtpass.QtPass.metainfo.xml` (was `qtpass.appdata.xml`); `main/main.pro`
  installs the desktop file, metainfo, icons and man page; the RPM spec
  follows; the Flatpak manifest drops `rename-appdata-file` with the 2.0 tag.
- Still true from 1.8: the macOS `.dmg` is not signed or notarized (see the
  [macOS page](https://qtpass.org/macos), [#1542](https://github.com/IJHack/QtPass/issues/1542))
  and the Windows installer is not code-signed ([#1643](https://github.com/IJHack/QtPass/issues/1643)).

### New Features

- Field names in the password dialog can be edited in place: double-click a
  `key: value` field's label (or use Rename in its context menu) to change
  the key; the × at the end of the value, or the context menu, removes the
  field. Applies to the fields
  "Show all fields templated" creates from the entry; the template's own
  fields keep their names (#132)
- A Linux AppImage (`QtPass-x.y.z-x86_64.AppImage`) is built by CI and
  attached to every release next to the Windows installer and the macOS dmg.
  It bundles Qt only and uses the `pass`, `gpg` and `git` already on the
  system, so the existing store, pinentry and smartcard keep working with no
  sandbox permissions to grant. Needs glibc 2.39 or newer (Ubuntu 24.04,
  Debian 13, Fedora 40) [#1803](https://github.com/IJHack/QtPass/pull/1803).
  It runs natively on Wayland as well as X11

### Security

- A link inside the store is not part of it, at every operation and not
  only in the walks: show, edit, add, move, copy and re-key refuse an entry
  or folder that is, or lies behind, a symbolic link or NTFS junction,
  before `gpg`, `pass` or `git` is asked anything, with both backends; deleting
  a link removes the link and nothing behind it. A linked `.gpg-id` is not
  a folder's recipient list (the parent's applies, a linked root list reads
  as missing), and a linked `.gpg-id` or `.gpg-id.sig` is never handed to
  `gpg --verify`. Re-keying a folder refuses a link planted under its
  `.gpg-id` or `.gpg-id.sig` name (QSaveFile, `pass init` and `gpg --output`
  all write through one), a copy onto a folder refuses a link under the
  resulting filename, and a linked folder is unlinked by QtPass itself
  rather than handed to `pass rm -rf` or `git rm -rf`, which would empty the
  target or fail on the trailing separator. A refused operation releases the
  interface like any failed one. With the `pass` backend, Search no longer
  runs `pass grep`, whose `find -L` follows a link out of the store and
  decrypts what it finds there; both backends use the same native search
  over real files now, with the same pattern semantics. A shared store's co-writer can make
  `git pull` create such links; SECURITY.md now says what QtPass does with
  them and that the configured store root itself may be a link [#1842](https://github.com/IJHack/QtPass/issues/1842)
- A new profile's `.gpg-id` is written the way the Users dialog writes one:
  to a temporary in the same folder, owner-only, renamed into place whole,
  so an interrupted first run leaves no half list for the signing step or a
  later start to take for the recipients. And the lookup of the `.gpg-id`
  that governs a file treats "inside the store" as a path boundary: a
  sibling folder whose name merely begins with the store's is outside it [#1842](https://github.com/IJHack/QtPass/issues/1842)
- CI: the Flatpak build container, which runs privileged on every pull
  request, is pinned by image digest instead of a mutable tag; the Doxygen
  archive the documentation job downloads (from GitHub or the doxygen.nl
  mirror) must match a recorded SHA-256 before it is unpacked and run; and
  the documentation workflow builds and tests with a read-only token,
  handing the result to a separate deploy job that alone holds
  `contents: write` and runs nothing from the repository [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Generating a GPG key without a passphrase is an explicit choice: the
  key-generation dialog's OK stays off with empty passphrase fields until a
  passphrase is typed twice or "No passphrase: store the private key
  unprotected" is ticked, which clears and disables the fields. Two empty
  fields used to produce an unprotected key without a word [#1842](https://github.com/IJHack/QtPass/issues/1842)
- The executor test suite runs on Windows CI too; it had been left out of
  the Windows build since 2018, so the WSL command parser, the bundled-binary
  lookup and the gpgconf resolution were never exercised where they matter
  [#1842](https://github.com/IJHack/QtPass/issues/1842)
- A signed `.gpg-id` is verified and parsed from the same bytes: the
  signature check used to run on the path and the recipient list was read
  from the file afterwards, so anyone able to write to the store in between
  could choose the recipients a new or re-encrypted entry was encrypted to.
  `gpg --verify` now gets the data on stdin, and the re-encryption run
  remembers the verified list per `.gpg-id` [#1842](https://github.com/IJHack/QtPass/issues/1842)
- `.gpg-id` and `.gpg-id.sig` land in one commit, and when that commit fails
  the re-encryption does not start; before, the signature followed in a
  second commit (or not at all) and the store was re-encrypted to a list the
  repository did not have [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Re-encryption stops when the automatic pull before it left unmerged files,
  instead of committing on top of a half-merged store; a pull that only failed
  to reach the remote still continues with the local store [#1842](https://github.com/IJHack/QtPass/issues/1842)
- `.gpg-id` is written atomically and a failed write stops the operation;
  a full disk used to leave a truncated recipient list that was then signed
  and encrypted to [#1842](https://github.com/IJHack/QtPass/issues/1842)
- The profile form only accepts full key fingerprints (40 or 64 hexadecimal
  characters) as signing key, which is what `pass` compares against gpg's
  VALIDSIG; a short key ID would sign but never verify [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Process output reaches the generic listeners (the process output panel)
  by allow list: a process kind that nobody has classified stays silent,
  where it used to be broadcast until someone remembered to exclude it. The
  comment had claimed the opposite of what the code did [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Re-encryption treats only regular files as entries: a symlinked `.gpg` is
  skipped (and counted in a status message) rather than followed, a symlinked
  directory is not descended into, and a symlink found under a backup's name
  is reported instead of renamed into place, where the run would have
  decrypted and rewritten whatever it pointed to [#1842](https://github.com/IJHack/QtPass/issues/1842)
- NTFS junctions get the same treatment as symlinks: Qt does not report a
  junction as a link, so `QDir::NoSymLinks` let one through and a recursive
  listing walked into it. Every walk over the store (re-encryption and its
  leftover recovery, the native search, the first-run entry count, the files
  staged by a fresh `git init`, the folder list of a new entry, the check
  before a folder is deleted) now visits real, visible directories only, in
  name order, and reports what it left out: a linked folder, or a link or
  special file under an entry's name. Deleting a folder without Git no
  longer goes through `QDir::removeRecursively()`, which walked into a
  junction on Windows and, given the trailing separator the tree uses to
  name a folder, into a symlinked folder anywhere, and emptied the target.
  A folder picked in the tree that is a link is refused for re-encryption
  and recipient changes, and deleting one removes the link only. The
  configured store root itself may still be a link: that is the user's
  setup, not something found inside the store [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Re-encryption checks its own bookkeeping: when the new ciphertext cannot
  be put in place and the original cannot be put back either, the message
  says where the original still is; a backup that cannot be removed after
  success is mentioned instead of ignored; and before a run, leftovers of an
  interrupted one are dealt with — a stale temporary is removed, a backup
  whose original is missing is restored, a backup next to a present original
  is reported and left for the user to decide [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Re-encryption writes the new ciphertext to a temporary file it created
  itself, exclusively and with an unguessable name, instead of a fixed
  `<file>.reencrypt.tmp` that anyone able to write to the store could
  pre-create [#1842](https://github.com/IJHack/QtPass/issues/1842)
- The debug log redacts the values of `--passphrase` and friends and
  `otpauth://` URIs should they ever appear in a command line; a `.gpg-id`
  that is not valid UTF-8 is refused rather than verified as something else,
  and gpg's VALIDSIG line is understood with 64-character fingerprints too [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Every GitHub Action in every workflow is pinned to a commit, with the
  version as a comment for Dependabot; the build, CodeQL, docs, lint, REUSE
  and publiccode workflows still referenced tags, which a compromised
  upstream can move [#1842](https://github.com/IJHack/QtPass/issues/1842)
- Every path handed to `git` or `gpg` as a positional argument comes after
  `--`, so a name starting with a dash can never be read as an option; the
  paths QtPass builds are absolute today, this makes the rule explicit [#1842](https://github.com/IJHack/QtPass/issues/1842)
- A key's fingerprint is only taken from gpg's `fpr` record when it is
  shaped like one (40 or 64 hexadecimal characters) [#1842](https://github.com/IJHack/QtPass/issues/1842)
- WSL commands run through `wsl --exec` instead of the distribution's login
  shell, so entry paths, `.gpg-id` recipients and commit messages are no
  longer word-split or `$()`-expanded [#1686](https://github.com/IJHack/QtPass/pull/1686)
- One definition of "a WSL command": `wsl` or `wsl.exe` in any case, bare or
  as a path, with wsl.exe options such as `-d Debian` or `-u me` before the
  program, is parsed in one place and always started through `--exec` with
  those options kept: process start, `wslpath` translation, the gpgconf
  lookup, the validity probes. On Windows a bare launcher is started as
  `wsl`; elsewhere (QtPass inside WSL) it is started as written. Before, `wsl.exe gpg` was accepted by the
  gpgconf lookup but never started at all, and a `-d` option ran nothing.
  Anything that is not exactly one program (`wsl sh -c …`) is not a WSL
  command and is started as written, which fails visibly rather than
  reaching a shell [#1842](https://github.com/IJHack/QtPass/issues/1842)
- The key-generation dialog keeps the passphrase out of the always-visible
  batch template; it is spliced in only when the key is generated, and batch
  keywords are matched the way gpg does [#1687](https://github.com/IJHack/QtPass/pull/1687), [#1694](https://github.com/IJHack/QtPass/pull/1694)
- The re-encryption backup commit only stages tracked files, so a stray
  plaintext export or editor swap file in the store is no longer committed
  and auto-pushed [#1685](https://github.com/IJHack/QtPass/pull/1685)
- New folders never get an unsigned `.gpg-id` when a signing key is
  configured [#1695](https://github.com/IJHack/QtPass/pull/1695)
- The Users dialog for a folder without a `.gpg-id` (first-run wizard, new
  profile) came up with every key in the keyring pre-selected, because an
  empty recipient list made `gpg --list-keys` return all of them. Nothing is
  pre-selected now

### Changed

- CI workflow and job names follow one scheme: the workflow says what it is
  for (Build and test, Lint, Documentation, Release installers, CodeQL,
  Flatpak, FreeBSD, REUSE, publiccode.yml) and every job says what it does
  and where ("Build and test on ubuntu-latest (Qt 6.8)", "Lint codebase",
  "Build Windows installer"), the way the FreeBSD job already did, so the
  check list on a pull request reads without decoding job IDs
- Adding a password is one dialog: the folder (picked from the store's
  folders, the tree's current one preselected) and the name sit above the
  password and fields, and OK stays off while the name is empty, taken,
  a folder, or would land outside the store. The separate "New file" prompt
  is gone; a name like `work/vpn` still creates the folder it needs
- The first start is a wizard: Programs (GnuPG required, pass and Git
  optional, with the autodetected paths filled in), Your key (the secret keys
  gpg has, with a "Generate a new key pair" button; tick the ones a new store
  should be encrypted to), Password store (an existing store is used as it
  is, an empty or missing folder is created and initialised on Finish) and a
  summary with the day-one preferences. It replaces the chain of message
  boxes that ran before the full configuration dialog, which sent people who
  declined "Create password-store?" into a loop and put the whole Settings
  dialog in front of them on the first launch
- The Profiles tab is a list with a form instead of a three-column table: pick
  a profile on the left, edit its name, store path (with a folder picker),
  signing key and its own Git flags on the right. The Git flags used to be
  edited through the global checkboxes on the Settings tab for whichever row
  was selected; now they sit with the profile. A profile without a name or
  path, or with a name another profile uses, keeps OK disabled and is marked
  in the list (a duplicate name used to overwrite the other profile silently)
- The `QtPass` glue object no longer reaches into the main window (it used to
  call eighteen of its methods and wire the backends to its slots); it emits
  `outputReady`, `operationFinished`, `statusMessage`, `pushRequested` and
  `entryInserted` and the window connects to them. The first-run configuration
  loop and the fresh-start state live in the window, the settings migration in
  `QtPass::init()`, which finally has a test suite (`tst_qtpass`)
- `ImitatePass` sheds two helpers: `NativeGrep` (the threaded decrypt-and-match
  search) and `GpgIdSigner` (secret-key check, detached signing and VALIDSIG
  verification of `.gpg-id`, now also used by the profile initialisation
  instead of its own copy), and holds its `simpleTransaction` as a member
  instead of inheriting it. `GpgIdSigner` gets a test suite driven by a
  scripted gpg
- The AppStream metainfo installs as `org.qtpass.QtPass.metainfo.xml`, the
  filename the spec derives from the component ID, instead of
  `qtpass.appdata.xml`. Packagers: the RPM spec follows; the Flatpak manifest
  drops `rename-appdata-file` with the first 2.0 tag
- Icon: the hairline where the shackle's straight legs met the arc is gone
  (the arc now overlaps the legs instead of butting against them); PNG, ICO
  and ICNS regenerated from the SVG
- `pass mv`/`pass cp` to a destination named `x.gpg` that did not exist yet
  produced `x.gpg.gpg`: the `.gpg` suffix was only stripped when the
  destination already existed
- A menu bar: File (add password/folder, edit, delete, quit), Store (users,
  pull, push, OTP), Settings (configuration, Ctrl+,) and Help (FAQ with F1,
  About QtPass, About Qt). Until now Push, Pull, Users and Config existed only
  as toolbar icons and there was no About at all; on macOS the standard
  application menu (Preferences, About, Quit) now appears because Qt builds
  it from the menu roles. The bar is off by default so the window stays as
  bare as it was: Ctrl+M or Configuration ▸ General ▸ Show menu bar turns it
  on (not on macOS, where the bar is the system's) and the choice is
  remembered
- Double-clicking a field in the password panel opens the entry for editing,
  as double-clicking it in the tree does
- Push and Pull are gone from the toolbar and the Store menu while Git is
  off, instead of sitting there greyed out; they are back the moment Git is
  enabled
- Ctrl+Q quits. It used to close the window, which with "hide on close"
  meant hiding to the tray; that is Ctrl+W (File ▸ Close window) now, in
  line with KeePassXC, Telegram and the KDE, GNOME and macOS guidelines,
  where Ctrl+Q/Cmd+Q always ends the application. The window close button
  and Alt+F4 behave as before ([#1788](https://github.com/IJHack/QtPass/issues/1788))
- The process output console keeps the indentation of what Git and pass
  print (rich text used to collapse the leading spaces the code carefully
  preserved)
- Debug output goes through one `qtpass` logging category instead of
  `#ifdef QT_DEBUG` blocks around a `dbg()` macro. Release builds keep it
  compiled in; `QT_LOGGING_RULES="qtpass.debug=true"` turns it on, so a user
  can attach a trace to a bug report without a debug build (see the FAQ)
- The search box matches its words literally, in order, with anything in
  between ("work mail" finds work/acme/mail); it used to be a regular
  expression, so a typed `[` or `(` silently switched filtering off
- `pass init` for a folder is given the folder relative to the store only
  when the folder is inside the store; a directory whose name merely started
  with the store's was handed a mangled path
- The key import dialog says what an import means: the key is in your
  keyring, its owner is still yours to verify
- SECURITY.md spells out the threat model: a cloned store's Git hooks run
  with your rights, path checks catch mistakes rather than a hostile local
  writer, what is and is not logged
- The manual page describes the current program (it dated from 2015 and
  still advertised WebDAV): arguments as search text and the single
  instance, the Qt options, `GNUPGHOME`, `SSH_AUTH_SOCK` and
  `QT_LOGGING_RULES`, the settings and store locations
- User-interface strings use sentence case throughout ("Export public key",
  "Password length:", "OTP code", "Rename folder to:", …); the handful that
  still capitalised Every Word were separate strings for translators, who get
  the existing translations carried over
- The configuration dialog is a sidebar of pages instead of four tabs with a
  long scrolling first one: General (window and tray, extensions), Clipboard
  (clipboard and content panel), Passwords (generation and template), Git,
  Programs and Profiles. Every setting keeps its place in the settings file
- The API docs no longer load Mermaid from a CDN (`MERMAID_RENDER_MODE = CLI`,
  nothing used it and the site's Content Security Policy blocked it), the docs
  job builds with Doxygen 1.18.0 and replaces `docs/` on `gh-pages` instead of
  leaving pages for removed sources behind

### Bugfixes

- A gpg, Git or pass executable configured as a bare name ("gpg") ran two
  different binaries: the background commands looked for it next to the
  QtPass executable only (and failed unless bundled), the blocking ones
  through `PATH`. One rule now, for both: an absolute path or `wsl …` as
  given, otherwise the copy next to QtPass if there is one, else `PATH`
- A successful key generation in the first-run wizard still ended the wizard:
  the result travelled KeygenDialog → ConfigDialog → MainWindow → QtPass →
  Pass and back through a pointer in the main window, which closed the dialog
  via reject, so `checkSecretKeys()` saw Cancel. The dialog now talks to the
  backend itself, accepts on success, and on failure shows the reason and
  gives the form back instead of vanishing
- Every decrypt result now says which entry it belongs to, and the main
  window, the Ctrl+C copy, the OTP request and the edit dialog each act only
  on the one they asked for. Before, a slower decrypt of an entry the user
  had already left could repaint the pane, be copied to the clipboard or land
  in the edit dialog; a second Ctrl+C while one was in flight was silently
  dropped
- "Automatically push" and "Automatically pull" in the settings did nothing
  since 1.8.0: [#1140](https://github.com/IJHack/QtPass/pull/1140) started
  storing them per profile and stopped writing the global keys that the
  backends and the pull-on-start actually read. Both are saved again, and
  switching profiles now applies that profile's Git flags along with its path
  and signing key
- The main window comes back where it was left. The saved position was
  discarded on every start (stale position/size keys were applied on top of
  the restored geometry, then `main()` re-centred the window on the cursor's
  screen unconditionally), and with "hide on close" nothing was ever saved.
  One `saveGeometry`/`restoreGeometry` round trip now, for the main window
  and the settings, keygen and users dialogs alike; the window is centred only
  when nothing was saved, and not on Wayland, where placement is the
  compositor's. The redundant `pos`, `size`, `maximized` and `dialog/pos|size|maximized`
  settings keys are gone
- The Users dialog claimed "Existing files will not be modified" while OK
  re-encrypts the whole folder; the help text now says so, describes entries
  by weight instead of by "black" text, and explains the `[INVALID]`,
  `[EXPIRED]` and `[PARTIAL]` markers. Expired keys were dark-red text on the
  default background, unreadable on dark themes; they get the same red badge
  as invalid keys
- A gpg, pass or Git process killed by a signal left the password pane blank;
  the pane now says which program crashed
- Right-click ▸ Users on a folder opened the recipients dialog for the folder
  last _left_-clicked instead: the folder was remembered in a member that only
  the left-click handler updated. The dialog now always uses the tree's
  current item, and the first-run wizard opens its recipients dialog itself
  instead of routing through the main window
- The Edit dialog turned every `key: value` line into a label-locked field
  since 1.8.0, also with templates off, so those keys could no longer be
  edited as text ([#1138](https://github.com/IJHack/QtPass/pull/1138) forced "all fields" on for [#132](https://github.com/IJHack/QtPass/issues/132)). Which lines become
  fields follows the settings again: the template's fields when templates are
  on, every `key: value` line only with "Template all fields", closes
  [#1766](https://github.com/IJHack/QtPass/issues/1766)
- "New folder" wrote a zero-byte `.gpg-id`, shadowing the parent recipients
  and breaking every insert in that folder; it is now seeded from the parent
  and staged in Git [#1688](https://github.com/IJHack/QtPass/pull/1688), [#1698](https://github.com/IJHack/QtPass/pull/1698)
- `.gpg-id` recipients gpg would accept (v6 fingerprints, user IDs,
  `=exact` selectors) were dropped silently and then erased on the next
  UsersDialog save; refused lines are now logged instead [#1684](https://github.com/IJHack/QtPass/pull/1684)
- The password pane showed `&amp;`, `&quot;`, `&gt;` for values containing
  `&`, `"`, `>`, and the open-in-browser tooltip did the same for URLs with
  query strings [#1683](https://github.com/IJHack/QtPass/pull/1683), [#1693](https://github.com/IJHack/QtPass/pull/1693)
- Re-encryption after a recipient change runs on a worker thread with a
  cancellable progress dialog and one aggregated error report instead of
  blocking the window and popping one modal per failed file [#1697](https://github.com/IJHack/QtPass/pull/1697)
- "Use Git" with no Git executable configured no longer wedges the command
  queue, and delete/rename/insert fall back to plain filesystem operations
  with a status message instead of silently doing nothing [#1691](https://github.com/IJHack/QtPass/pull/1691)
- Cancelling the first-run wizard quits instead of showing a half-configured
  window, and an accepted-but-invalid configuration re-asks instead of
  starting on a broken store [#1689](https://github.com/IJHack/QtPass/pull/1689), [#1696](https://github.com/IJHack/QtPass/pull/1696)
- Edits typed in the password dialog before the decrypt landed were silently
  discarded or overwritten; the dialog now stays inert until the content is
  in [#1690](https://github.com/IJHack/QtPass/pull/1690)
- The "new folder" and "rename folder" prompts were titled "New file" and
  "Rename file"
- Field templates from `.templates` were switchable only through an
  undocumented Ctrl+T, and the active template was never shown. The password
  dialog now has a Template box naming it, selecting from the box applies it,
  and Ctrl+T (owned by the dialog) still cycles
- Accessibility basics in the settings dialog and the password pane: the
  twelve field labels now point at their fields (`buddy`), the five "…"
  browse buttons and the icon-only copy, QR and show/hide buttons have an
  accessible name and a tooltip, so a screen reader no longer announces
  them as "button"
- Process output ending in a byte that is not valid UTF-8 lost that byte: the
  decoder held it back as the start of a multi-byte sequence. It is decoded
  statelessly now, so a stray Latin-1 character shows up as a replacement
  character instead of disappearing
- A queued command without a working directory of its own ran in whichever
  directory the previous command used
- The settings dialog opened at its layout minimum (659×728) and could not be
  made smaller, so a wide translation pushed OK off a 1280×720 screen. Each
  tab scrolls now, the five wide checkbox rows wrap into two columns, and the
  six bold labels that stood in for section headers are real group boxes
  (same strings, so translations carry over). The "Extensions:" header is no
  longer hidden on Windows, where OTP and content search still live under it
- Deselecting an entry when nothing had been copied flashed "Clipboard
  cleared" in the status bar (the empty tracker matched an empty selection);
  clearing with nothing tracked is silent now
- Adding a profile for a new directory initialised it through the backend
  of the _active_ store: `pass init` nested the new `.gpg-id` inside the
  active store and `git init` ran there. The recipients file, its optional
  signature and the first commit are now written directly into the new
  directory, without touching the active store
  ([#1774](https://github.com/IJHack/QtPass/issues/1774))

### Removed

- The dead packaging leftovers: `release-winstore.bat` (Qt 5
  `windeployqt --no-angle`, a hardcoded VS2017 redist path, the Windows Store
  build that never shipped), `release-zip.mak` (pinned to v1.5.1, timestamps against the
  defunct StartSSL), `key_management.bat`, the `WINSTORE` build flag with its
  Store-only GnuPG hint, and the `debian/` ignore rules for a directory that
  no longer exists. `scripts/release-mac.sh` builds the DMG with `create-dmg`
  like the release workflow, so `appdmg.json` and the pandoc RTF step are gone
- Qt 5 support. QtPass 2.x requires Qt 6.8 or newer; `qmake` refuses older
  Qt with a clear message. All `QT_VERSION` compatibility branches for Qt 5
  and the `splitCommandCompat()` / `disconnectSingleShot()` shims are gone
  ([#908](https://github.com/IJHack/QtPass/issues/908)). The floor is the
  oldest Qt the CI matrix builds (6.8 LTS), so a version that is only
  declared and never compiled cannot silently break
  [#1761](https://github.com/IJHack/QtPass/pull/1761)
- WebDAV mounting. It stored the WebDAV password in plain text in the
  configuration file, had no user interface at all, and the `fusedav` call on
  Linux and BSD had passed literal quote characters since 2020. Saving the
  settings once in 2.0 deletes the four `webDav*` keys, the plaintext password
  included. Sync the store with Git, or put it in a folder your system already
  syncs or mounts — see the FAQ entry
  [#1752](https://github.com/IJHack/QtPass/pull/1752)

## [1.8.1](https://github.com/IJHack/QtPass/tree/v1.8.1) (2026-09-15)

The security and data-loss fixes from the 2.0 branch, backported to the 1.8
line ([#1709](https://github.com/IJHack/QtPass/issues/1709)). Qt 5.15 and Qt 6 are both still supported.

### Security <!-- markdownlint-disable-line MD024 -->

- Windows/WSL: commands run through `wsl --exec` instead of the distribution's
  login shell, so entry paths, `.gpg-id` recipients and commit messages are no
  longer word-split or `$()`-expanded (a hostile shared store could run code
  inside WSL). Binaries reachable only through PATH additions in shell rc
  files are no longer found [#1723](https://github.com/IJHack/QtPass/pull/1723) (ported from [#1686](https://github.com/IJHack/QtPass/pull/1686))
- The key-generation dialog no longer shows the passphrase in clear text in
  the batch template box; it is spliced in only when the key is generated, and
  batch keywords are matched the way gpg does, so an expert-mode template
  cannot produce an unprotected key by accident [#1718](https://github.com/IJHack/QtPass/pull/1718) (ported from [#1687](https://github.com/IJHack/QtPass/pull/1687), [#1694](https://github.com/IJHack/QtPass/pull/1694))
- The re-encryption backup commit stages tracked files only, so a stray
  plaintext export or editor swap file in the store is no longer committed and
  auto-pushed; Init stages a new folder's untracked `.gpg-id` itself
  [#1713](https://github.com/IJHack/QtPass/pull/1713) (ported from [#1685](https://github.com/IJHack/QtPass/pull/1685), [#1698](https://github.com/IJHack/QtPass/pull/1698))
- Every gpg encrypt call passes `--no-encrypt-to` and `--compress-algo=none`,
  as `pass` does, so an `encrypt-to` line in the user's `gpg.conf` can no
  longer add a recipient the `.gpg-id` never listed [#1725](https://github.com/IJHack/QtPass/pull/1725) (ported from [#1720](https://github.com/IJHack/QtPass/pull/1720))
- Only launchable `http(s)` URLs become clickable links in the password pane
  and the text browser; `ssh://`, `ftp://`, `sftp://`, `webdav://` and URLs
  with embedded credentials are shown as text [#1726](https://github.com/IJHack/QtPass/pull/1726) (ported from [#1719](https://github.com/IJHack/QtPass/pull/1719))
- Single-instance IPC: a stale socket left behind by a crash no longer
  disables it permanently (launcher clicks did nothing), the socket is
  restricted to the owning user, and a launch whose forward fails opens a
  window instead of exiting silently [#1728](https://github.com/IJHack/QtPass/pull/1728) (ported from [#1721](https://github.com/IJHack/QtPass/pull/1721))
- An out-of-range `passwordCharsSelection` in the settings file is clamped to
  "All characters" when loaded instead of indexing the character-set table
  out of bounds when the Settings dialog opens [#1724](https://github.com/IJHack/QtPass/pull/1724) (ported from [#1715](https://github.com/IJHack/QtPass/pull/1715))

### Bugfixes <!-- markdownlint-disable-line MD024 -->

- "New folder" wrote a zero-byte `.gpg-id`, shadowing the parent recipients
  and breaking every insert in that folder; it is now seeded from the parent
  recipients, and never left unsigned when a signing key is configured (the
  folder then inherits the parent's signed list) [#1714](https://github.com/IJHack/QtPass/pull/1714) (ported from [#1688](https://github.com/IJHack/QtPass/pull/1688), [#1695](https://github.com/IJHack/QtPass/pull/1695))
- `.gpg-id` recipients gpg would accept (v6 fingerprints, user IDs, `=exact`
  selectors) were dropped silently and then erased on the next UsersDialog
  save; refused lines are now logged instead [#1710](https://github.com/IJHack/QtPass/pull/1710) (ported from [#1684](https://github.com/IJHack/QtPass/pull/1684))
- The password pane showed `&amp;`, `&quot;`, `&gt;` for values containing
  `&`, `"`, `>`, and the open-in-browser tooltip did the same for URLs with
  query strings; the tooltip also stays on one line [#1712](https://github.com/IJHack/QtPass/pull/1712) (ported from [#1683](https://github.com/IJHack/QtPass/pull/1683), [#1693](https://github.com/IJHack/QtPass/pull/1693))
- Edits typed in the password dialog before the decrypt landed were silently
  discarded or overwritten; the dialog now stays inert until the content is
  in, and a failed decrypt shows the gpg error instead of closing the dialog
  [#1722](https://github.com/IJHack/QtPass/pull/1722) (ported from [#1690](https://github.com/IJHack/QtPass/pull/1690))
- Cancelling the first-run wizard quits instead of showing a half-configured
  window, and an accepted-but-invalid configuration re-asks instead of
  starting on a broken store [#1717](https://github.com/IJHack/QtPass/pull/1717) (ported from [#1689](https://github.com/IJHack/QtPass/pull/1689), [#1696](https://github.com/IJHack/QtPass/pull/1696))
- "Use Git" with no Git executable configured no longer wedges the command
  queue; delete, rename, insert, copy, init, pull and push fall back to plain
  filesystem operations with a status message instead of silently doing
  nothing [#1716](https://github.com/IJHack/QtPass/pull/1716) (ported from [#1691](https://github.com/IJHack/QtPass/pull/1691))
- A configured GPG home that no longer exists is ignored with a status message
  instead of making every gpg call fail with "No secret key" (the 1.7.0 test
  suite left its temporary keyring path in the live settings when `make check`
  ran as the user), closes [#1711](https://github.com/IJHack/QtPass/issues/1711) [#1741](https://github.com/IJHack/QtPass/pull/1741) (ported from [#1740](https://github.com/IJHack/QtPass/pull/1740))
- `make check` no longer kills the developer's own gpg-agent
  [#1739](https://github.com/IJHack/QtPass/pull/1739) (ported from [#1738](https://github.com/IJHack/QtPass/pull/1738))

## [1.8.0](https://github.com/IJHack/QtPass/tree/v1.8.0) (2026-09-13)

### New Features <!-- markdownlint-disable-line MD024 -->

- Built-in TOTP (RFC 6238): one-time passwords are now generated inside QtPass
  instead of shelling out to the `pass-otp` extension, so OTP works on every
  platform and with both the `pass` and direct `gpg2`/`git` backends. Store the
  configuration as an `otpauth://` URI in the `OTP` template field; bare
  `otpauth://` lines written by `pass-otp` are still read. The selected entry
  shows a live code with a copy button and a countdown, and
  SHA-1/SHA-256/SHA-512 plus Steam Guard codes are supported [#1625](https://github.com/IJHack/QtPass/pull/1625)
- Share submenu on folders: re-encrypt, export your public key, add
  recipients, and a "What is this?" explainer [#1144](https://github.com/IJHack/QtPass/pull/1144), [#1162](https://github.com/IJHack/QtPass/pull/1162),
  closes [#422](https://github.com/IJHack/QtPass/issues/422)
- Import GPG keys from file or clipboard via the Users dialog [#1170](https://github.com/IJHack/QtPass/pull/1170),
  closes [#1167](https://github.com/IJHack/QtPass/issues/1167)
- Process output panel (dockable) with command labels, colour-coded errors,
  auto-scroll with hysteresis and a 1000-line cap [#1172](https://github.com/IJHack/QtPass/pull/1172), [#1193](https://github.com/IJHack/QtPass/pull/1193),
  closes [#252](https://github.com/IJHack/QtPass/issues/252)
- "Open in browser" button for URL fields in the password panel [#1517](https://github.com/IJHack/QtPass/pull/1517),
  closes [#1516](https://github.com/IJHack/QtPass/issues/1516)
- Manual `SSH_AUTH_SOCK` override with `gpgconf` auto-probe fallback
  [#1438](https://github.com/IJHack/QtPass/pull/1438), closes [#543](https://github.com/IJHack/QtPass/issues/543)
- Opt-in content search across decrypted entries (regular expression)
- Multiple templates via `.templates` files, auto-applied to new entries,
  Ctrl+T cycles between them [#1141](https://github.com/IJHack/QtPass/pull/1141), [#1142](https://github.com/IJHack/QtPass/pull/1142), [#1143](https://github.com/IJHack/QtPass/pull/1143)
- Git options are stored per profile [#1140](https://github.com/IJHack/QtPass/pull/1140), closes [#112](https://github.com/IJHack/QtPass/issues/112)
- All fields of an entry can be edited, not only the password [#1138](https://github.com/IJHack/QtPass/pull/1138),
  closes [#132](https://github.com/IJHack/QtPass/issues/132)
- Status bar feedback while creating a profile [#1136](https://github.com/IJHack/QtPass/pull/1136), closes [#1034](https://github.com/IJHack/QtPass/issues/1034)

### Upgrade Notes <!-- markdownlint-disable-line MD024 -->

- OTP support is now on by default, so upgrading shows live one-time-password
  codes for entries that contain an `otpauth://` secret, even if you had it off
  before. The old setting only ever gated the Unix-only `pass-otp` extension,
  so its stored value was meaningless on Windows and macOS. To turn it off,
  uncheck **Enable one-time password (OTP) support** on the **Settings** tab of
  the configuration dialog; the choice is remembered and is not re-enabled on
  later launches. The upgrade changes only this setting — no stored passwords
  are read, rewritten, or re-encrypted.
- macOS: the release `.dmg` is not signed or notarized, and the Homebrew cask
  was disabled by Homebrew on 2026-09-01 for that reason. Install the `.dmg`
  from the GitHub release and clear the quarantine flag
  (`xattr -d com.apple.quarantine /Applications/QtPass.app`). Status and how
  to help: the [macOS page on qtpass.org](https://qtpass.org/macos) and [#1542](https://github.com/IJHack/QtPass/issues/1542).
- Windows: the installer is not code-signed; SmartScreen asks for
  _More info → Run anyway_ on first start [#1643](https://github.com/IJHack/QtPass/issues/1643).
- 31 single-variant locales were renamed to language-only codes (e.g.
  `ar_MA` → `ar`); Qt's locale fallback picks them up automatically
  [#1328](https://github.com/IJHack/QtPass/pull/1328), [#1350](https://github.com/IJHack/QtPass/pull/1350)

### Security <!-- markdownlint-disable-line MD024 -->

- Path-traversal hardening for new file, rename and drag-and-drop targets
  [#1464](https://github.com/IJHack/QtPass/pull/1464)
- `.gpg-id` is written with mode 0600 [#1465](https://github.com/IJHack/QtPass/pull/1465)
- URLs are HTML-escaped in the password panel [#1584](https://github.com/IJHack/QtPass/pull/1584)
- A TOTP shared secret is never rendered in cleartext, also when stored as an
  `OTP:` field, and Ctrl+C on an `otpauth://`-only entry no longer copies the
  seed [#1625](https://github.com/IJHack/QtPass/pull/1625)

### Bugfixes <!-- markdownlint-disable-line MD024 -->

- ConfigDialog no longer silently corrupts saved settings [#1602](https://github.com/IJHack/QtPass/pull/1602)
- PasswordDialog: no content duplication or data loss on premature save
  [#1605](https://github.com/IJHack/QtPass/pull/1605)
- ImitatePass Git re-encryption used the wrong recipients and working
  directory, and its copy operation was broken [#1604](https://github.com/IJHack/QtPass/pull/1604)
- Executor could stall when a stdin-less command failed to start [#1606](https://github.com/IJHack/QtPass/pull/1606);
  crashed subprocesses no longer hang the UI [#1570](https://github.com/IJHack/QtPass/pull/1570)
- Use-after-free of the key-generation dialog pointer [#1603](https://github.com/IJHack/QtPass/pull/1603); keygen start
  failures are reported instead of hanging [#1599](https://github.com/IJHack/QtPass/pull/1599), closes [#1598](https://github.com/IJHack/QtPass/issues/1598)
- Clipboard autoclear kept tracking the right entry when navigating [#1607](https://github.com/IJHack/QtPass/pull/1607)
- The window follows light/dark theme switches at runtime (KDE day/night),
  including the toolbar, which Breeze kept in the previous theme
  [#1669](https://github.com/IJHack/QtPass/pull/1669), [#1661](https://github.com/IJHack/QtPass/pull/1661)
- A missing `qrencode` binary is reported instead of showing an empty QR
  dialog [#1659](https://github.com/IJHack/QtPass/pull/1659)
- Close button quits when hide-on-close is off and a tray icon is present
  [#1580](https://github.com/IJHack/QtPass/pull/1580)
- WSL: `wslpath` translation is a real call instead of a broken shell
  substitution [#1569](https://github.com/IJHack/QtPass/pull/1569), closes [#1509](https://github.com/IJHack/QtPass/issues/1509)
- Segfault chain on first launch (`focusInput` before init) [#1187](https://github.com/IJHack/QtPass/pull/1187)–[#1191](https://github.com/IJHack/QtPass/pull/1191)
- Process output panel obscured the central widget [#1192](https://github.com/IJHack/QtPass/pull/1192)
- Locale-aware `QTranslator::load()` so regional variants fall back correctly
  [#1362](https://github.com/IJHack/QtPass/pull/1362)
- Plural agreement in the grep status message [#1133](https://github.com/IJHack/QtPass/pull/1133), closes [#1042](https://github.com/IJHack/QtPass/issues/1042)
- Coverity and clang-tidy findings (one real bug, bulk modernize/performance)
  [#1096](https://github.com/IJHack/QtPass/pull/1096), [#1100](https://github.com/IJHack/QtPass/pull/1100), [#1432](https://github.com/IJHack/QtPass/pull/1432), [#1435](https://github.com/IJHack/QtPass/pull/1435)

### Code Quality (umbrella [#1508](https://github.com/IJHack/QtPass/issues/1508))

- Split the `Util` grab-bag into `PathValidator`, `SshAuthSock` and `TemplateIO`, and consolidated `StoreModel` drag-drop [#1514](https://github.com/IJHack/QtPass/issues/1514)
- Tightened the `Pass` interface: `beforeExecute()` hook, `Move`/`Copy` dedup, documented Grep regular-expression dialect, and `PassBackendFactory` [#1513](https://github.com/IJHack/QtPass/issues/1513)
- Decomposed `MainWindow` into `GrepSearchController`, `PasswordDisplayPanel`, a UI watchdog, and `StoreModel::rootIndexFor` [#1512](https://github.com/IJHack/QtPass/issues/1512)
- Introduced `AppSettings` + `SettingsSerializer` with a `QtPassSettings::load()`/`save()` facade, injected through the `Pass`/dialog layers; 70 dead getter/setter wrappers removed [#1511](https://github.com/IJHack/QtPass/issues/1511)
- P1 audit sweep: executor crash, StoreModel guard, `getKeysFromFile`, profile sort order, `reencryptPath` init [#1570](https://github.com/IJHack/QtPass/pull/1570), [#1571](https://github.com/IJHack/QtPass/pull/1571), [#1572](https://github.com/IJHack/QtPass/pull/1572)

### Tests

- Test suites grew from 11 to 25: widget tests for MainWindow, ConfigDialog,
  KeygenDialog, TrayIcon, UsersDialog, PasswordDisplayPanel, Import/Export key
  dialogs; unit suites for `Base32`, `TOTP`, `PassBackendFactory`, `UserInfo`,
  `ProfileInit`; GPG end-to-end coverage for multi-recipient encryption,
  per-folder re-encryption and the decrypt-and-edit GUI flow
- Tests run against an isolated settings directory instead of the user's live
  config [#1662](https://github.com/IJHack/QtPass/pull/1662)

### Localization

- 64 locales, 14 of them new since 1.7.0: Bengali, Hindi, Indonesian,
  Latvian, Lithuanian, Marathi, Persian, Punjabi, Slovenian, Swahili, Telugu,
  Thai, Urdu and Vietnamese. Most locales are complete apart from the strings
  added late in this cycle
- Hundreds of reviewer-driven corrections across sr_Cyrl, hu, cy, et, pl,
  zh_CN, gl, sv, nl and others; mnemonic and placeholder audit tooling added
- The strings added this cycle were pre-filled in 46 locales and left
  `unfinished` for native review on Weblate
  [#1665](https://github.com/IJHack/QtPass/pull/1665), [#1666](https://github.com/IJHack/QtPass/pull/1666), [#1667](https://github.com/IJHack/QtPass/pull/1667),
  [#1670](https://github.com/IJHack/QtPass/pull/1670)
- Weblate remains the place to translate: <https://hosted.weblate.org/projects/qtpass/>

### Build / CI

- macOS builds on Qt 6.11; Linux/Windows stay on Qt 6.8 LTS; Qt 5.15 still
  builds [#1600](https://github.com/IJHack/QtPass/pull/1600)
- Doxygen download resilient to doxygen.nl outages [#1538](https://github.com/IJHack/QtPass/pull/1538); zero-warning
  Doxygen enforced
- super-linter v8 (clang-format 21), commitlint, `.editorconfig`, REUSE
  compliance badge
- Dead Coverity integration removed [#1544](https://github.com/IJHack/QtPass/pull/1544)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.7.0...v1.8.0)

## [1.7.0](https://github.com/IJHack/QtPass/tree/v1.7.0) (2026-04-20)

- Qt 6.10 `beginFilterChange`/`endFilterChange` support [#1052](https://github.com/IJHack/QtPass/pull/1052)
- Spanish regional variants [#1040](https://github.com/IJHack/QtPass/pull/1040)
- More integration tests + CI GPG signing fix [#1059](https://github.com/IJHack/QtPass/pull/1059)
- Guard against out-of-bounds index in password character set [#1084](https://github.com/IJHack/QtPass/pull/1084)
- WSL case-insensitive path handling [#1065](https://github.com/IJHack/QtPass/pull/1065)
- Replace isDir/isFile bools with `ItemKind` enum in drag-and-drop [#1066](https://github.com/IJHack/QtPass/pull/1066)
- Redundant `as_const` cleanup [#1063](https://github.com/IJHack/QtPass/pull/1063)
- Reduced cyclomatic complexity in complex methods [#1055](https://github.com/IJHack/QtPass/pull/1055)
- Extract `isGrepHeaderLine` helper in pass.cpp [#1073](https://github.com/IJHack/QtPass/pull/1073)
- Rename single-letter parameters in `setLength`/`setPasswordCharTemplate` [#1086](https://github.com/IJHack/QtPass/pull/1086)
- Doxygen style standardized across headers and main.cpp [#1087](https://github.com/IJHack/QtPass/pull/1087)
- Stale `@todo` comments removed [#1081](https://github.com/IJHack/QtPass/pull/1081)
- Skill documentation for OpenCode/Claude agents [#1088](https://github.com/IJHack/QtPass/pull/1088)
- Translations updated (Swedish, Turkish, Ukrainian, Spanish)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.6.0...v1.7.0)

## [1.6.0](https://github.com/IJHack/QtPass/tree/v1.6.0) (2026-04-13)

### Highlights

- Auto-detect Git in existing password-store [#804](https://github.com/IJHack/QtPass/pull/804)
- Use ed25519 for GPG key generation when available [#790](https://github.com/IJHack/QtPass/pull/790)
- Improved GPG key parsing with new `gpgkeystate` module

### New Features <!-- markdownlint-disable-line MD024 -->

- Added auto-detect Git in existing password-store
- Use ed25519 (ECC) for GPG key generation when GPG supports it
- Added compile_commands.json generation script for IDE tooling

### Improvements

- Improved GPG key parsing with new `gpgkeystate` module [#979](https://github.com/IJHack/QtPass/pull/979)
- Improved WSL path handling for gpgconf resolution
- Re-encryption security improvements [#815](https://github.com/IJHack/QtPass/issues/815)
- Kill stale GPG agents before key generation [#815](https://github.com/IJHack/QtPass/issues/815)
- Consolidated release scripts into `scripts/` folder
- UsersDialog performance optimizations [#977](https://github.com/IJHack/QtPass/pull/977)

### Bugfixes <!-- markdownlint-disable-line MD024 -->

- Fixed path separator check in gpgconf resolution
- Fixed .gpg-id path construction for cross-platform [#780](https://github.com/IJHack/QtPass/issues/780)
- Fixed re-encryption security issues
- Fixed GPG key generation timeout handling [#815](https://github.com/IJHack/QtPass/issues/815)
- Fixed clipboard history password exclusion [#970](https://github.com/IJHack/QtPass/pull/970)
- Fixed transparent context menu in dark mode [#967](https://github.com/IJHack/QtPass/pull/967)
- Fixed window positioning with window manager [#947](https://github.com/IJHack/QtPass/pull/947)
- Fixed theme colors for button icons [#949](https://github.com/IJHack/QtPass/pull/949)
- Fixed hardcoded black text in dark mode [#946](https://github.com/IJHack/QtPass/pull/946)

### Testing

- Added `gpgkeystate` test suite (8 test suites total) [#979](https://github.com/IJHack/QtPass/pull/979)
- Added GPG colon output fixtures
- Improved test assertions and coverage [#978](https://github.com/IJHack/QtPass/pull/978), [#981](https://github.com/IJHack/QtPass/pull/981), [#982](https://github.com/IJHack/QtPass/pull/982)

### Documentation & Maintenance

- Added AGENTS.md for AI agent guidance
- Added security policy (SECURITY.md)
- Extensive doxygen documentation improvements
- CI/CD improvements and optimizations

### Localization <!-- markdownlint-disable-line MD024 -->

- Updated translations via Weblate

### Release, CI and Maintenance

- Consolidated release scripts into `scripts/` folder
- Added compile_commands.json generation script
- Extensive code quality improvements via AI-assisted review
- CI/CD workflow optimizations

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.5.1...v1.6.0)

## [1.5.1](https://github.com/IJHack/QtPass/tree/v1.5.1) (2026-03-22)

### Fixes

- Fixed crash on Wayland when screenAt() returns null [#706](https://github.com/IJHack/QtPass/issues/706), [#663](https://github.com/IJHack/QtPass/issues/663)
- Fixed CLI arguments being parsed as password search [#652](https://github.com/IJHack/QtPass/issues/652)
- Fixed OTP error handling with better messages and pass-otp availability check [#677](https://github.com/IJHack/QtPass/issues/677)
- Fixed window icon not showing on dialog boxes [#671](https://github.com/IJHack/QtPass/issues/671)
- Fixed Slovak translation GPG keygen script [#667](https://github.com/IJHack/QtPass/issues/667)
- Suppressed qApp deprecation warnings on Qt6

## [1.5.0](https://github.com/IJHack/QtPass/tree/v1.5.0) (2026-03-21)

### Highlights <!-- markdownlint-disable-line MD024 -->

- `1.5.0` release metadata and packaging preparation across the desktop app, installer, and CI configuration.
- Windows release pipeline reliability improvements for AppVeyor and Inno Setup packaging.
- Modernized C++/Qt build pipeline with clang-tidy and wider lint/test hardening.

### Fixes & Improvements

- Fixed profile handling issues in Qt6 compatibility scenarios and improved profile selection behavior [#681](https://github.com/IJHack/QtPass/pull/681), [#695](https://github.com/IJHack/QtPass/pull/695).
- Preserved existing application behavior when launching `qtpass` without parameters [#704](https://github.com/IJHack/QtPass/pull/704).
- Removed a regression where gpg_id comments could be altered [#658](https://github.com/IJHack/QtPass/pull/658).
- Added missing include path fixes and small reliability hardening in core code paths [#690](https://github.com/IJHack/QtPass/pull/690), [#716](https://github.com/IJHack/QtPass/pull/716).

### Release, CI and Maintenance <!-- markdownlint-disable-line MD024 -->

- Added/updated PublicCode and CI workflows, including release-time validation and action upgrades [#701](https://github.com/IJHack/QtPass/pull/701), [#709](https://github.com/IJHack/QtPass/pull/709), [#710](https://github.com/IJHack/QtPass/pull/710), [#711](https://github.com/IJHack/QtPass/pull/711), [#712](https://github.com/IJHack/QtPass/pull/712).
- Completed the AppVeyor/Inno Setup modernization and packaging updates [#722](https://github.com/IJHack/QtPass/pull/722).
- Updated readme status badges and branch links to current workflow targets [#724](https://github.com/IJHack/QtPass/pull/724).

### Localization <!-- markdownlint-disable-line MD024 -->

- Synchronised a large set of translation updates through Weblate and translation automation to improve language coverage and keep localization current [#659](https://github.com/IJHack/QtPass/pull/659), [#664](https://github.com/IJHack/QtPass/pull/664), [#666](https://github.com/IJHack/QtPass/pull/666), [#669](https://github.com/IJHack/QtPass/pull/669), [#670](https://github.com/IJHack/QtPass/pull/670), [#673](https://github.com/IJHack/QtPass/pull/673), [#676](https://github.com/IJHack/QtPass/pull/676), [#685](https://github.com/IJHack/QtPass/pull/685), [#689](https://github.com/IJHack/QtPass/pull/689), [#691](https://github.com/IJHack/QtPass/pull/691), [#699](https://github.com/IJHack/QtPass/pull/699), [#702](https://github.com/IJHack/QtPass/pull/702), [#703](https://github.com/IJHack/QtPass/pull/703), [#707](https://github.com/IJHack/QtPass/pull/707), [#713](https://github.com/IJHack/QtPass/pull/713), [#723](https://github.com/IJHack/QtPass/pull/723). <!-- markdownlint-disable-line MD013 -->

**New Contributors**

- @Vascom made their first contribution in [#656](https://github.com/IJHack/QtPass/pull/656).
- @shemeshg made their first contribution in [#658](https://github.com/IJHack/QtPass/pull/658).
- @ruimaciel made their first contribution in [#672](https://github.com/IJHack/QtPass/pull/672).
- @vdchuyen made their first contribution in [#680](https://github.com/IJHack/QtPass/pull/680).
- @souk4711 made their first contribution in [#681](https://github.com/IJHack/QtPass/pull/681).
- @principis made their first contribution in [#690](https://github.com/IJHack/QtPass/pull/690).
- @stkw0 made their first contribution in [#695](https://github.com/IJHack/QtPass/pull/695).
- @publiccode-pr-bot made their first contribution in [#701](https://github.com/IJHack/QtPass/pull/701).
- @transifex-integration[bot] made their first contribution in [#698](https://github.com/IJHack/QtPass/pull/698).
- @basil made their first contribution in [#704](https://github.com/IJHack/QtPass/pull/704).
- @dependabot[bot] made their first contribution in [#709](https://github.com/IJHack/QtPass/pull/709).

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.4.0...v1.5.0)

## [v1.4.0](https://github.com/IJHack/QtPass/tree/v1.4.0) (2023-09-17)

[Full Changelog](https://github.com/IJHack/QtPass/compare/1.4.0-rc2...v1.4.0)

**Fixed bugs:**

- Update site to reflect new brew command syntax [\#601](https://github.com/IJHack/QtPass/issues/601)
- QtPass - not asking for password [\#585](https://github.com/IJHack/QtPass/issues/585)
- Missing menu [\#574](https://github.com/IJHack/QtPass/issues/574)

**Merged pull requests:**

- Initial Korean from Weblate [\#655](https://github.com/IJHack/QtPass/pull/655) ([annejan](https://github.com/annejan))
- Natural language fixes [\#654](https://github.com/IJHack/QtPass/pull/654) ([annejan](https://github.com/annejan))
- Translations update from Hosted Weblate [\#650](https://github.com/IJHack/QtPass/pull/650) ([weblate](https://github.com/weblate))
- Added Serbian and Estonian to project file [\#649](https://github.com/IJHack/QtPass/pull/649) ([annejan](https://github.com/annejan))
- Translations update from Hosted Weblate [\#648](https://github.com/IJHack/QtPass/pull/648) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#647](https://github.com/IJHack/QtPass/pull/647) ([weblate](https://github.com/weblate))
- Version bump and cleanup [\#646](https://github.com/IJHack/QtPass/pull/646) ([annejan](https://github.com/annejan))

## [1.4.0-rc2](https://github.com/IJHack/QtPass/tree/1.4.0-rc2) (2023-08-31)

[Full Changelog](https://github.com/IJHack/QtPass/compare/1.4.0-rc1...1.4.0-rc2)

**Fixed bugs:**

- OTP function stopped working [\#630](https://github.com/IJHack/QtPass/issues/630)
- Cannot decrypt own passwords; No secret key [\#580](https://github.com/IJHack/QtPass/issues/580)
- gpg not found on macOS [\#575](https://github.com/IJHack/QtPass/issues/575)
- Installation is failed using latest Homebrew in macOS [\#564](https://github.com/IJHack/QtPass/issues/564)
- Deleting a directory sometimes deletes the entire password store including Git repositories [\#556](https://github.com/IJHack/QtPass/issues/556)

**Closed issues:**

- \[Windows\] Git repository not working [\#638](https://github.com/IJHack/QtPass/issues/638)
- support `PASSWORD_STORE_SIGNING_KEY` with profiles [\#624](https://github.com/IJHack/QtPass/issues/624)
- Support multiple branches via "Profiles" feature [\#545](https://github.com/IJHack/QtPass/issues/545)

**Merged pull requests:**

- clang-format -i src/\*.cpp src/\*.h [\#645](https://github.com/IJHack/QtPass/pull/645) ([annejan](https://github.com/annejan))
- Translations update from Hosted Weblate [\#644](https://github.com/IJHack/QtPass/pull/644) ([weblate](https://github.com/weblate))
- Fix taborder and add buddies in keygen dialog [\#643](https://github.com/IJHack/QtPass/pull/643) ([svuorela](https://github.com/svuorela))
- Restore licensing info for QProgressIndicator [\#642](https://github.com/IJHack/QtPass/pull/642) ([svuorela](https://github.com/svuorela))
- Clazy cleanup and other minor fixes [\#641](https://github.com/IJHack/QtPass/pull/641) ([svuorela](https://github.com/svuorela))
- fix the unintended "running" of the entropy window in the keygen dial… [\#640](https://github.com/IJHack/QtPass/pull/640) ([lherschi](https://github.com/lherschi))
- Translations update from Hosted Weblate [\#636](https://github.com/IJHack/QtPass/pull/636) ([weblate](https://github.com/weblate))
- Add pass store signing key feature [\#634](https://github.com/IJHack/QtPass/pull/634) ([timegrid](https://github.com/timegrid))
- Translations update from Hosted Weblate [\#633](https://github.com/IJHack/QtPass/pull/633) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#632](https://github.com/IJHack/QtPass/pull/632) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#629](https://github.com/IJHack/QtPass/pull/629) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#628](https://github.com/IJHack/QtPass/pull/628) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#627](https://github.com/IJHack/QtPass/pull/627) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#626](https://github.com/IJHack/QtPass/pull/626) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#622](https://github.com/IJHack/QtPass/pull/622) ([weblate](https://github.com/weblate))
- markdownlint --fix && textlint --fix [\#621](https://github.com/IJHack/QtPass/pull/621) ([annejan](https://github.com/annejan))
- Document "Using profiles" [\#619](https://github.com/IJHack/QtPass/pull/619) ([buepro](https://github.com/buepro))
- Translations update from Hosted Weblate [\#618](https://github.com/IJHack/QtPass/pull/618) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#617](https://github.com/IJHack/QtPass/pull/617) ([weblate](https://github.com/weblate))
- super-linter ENV variables in shared location for local and automated [\#616](https://github.com/IJHack/QtPass/pull/616) ([annejan](https://github.com/annejan))
- fix bug =\> clipboard was not cleared when using primary selection [\#615](https://github.com/IJHack/QtPass/pull/615) ([pythcoiner](https://github.com/pythcoiner))
- Translations update from Hosted Weblate [\#614](https://github.com/IJHack/QtPass/pull/614) ([weblate](https://github.com/weblate))
- Translations update from Hosted Weblate [\#613](https://github.com/IJHack/QtPass/pull/613) ([weblate](https://github.com/weblate))
- Removed travis \(no longer free\) and lgtm \(migrated to GitHub\) [\#612](https://github.com/IJHack/QtPass/pull/612) ([annejan](https://github.com/annejan))
- Translations update from Hosted Weblate [\#611](https://github.com/IJHack/QtPass/pull/611) ([weblate](https://github.com/weblate))
- Super Linter added and fixing findings [\#610](https://github.com/IJHack/QtPass/pull/610) ([annejan](https://github.com/annejan))
- New Transifex integration yml [\#609](https://github.com/IJHack/QtPass/pull/609) ([annejan](https://github.com/annejan))
- Install QT in codeql workflow [\#608](https://github.com/IJHack/QtPass/pull/608) ([annejan](https://github.com/annejan))
- Translations update from Hosted Weblate [\#607](https://github.com/IJHack/QtPass/pull/607) ([weblate](https://github.com/weblate))
- Translation cleanup [\#606](https://github.com/IJHack/QtPass/pull/606) ([annejan](https://github.com/annejan))
- translations updated [\#605](https://github.com/IJHack/QtPass/pull/605) ([annejan](https://github.com/annejan))
- Fix accidental deletion of entire passwordstore [\#604](https://github.com/IJHack/QtPass/pull/604) ([FSMaxB](https://github.com/FSMaxB))
- Add more options for the password displaying [\#587](https://github.com/IJHack/QtPass/pull/587) ([l3u](https://github.com/l3u))
- Delete context menu after exec [\#578](https://github.com/IJHack/QtPass/pull/578) ([fasked](https://github.com/fasked))
- Translations update from Hosted Weblate [\#576](https://github.com/IJHack/QtPass/pull/576) ([weblate](https://github.com/weblate))

## [1.4.0-rc1](https://github.com/IJHack/QtPass/tree/1.4.0-rc1) (2021-09-22)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.3.2...1.4.0-rc1)

**Implemented enhancements:**

- Set correct WM_CLASS for the qr-code popup [\#506](https://github.com/IJHack/QtPass/issues/506)

**Fixed bugs:**

- QtPass does not detect current $GNUPGHOME and causes it to fail decryption [\#569](https://github.com/IJHack/QtPass/issues/569)
- \<tt\> ... \</tt\> included in password text [\#542](https://github.com/IJHack/QtPass/issues/542)
- Markup tags are left in password and clipboard [\#533](https://github.com/IJHack/QtPass/issues/533)
- Renaming passwords and directories fail [\#487](https://github.com/IJHack/QtPass/issues/487)
- Will not run on Windows 10 1903 b18362.418 [\#486](https://github.com/IJHack/QtPass/issues/486)

**Closed issues:**

- Hide results on search [\#551](https://github.com/IJHack/QtPass/issues/551)
- QtPass 1.3.2 freezes on macOS 10.15.6 when trying to display password [\#544](https://github.com/IJHack/QtPass/issues/544)
- Icons are blurry when fractional scaling is enabled [\#525](https://github.com/IJHack/QtPass/issues/525)
- \[Request\] clear search password when change profile [\#524](https://github.com/IJHack/QtPass/issues/524)
- Copying not possible on Ubuntu 20.04 [\#521](https://github.com/IJHack/QtPass/issues/521)
- UI can't handle passwords with periods in their name [\#520](https://github.com/IJHack/QtPass/issues/520)
- Display passwords in mono space font [\#514](https://github.com/IJHack/QtPass/issues/514)
- QtPass 1.3.2 for Ubuntu 19.10 \(eoan\) [\#512](https://github.com/IJHack/QtPass/issues/512)
- Default password visibility [\#511](https://github.com/IJHack/QtPass/issues/511)
- Consider mentioning export abilities in migration docs, if any are present [\#505](https://github.com/IJHack/QtPass/issues/505)
- Enable out-of-source \(shadow\) builds. [\#501](https://github.com/IJHack/QtPass/issues/501)
- password visibility can't be fully hidden [\#496](https://github.com/IJHack/QtPass/issues/496)
- Translations need updating and checking [\#488](https://github.com/IJHack/QtPass/issues/488)
- Frontend doesn't work well with HiDPI screen [\#464](https://github.com/IJHack/QtPass/issues/464)
- How to let QtPass use the real "pass" on windows [\#458](https://github.com/IJHack/QtPass/issues/458)
- Fresh install of Antergos with Deepin - High DPI scaling is not working [\#417](https://github.com/IJHack/QtPass/issues/417)
- Strange behavior when clearing filter [\#402](https://github.com/IJHack/QtPass/issues/402)
- Tray icon remains after quitting program [\#401](https://github.com/IJHack/QtPass/issues/401)
- QtPass doesn't work will pass in WSL [\#375](https://github.com/IJHack/QtPass/issues/375)
- UI is blurry on HiDPI screens on macOS \(retina\) since 1.2.x [\#355](https://github.com/IJHack/QtPass/issues/355)
- No prompt for passphrase for Git key on windows. [\#317](https://github.com/IJHack/QtPass/issues/317)
- Config dialog's Password Generation field got crowded between 1.1.3 and 1.1.6 [\#278](https://github.com/IJHack/QtPass/issues/278)

**Merged pull requests:**

- Translations update from Weblate [\#573](https://github.com/IJHack/QtPass/pull/573) ([weblate](https://github.com/weblate))
- Fix keys created/expires dates in the users dialog window \(fix: 571\) [\#572](https://github.com/IJHack/QtPass/pull/572) ([nfetisov](https://github.com/nfetisov))
- Correct a typo in pass.cpp [\#570](https://github.com/IJHack/QtPass/pull/570) ([felixonmars](https://github.com/felixonmars))
- Fix installation instructions in README.md [\#565](https://github.com/IJHack/QtPass/pull/565) ([kawarimidoll](https://github.com/kawarimidoll))
- Translations update from Weblate [\#563](https://github.com/IJHack/QtPass/pull/563) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#562](https://github.com/IJHack/QtPass/pull/562) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#560](https://github.com/IJHack/QtPass/pull/560) ([weblate](https://github.com/weblate))
- Keep suffices when moving \(to\) a directory while imitiating pass [\#559](https://github.com/IJHack/QtPass/pull/559) ([ichthyosaurus](https://github.com/ichthyosaurus))
- Explicitly only remove ".gpg" when renaming files [\#558](https://github.com/IJHack/QtPass/pull/558) ([ichthyosaurus](https://github.com/ichthyosaurus))
- Translations update from Weblate [\#554](https://github.com/IJHack/QtPass/pull/554) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#553](https://github.com/IJHack/QtPass/pull/553) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#552](https://github.com/IJHack/QtPass/pull/552) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#548](https://github.com/IJHack/QtPass/pull/548) ([weblate](https://github.com/weblate))
- Move MainWindow to the screen the cursor is on [\#547](https://github.com/IJHack/QtPass/pull/547) ([inhinias](https://github.com/inhinias))
- Translations update from Weblate [\#541](https://github.com/IJHack/QtPass/pull/541) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#535](https://github.com/IJHack/QtPass/pull/535) ([weblate](https://github.com/weblate))
- Fix issues with renaming passwords and moving folders [\#532](https://github.com/IJHack/QtPass/pull/532) ([ChaoticEnigma](https://github.com/ChaoticEnigma))
- Translations update from Weblate [\#531](https://github.com/IJHack/QtPass/pull/531) ([weblate](https://github.com/weblate))
- Translations update from Weblate [\#530](https://github.com/IJHack/QtPass/pull/530) ([weblate](https://github.com/weblate))
- Clear search on profile change [\#529](https://github.com/IJHack/QtPass/pull/529) ([cmol](https://github.com/cmol))
- \#514 Show password with a monospace font [\#528](https://github.com/IJHack/QtPass/pull/528) ([cmol](https://github.com/cmol))
- Update minimum Qt version [\#527](https://github.com/IJHack/QtPass/pull/527) ([cmol](https://github.com/cmol))
- Fix blurry icons when fractional scaling is enabled [\#526](https://github.com/IJHack/QtPass/pull/526) ([mthw0](https://github.com/mthw0))
- Spelling: Git pull, Git push [\#516](https://github.com/IJHack/QtPass/pull/516) ([comradekingu](https://github.com/comradekingu))
- Enable Ubuntu, Windows and macOS based builds for CI [\#508](https://github.com/IJHack/QtPass/pull/508) ([boppybibbles](https://github.com/boppybibbles))
- Enable out-of-source build [\#503](https://github.com/IJHack/QtPass/pull/503) ([boppybibbles](https://github.com/boppybibbles))
- Use new stable version of `install-qt-action`. [\#502](https://github.com/IJHack/QtPass/pull/502) ([boppybibbles](https://github.com/boppybibbles))
- Don't base pass-otp availability decision on hardcoded /usr/lib [\#499](https://github.com/IJHack/QtPass/pull/499) ([nh2](https://github.com/nh2))
- Spelling: Search for users, , [\#495](https://github.com/IJHack/QtPass/pull/495) ([comradekingu](https://github.com/comradekingu))
- Spelling: Keylist missing, Could not fetch, GPG [\#493](https://github.com/IJHack/QtPass/pull/493) ([comradekingu](https://github.com/comradekingu))
- Spelling: Git, GPG, PWGen, etc. [\#492](https://github.com/IJHack/QtPass/pull/492) ([comradekingu](https://github.com/comradekingu))
- Don't use a deprecated method [\#491](https://github.com/IJHack/QtPass/pull/491) ([amarsman](https://github.com/amarsman))
- Issue \#402: 'deselect\(\)' on clearing filter [\#490](https://github.com/IJHack/QtPass/pull/490) ([petr-nehez](https://github.com/petr-nehez))

## [v1.3.2](https://github.com/IJHack/QtPass/tree/v1.3.2) (2019-10-09)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.3.1...v1.3.2)

**Fixed bugs:**

- QtPass could not run on Windows7 thin [\#485](https://github.com/IJHack/QtPass/issues/485)
- Segfault on application startup \(macOS\) [\#481](https://github.com/IJHack/QtPass/issues/481)
- Application crashes on empty password store [\#466](https://github.com/IJHack/QtPass/issues/466)
- App is completely broken [\#423](https://github.com/IJHack/QtPass/issues/423)

**Closed issues:**

- Edit window on Gnome has no padding around [\#484](https://github.com/IJHack/QtPass/issues/484)
- Buttons width on RHEL 8 [\#483](https://github.com/IJHack/QtPass/issues/483)
- `Start minimized' no longer works [\#471](https://github.com/IJHack/QtPass/issues/471)
- Editor doesn't wait for PGP key to decrypt [\#470](https://github.com/IJHack/QtPass/issues/470)
- v1.3.0 Data Not Showing [\#465](https://github.com/IJHack/QtPass/issues/465)
- Hangs on macOS after Security Update 2019-003 10.12.6 [\#461](https://github.com/IJHack/QtPass/issues/461)
- No public key [\#308](https://github.com/IJHack/QtPass/issues/308)

**Merged pull requests:**

- Don't call QtPass::setup\(\) from QtPass class constructor \(should fix \#466\) [\#482](https://github.com/IJHack/QtPass/pull/482) ([maciejsszmigiero](https://github.com/maciejsszmigiero))

## [v1.3.1](https://github.com/IJHack/QtPass/tree/v1.3.1) (2019-10-01)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.3.0...v1.3.1)

**Implemented enhancements:**

- Renaming password [\#463](https://github.com/IJHack/QtPass/issues/463)
- \[Feature Request\] Edit main title field [\#446](https://github.com/IJHack/QtPass/issues/446)

**Fixed bugs:**

- build: dependency issue [\#467](https://github.com/IJHack/QtPass/issues/467)
- is running but no GUI [\#451](https://github.com/IJHack/QtPass/issues/451)

**Closed issues:**

- Additional lines \(notes\) are not shown [\#474](https://github.com/IJHack/QtPass/issues/474)
- Bundle ID is literally `$(PRODUCT_BUNDLE_IDENTIFIER)` [\#448](https://github.com/IJHack/QtPass/issues/448)

**Merged pull requests:**

- Add license scan report and status [\#480](https://github.com/IJHack/QtPass/pull/480) ([fossabot](https://github.com/fossabot))
- Build tooling related fixes [\#479](https://github.com/IJHack/QtPass/pull/479) ([maciejsszmigiero](https://github.com/maciejsszmigiero))
- Add missing overrides [\#478](https://github.com/IJHack/QtPass/pull/478) ([amarsman](https://github.com/amarsman))
- Main window entry details improvements [\#477](https://github.com/IJHack/QtPass/pull/477) ([maciejsszmigiero](https://github.com/maciejsszmigiero))
- Fix HTML links color and NL translation building error [\#476](https://github.com/IJHack/QtPass/pull/476) ([a-andreyev](https://github.com/a-andreyev))
- Restore directories-first order of passwords tree view on non-Mac platforms [\#475](https://github.com/IJHack/QtPass/pull/475) ([maciejsszmigiero](https://github.com/maciejsszmigiero))
- Add missing finishedShow\(\) signal connection in PasswordDialog constructor \(fixes the "Edit password" function\) [\#473](https://github.com/IJHack/QtPass/pull/473) ([maciejsszmigiero](https://github.com/maciejsszmigiero))
- Sorted profiles dropdown as in \#404 [\#472](https://github.com/IJHack/QtPass/pull/472) ([Noettore](https://github.com/Noettore))
- Add support for passwords and directories renaming as requested in \#463 [\#469](https://github.com/IJHack/QtPass/pull/469) ([Noettore](https://github.com/Noettore))
- Fix missing app ID and icon on Wayland. [\#468](https://github.com/IJHack/QtPass/pull/468) ([lightbulbjim](https://github.com/lightbulbjim))

## [v1.3.0](https://github.com/IJHack/QtPass/tree/v1.3.0) (2019-08-20)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.2.3...v1.3.0)

**Implemented enhancements:**

- Localization makes commits absolutely unreadable [\#405](https://github.com/IJHack/QtPass/issues/405)
- Add otp \(two factor authentication\) support [\#327](https://github.com/IJHack/QtPass/issues/327)
- Open specific entry from command-line parameter [\#32](https://github.com/IJHack/QtPass/issues/32)

**Fixed bugs:**

- Windows sigsev issues [\#326](https://github.com/IJHack/QtPass/issues/326)
- Access to the / \(root\) directory form within the application window on macOS [\#302](https://github.com/IJHack/QtPass/issues/302)
- PRNG seeding is done totally wrong [\#238](https://github.com/IJHack/QtPass/issues/238)
- Context menu on transparent fields is transparent too . . [\#227](https://github.com/IJHack/QtPass/issues/227)

**Closed issues:**

- various issues with Info.plist file on macOS [\#457](https://github.com/IJHack/QtPass/issues/457)
- Can not add new passwords for some reason [\#454](https://github.com/IJHack/QtPass/issues/454)
- GnuPG not found on Linux Mint [\#433](https://github.com/IJHack/QtPass/issues/433)
- How to clean up the app [\#429](https://github.com/IJHack/QtPass/issues/429)
- LAN sync request [\#427](https://github.com/IJHack/QtPass/issues/427)
- Profiles can not be removed [\#415](https://github.com/IJHack/QtPass/issues/415)
- Compilation error in \(K\)Ubuntu 16.04.5 with sources tar.gz from version 1.2.3 [\#408](https://github.com/IJHack/QtPass/issues/408)
- Prevent from removing whole password-store directory and hidden directories and files [\#400](https://github.com/IJHack/QtPass/issues/400)
- Version information string/s [\#398](https://github.com/IJHack/QtPass/issues/398)
- We should select a C++ std too [\#372](https://github.com/IJHack/QtPass/issues/372)
- We should select a minimum Qt version [\#371](https://github.com/IJHack/QtPass/issues/371)
- Problem with GNUpg not found on macOS [\#362](https://github.com/IJHack/QtPass/issues/362)
- Compiling for Linux Mint 18 Ubuntu 16 [\#357](https://github.com/IJHack/QtPass/issues/357)
- make qtpass portable in windows [\#356](https://github.com/IJHack/QtPass/issues/356)
- Unable to see main application window \(applicationn runs minimized to tray only\) [\#286](https://github.com/IJHack/QtPass/issues/286)
- Startup variables and parameters [\#212](https://github.com/IJHack/QtPass/issues/212)
- \[macOS\] Password input dialog suddenly stopped popping up [\#191](https://github.com/IJHack/QtPass/issues/191)
- MainWindow is a giant monolithic mess [\#107](https://github.com/IJHack/QtPass/issues/107)

**Merged pull requests:**

- Use key fingerprint as ID instead of “long” ID. [\#452](https://github.com/IJHack/QtPass/pull/452) ([Natureshadow](https://github.com/Natureshadow))
- Typo: dialogue to dialogue. [\#444](https://github.com/IJHack/QtPass/pull/444) ([georgjaehnig](https://github.com/georgjaehnig))
- Scripts and logic specific to Windows Store releases [\#439](https://github.com/IJHack/QtPass/pull/439) ([rdoeffinger](https://github.com/rdoeffinger))
- For config check, check that the selected binary is available. [\#438](https://github.com/IJHack/QtPass/pull/438) ([rdoeffinger](https://github.com/rdoeffinger))
- Fix character encoding issues for non-UTF-8 locales. [\#435](https://github.com/IJHack/QtPass/pull/435) ([rdoeffinger](https://github.com/rdoeffinger))
- Fixes and improvements for config dialog [\#432](https://github.com/IJHack/QtPass/pull/432) ([rdoeffinger](https://github.com/rdoeffinger))
- Support for using WSL binaries on Windows [\#431](https://github.com/IJHack/QtPass/pull/431) ([rdoeffinger](https://github.com/rdoeffinger))
- Bugfixes and Windows compatibility improvements [\#430](https://github.com/IJHack/QtPass/pull/430) ([mrsch](https://github.com/mrsch))
- Semi-automatic code cleanup [\#425](https://github.com/IJHack/QtPass/pull/425) ([annejan](https://github.com/annejan))
- Update to prevent the installer requesting admin [\#424](https://github.com/IJHack/QtPass/pull/424) ([hughwilliams94](https://github.com/hughwilliams94))
- Display passwords as QR codes [\#421](https://github.com/IJHack/QtPass/pull/421) ([frawi](https://github.com/frawi))
- Tested working on macOS HS with pinentry-mac [\#419](https://github.com/IJHack/QtPass/pull/419) ([riccardocossu](https://github.com/riccardocossu))
- Dutch \(nl\) translation improvements [\#418](https://github.com/IJHack/QtPass/pull/418) ([equaeghe](https://github.com/equaeghe))
- Bugfixes [\#413](https://github.com/IJHack/QtPass/pull/413) ([rdoeffinger](https://github.com/rdoeffinger))
- pwgen: fix inverted "Generate ... less secure passwords" checkbox [\#409](https://github.com/IJHack/QtPass/pull/409) ([ahippo](https://github.com/ahippo))
- Continuing refactoring [\#407](https://github.com/IJHack/QtPass/pull/407) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- \#390 make box checked when opening a folder users panel [\#403](https://github.com/IJHack/QtPass/pull/403) ([kenji21](https://github.com/kenji21))

## [v1.2.3](https://github.com/IJHack/QtPass/tree/v1.2.3) (2018-06-04)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.2.2...v1.2.3)

**Closed issues:**

- Consider repology badges [\#396](https://github.com/IJHack/QtPass/issues/396)
- Unable to create new password [\#391](https://github.com/IJHack/QtPass/issues/391)
- Duplicate prefix in installation of tests directory in v1.2.2. [\#389](https://github.com/IJHack/QtPass/issues/389)
- Compilation error on FreeBSD member access into incomplete type [\#388](https://github.com/IJHack/QtPass/issues/388)
- No icons on macOS [\#377](https://github.com/IJHack/QtPass/issues/377)

**Merged pull requests:**

- Add support for OTP code generation on Linux as requested in \#327 [\#394](https://github.com/IJHack/QtPass/pull/394) ([Noettore](https://github.com/Noettore))
- Revert scroll bar changes [\#393](https://github.com/IJHack/QtPass/pull/393) ([destanyol](https://github.com/destanyol))
- Fix High Dpi Support. Works now under Windows and KDE/Plasma. [\#392](https://github.com/IJHack/QtPass/pull/392) ([hgraeber](https://github.com/hgraeber))

## [v1.2.2](https://github.com/IJHack/QtPass/tree/v1.2.2) (2018-05-07)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.2.1...v1.2.2)

**Implemented enhancements:**

- Cleaning \#includes [\#364](https://github.com/IJHack/QtPass/pull/364) ([FiloSpaTeam](https://github.com/FiloSpaTeam))

**Fixed bugs:**

- Insecure Password Generation [\#338](https://github.com/IJHack/QtPass/issues/338)
- Clipboard clearing timer is not reset when new passwords are copied to the clipboard [\#309](https://github.com/IJHack/QtPass/issues/309)
- Removal of files outside of password-store [\#300](https://github.com/IJHack/QtPass/issues/300)
- Some fixes and refactoring. [\#376](https://github.com/IJHack/QtPass/pull/376) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Fix & make clearClipboard more robust [\#359](https://github.com/IJHack/QtPass/pull/359) ([lukedirtwalker](https://github.com/lukedirtwalker))

**Closed issues:**

- Multiple question marks while trying to delete password [\#385](https://github.com/IJHack/QtPass/issues/385)
- No button icons and text in "menu bar" [\#383](https://github.com/IJHack/QtPass/issues/383)
- Cannot add a new password [\#380](https://github.com/IJHack/QtPass/issues/380)
- Tiny bit of regression [\#379](https://github.com/IJHack/QtPass/issues/379)
- Running qtPass remotelly not prompting for the GPG key passphrasse [\#374](https://github.com/IJHack/QtPass/issues/374)
- Entire program is huge on High DPI screen on Linux [\#369](https://github.com/IJHack/QtPass/issues/369)
- Two new issues since latest refactoring [\#368](https://github.com/IJHack/QtPass/issues/368)
- Chocolatey package outdated [\#366](https://github.com/IJHack/QtPass/issues/366)
- How do I change the language ? [\#352](https://github.com/IJHack/QtPass/issues/352)
- Parallel make issue in qtpass-1.2.1: ld: cannot find -lqtpass [\#350](https://github.com/IJHack/QtPass/issues/350)
- "copy" icon has disappeared in v1.2.1 [\#344](https://github.com/IJHack/QtPass/issues/344)
- No password entry prompt [\#343](https://github.com/IJHack/QtPass/issues/343)
- Can't install on macOS Sierra [\#337](https://github.com/IJHack/QtPass/issues/337)
- No icon on macOS [\#333](https://github.com/IJHack/QtPass/issues/333)
- Font and spacing used for URL links on right in main window absurdly large [\#329](https://github.com/IJHack/QtPass/issues/329)
- QtPass don't display all lines with templates [\#273](https://github.com/IJHack/QtPass/issues/273)

**Merged pull requests:**

- 2 simple fixes [\#386](https://github.com/IJHack/QtPass/pull/386) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Should fix \#383 [\#384](https://github.com/IJHack/QtPass/pull/384) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Move connect action to main.cpp. Default search text as parameter of… [\#382](https://github.com/IJHack/QtPass/pull/382) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- fix \#380 [\#381](https://github.com/IJHack/QtPass/pull/381) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Small refactoring. [\#378](https://github.com/IJHack/QtPass/pull/378) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Sorry for last error :\) [\#370](https://github.com/IJHack/QtPass/pull/370) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Optimizations :\) [\#367](https://github.com/IJHack/QtPass/pull/367) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Removed comment out \#includes [\#365](https://github.com/IJHack/QtPass/pull/365) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- fix for \#300 [\#363](https://github.com/IJHack/QtPass/pull/363) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Translated all missing content to Italian, created Release of transla… [\#361](https://github.com/IJHack/QtPass/pull/361) ([FiloSpaTeam](https://github.com/FiloSpaTeam))
- Refactoring [\#360](https://github.com/IJHack/QtPass/pull/360) ([lukedirtwalker](https://github.com/lukedirtwalker))
- Display all fields when using template setting, fixes \#273 [\#358](https://github.com/IJHack/QtPass/pull/358) ([lukedirtwalker](https://github.com/lukedirtwalker))
- Update CONTRIBUTING.md [\#354](https://github.com/IJHack/QtPass/pull/354) ([5bentz](https://github.com/5bentz))
- Add two entries in FAQ about the language [\#353](https://github.com/IJHack/QtPass/pull/353) ([5bentz](https://github.com/5bentz))
- Fix typo in french translation [\#349](https://github.com/IJHack/QtPass/pull/349) ([babolivier](https://github.com/babolivier))
- New scroll bar on large files [\#347](https://github.com/IJHack/QtPass/pull/347) ([destanyol](https://github.com/destanyol))
- Fix nested template argument list compile error [\#346](https://github.com/IJHack/QtPass/pull/346) ([martinburchell](https://github.com/martinburchell))
- Honor PREFIX during tests install [\#345](https://github.com/IJHack/QtPass/pull/345) ([SpiderX](https://github.com/SpiderX))

## [v1.2.1](https://github.com/IJHack/QtPass/tree/v1.2.1) (2018-01-04)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.2.0...v1.2.1)

**Closed issues:**

- Question: is it possible to mass import passes? [\#339](https://github.com/IJHack/QtPass/issues/339)
- Version 1.2.0 leaks passwords [\#334](https://github.com/IJHack/QtPass/issues/334)
- signed release files [\#332](https://github.com/IJHack/QtPass/issues/332)
- 2017 [\#330](https://github.com/IJHack/QtPass/issues/330)
- When importing settings from 1.1.5 or older clipboard settings revert to No Clipboard [\#232](https://github.com/IJHack/QtPass/issues/232)

**Merged pull requests:**

- Insecure password generation [\#342](https://github.com/IJHack/QtPass/pull/342) ([annejan](https://github.com/annejan))
- Add Catalan translation [\#336](https://github.com/IJHack/QtPass/pull/336) ([rbuj](https://github.com/rbuj))

## [v1.2.0](https://github.com/IJHack/QtPass/tree/v1.2.0) (2017-11-08)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.6...v1.2.0)

**Implemented enhancements:**

- Icon tray from system icon theme [\#318](https://github.com/IJHack/QtPass/issues/318)
- Copy button for each custom field [\#291](https://github.com/IJHack/QtPass/issues/291)
- Feature Request: Use primary selection instead of clipboard [\#280](https://github.com/IJHack/QtPass/issues/280)
- Add primary selection as clipboard option [\#281](https://github.com/IJHack/QtPass/pull/281) ([annejan](https://github.com/annejan))
- Feature: CTRL/CMD + Q closes the mainwindow \#258 [\#259](https://github.com/IJHack/QtPass/pull/259) ([YoshiMan](https://github.com/YoshiMan))
- Feature/testing moved sources to src added tests [\#257](https://github.com/IJHack/QtPass/pull/257) ([annejan](https://github.com/annejan))
- enabled drag and drop support for passwords and passwordfolders [\#245](https://github.com/IJHack/QtPass/pull/245) ([YoshiMan](https://github.com/YoshiMan))
- Password dialog decoupling from MW [\#242](https://github.com/IJHack/QtPass/pull/242) ([tezeb](https://github.com/tezeb))
- Refactoring of qpushbuttonwithclipboard and timers [\#241](https://github.com/IJHack/QtPass/pull/241) ([tezeb](https://github.com/tezeb))
- added a copy button for each line to paste the content into the clipboard, "pass init -- path=" command with right path-parameter, lupdate qtpass.pro [\#218](https://github.com/IJHack/QtPass/pull/218) ([YoshiMan](https://github.com/YoshiMan))

**Fixed bugs:**

- Do not hide passwords and no generator [\#267](https://github.com/IJHack/QtPass/issues/267)
- Weird behavior when turning on Git support \(auto push/pull\) with non-clean Git dir [\#128](https://github.com/IJHack/QtPass/issues/128)
- SingleApplication implementation buggy [\#26](https://github.com/IJHack/QtPass/issues/26)

**Closed issues:**

- Tab order is wrong in password dialog [\#331](https://github.com/IJHack/QtPass/issues/331)
- Missing icons since split to static lib [\#325](https://github.com/IJHack/QtPass/issues/325)
- "-session XXX" upon session restore taken as search string [\#320](https://github.com/IJHack/QtPass/issues/320)
- Instructions to install it on OSX maybe outdated [\#315](https://github.com/IJHack/QtPass/issues/315)
- QtPass hangs when trying to decrypt entry [\#313](https://github.com/IJHack/QtPass/issues/313)
- Unable to locate package \(Linux Mint 17.3\) [\#310](https://github.com/IJHack/QtPass/issues/310)
- Git commit signing [\#303](https://github.com/IJHack/QtPass/issues/303)
- Add to Linux brew [\#301](https://github.com/IJHack/QtPass/issues/301)
- Pass 1.7 testing [\#299](https://github.com/IJHack/QtPass/issues/299)
- Measure unit-test code coverage [\#298](https://github.com/IJHack/QtPass/issues/298)
- Config dialog: Propose "Password behaviour" label change [\#294](https://github.com/IJHack/QtPass/issues/294)
- make install currently broken. [\#289](https://github.com/IJHack/QtPass/issues/289)
- Unable to locate package \(Raspbian\) [\#287](https://github.com/IJHack/QtPass/issues/287)
- There is no `git cp` [\#272](https://github.com/IJHack/QtPass/issues/272)
- pass is apparently switching out pwgen [\#264](https://github.com/IJHack/QtPass/issues/264)
- Bugs since refactoring [\#262](https://github.com/IJHack/QtPass/issues/262)
- pass working fine but qtprocess failure with qtpass [\#260](https://github.com/IJHack/QtPass/issues/260)
- Feature: CTRL/CMD + Q closes the mainwindow [\#258](https://github.com/IJHack/QtPass/issues/258)
- Refactoring: removal of lastDecrypt [\#256](https://github.com/IJHack/QtPass/issues/256)
- Pass environment not set-up correctly [\#250](https://github.com/IJHack/QtPass/issues/250)
- Make fails - std c++11 not set [\#244](https://github.com/IJHack/QtPass/issues/244)
- Double-clicking might open previous entry instead of one double-clicked on [\#243](https://github.com/IJHack/QtPass/issues/243)
- Clean up ConfigDialog [\#235](https://github.com/IJHack/QtPass/issues/235)

**Merged pull requests:**

- Extract static library and separate main function [\#324](https://github.com/IJHack/QtPass/pull/324) ([tezeb](https://github.com/tezeb))
- galego actualizado [\#323](https://github.com/IJHack/QtPass/pull/323) ([xmgz](https://github.com/xmgz))
- Add SFTP, FTPS, WebDAV and WebDAVS as supported links [\#322](https://github.com/IJHack/QtPass/pull/322) ([cgonzalez](https://github.com/cgonzalez))
- Ignore cmdline arguments if -session is used. [\#321](https://github.com/IJHack/QtPass/pull/321) ([Achimh3011](https://github.com/Achimh3011))
- Finished French translation \(and proof-read the already translated strings\). [\#311](https://github.com/IJHack/QtPass/pull/311) ([Marcool04](https://github.com/Marcool04))
- Once again, code coverage [\#305](https://github.com/IJHack/QtPass/pull/305) ([tezeb](https://github.com/tezeb))
- Fixed path of resources.qrc [\#297](https://github.com/IJHack/QtPass/pull/297) ([sideeffect42](https://github.com/sideeffect42))
- Add pt_PT translation [\#295](https://github.com/IJHack/QtPass/pull/295) ([keitalbame](https://github.com/keitalbame))
- Update README.md [\#293](https://github.com/IJHack/QtPass/pull/293) ([joostruis](https://github.com/joostruis))
- small band aid fix for password generation on windows [\#276](https://github.com/IJHack/QtPass/pull/276) ([treat1](https://github.com/treat1))
- Final step in process mgmt refactoring [\#275](https://github.com/IJHack/QtPass/pull/275) ([tezeb](https://github.com/tezeb))
- Fix pwgen and refactor Pass::finished [\#271](https://github.com/IJHack/QtPass/pull/271) ([tezeb](https://github.com/tezeb))
- Process specific signals for process management [\#270](https://github.com/IJHack/QtPass/pull/270) ([tezeb](https://github.com/tezeb))
- \#239 reencrypting after a drag and drop action [\#261](https://github.com/IJHack/QtPass/pull/261) ([YoshiMan](https://github.com/YoshiMan))
- this if evaluates every time to true [\#255](https://github.com/IJHack/QtPass/pull/255) ([YoshiMan](https://github.com/YoshiMan))
- executeing pass show before editpassword dialog shows up [\#254](https://github.com/IJHack/QtPass/pull/254) ([YoshiMan](https://github.com/YoshiMan))
- Minor fix for filenames and Git push [\#251](https://github.com/IJHack/QtPass/pull/251) ([tezeb](https://github.com/tezeb))
- Process management refactoring part 2 [\#249](https://github.com/IJHack/QtPass/pull/249) ([tezeb](https://github.com/tezeb))
- refactoring - pass ifce, process mgmt [\#234](https://github.com/IJHack/QtPass/pull/234) ([tezeb](https://github.com/tezeb))
- Solve double-click issue [\#230](https://github.com/IJHack/QtPass/pull/230) ([jounathaen](https://github.com/jounathaen))
- refactoring, new QtPassSettings class, all settings should be read and written here [\#224](https://github.com/IJHack/QtPass/pull/224) ([YoshiMan](https://github.com/YoshiMan))
- Moved @YoshiMan 's copy buttons inside the line Edit [\#222](https://github.com/IJHack/QtPass/pull/222) ([jounathaen](https://github.com/jounathaen))
- UI Improvements [\#220](https://github.com/IJHack/QtPass/pull/220) ([jounathaen](https://github.com/jounathaen))
- creating password store directory, if it does not exists, de_DE translation fixes and removed obsolete translations [\#216](https://github.com/IJHack/QtPass/pull/216) ([YoshiMan](https://github.com/YoshiMan))

## [v1.1.6](https://github.com/IJHack/QtPass/tree/v1.1.6) (2016-12-02)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.5...v1.1.6)

**Implemented enhancements:**

- Feedback on copy button use [\#229](https://github.com/IJHack/QtPass/issues/229)
- Clickable URLs + open in default browser [\#226](https://github.com/IJHack/QtPass/issues/226)
- Deselecting password re-opens the file [\#221](https://github.com/IJHack/QtPass/issues/221)
- Copy password button should include tooltip to say why, when disabled [\#214](https://github.com/IJHack/QtPass/issues/214)
- QtPass starts by searching for -psn_0_12345 on macOS [\#213](https://github.com/IJHack/QtPass/issues/213)
- Copy after timeout [\#189](https://github.com/IJHack/QtPass/issues/189)
- Feature Request: Copy template fields with button [\#133](https://github.com/IJHack/QtPass/issues/133)
- Cannot create top level folder [\#127](https://github.com/IJHack/QtPass/issues/127)
- Feature: moving items \(reordering folders\) [\#116](https://github.com/IJHack/QtPass/issues/116)

**Fixed bugs:**

- Regression with new view mode when using templates and URLs [\#223](https://github.com/IJHack/QtPass/issues/223)
- Problems with high dpi screen [\#217](https://github.com/IJHack/QtPass/issues/217)
- Hangs forever on Generate GnuPG keypair [\#215](https://github.com/IJHack/QtPass/issues/215)
- recent change to passworddialog.cpp [\#188](https://github.com/IJHack/QtPass/issues/188)
- Re-opening entry in QtPass on Windows does not put login or URL values back in the right place [\#183](https://github.com/IJHack/QtPass/issues/183)

**Closed issues:**

- Click does not stick [\#233](https://github.com/IJHack/QtPass/issues/233)
- double-click on Treeview does not open the edit dialogue [\#228](https://github.com/IJHack/QtPass/issues/228)
- Windows - Enable GPG SSH Authentication [\#225](https://github.com/IJHack/QtPass/issues/225)
- We need autotype . . [\#65](https://github.com/IJHack/QtPass/issues/65)

## [v1.1.5](https://github.com/IJHack/QtPass/tree/v1.1.5) (2016-10-19)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.4...v1.1.5)

**Implemented enhancements:**

- I translated for Simplified Chinese. [\#208](https://github.com/IJHack/QtPass/issues/208)
- Short fullname hangs QtPass keypair generation process for infinite time [\#202](https://github.com/IJHack/QtPass/issues/202)
- More options for password generation [\#98](https://github.com/IJHack/QtPass/issues/98)
- Git hangs on windows [\#71](https://github.com/IJHack/QtPass/issues/71)

**Fixed bugs:**

- view box is trimming whitespace [\#210](https://github.com/IJHack/QtPass/issues/210)

**Closed issues:**

- PREFIX is now really a prefix [\#185](https://github.com/IJHack/QtPass/issues/185)
- QtPass, Git and windows [\#173](https://github.com/IJHack/QtPass/issues/173)

**Merged pull requests:**

- Allow SSH links [\#211](https://github.com/IJHack/QtPass/pull/211) ([cgonzalez](https://github.com/cgonzalez))
- Increase maximum password length to 255 [\#209](https://github.com/IJHack/QtPass/pull/209) ([vladimiroff](https://github.com/vladimiroff))
- Password templates [\#207](https://github.com/IJHack/QtPass/pull/207) ([jounathaen](https://github.com/jounathaen))
- Updated German Translation [\#206](https://github.com/IJHack/QtPass/pull/206) ([jounathaen](https://github.com/jounathaen))
- Italian translation [\#204](https://github.com/IJHack/QtPass/pull/204) ([dakk](https://github.com/dakk))
- keygendialog email and name validition \(issue 202\) [\#203](https://github.com/IJHack/QtPass/pull/203) ([dakk](https://github.com/dakk))
- Lookup validity field to check if keys are valid [\#201](https://github.com/IJHack/QtPass/pull/201) ([thotypous](https://github.com/thotypous))
- Fix spelling error [\#200](https://github.com/IJHack/QtPass/pull/200) ([innir](https://github.com/innir))

## [v1.1.4](https://github.com/IJHack/QtPass/tree/v1.1.4) (2016-09-26)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.3...v1.1.4)

**Implemented enhancements:**

- Re-assign permissions when adding users [\#161](https://github.com/IJHack/QtPass/issues/161)
- Main window immediately closes upon app launch [\#139](https://github.com/IJHack/QtPass/issues/139)

**Fixed bugs:**

- German umlauts fails [\#192](https://github.com/IJHack/QtPass/issues/192)
- Error after change configuration [\#190](https://github.com/IJHack/QtPass/issues/190)
- Bug: Special characters in Template [\#131](https://github.com/IJHack/QtPass/issues/131)
- Character encoding issue with GPG key [\#101](https://github.com/IJHack/QtPass/issues/101)
- saved password '§' turns to 'Â§' when copied to clipboard or shown when editing [\#91](https://github.com/IJHack/QtPass/issues/91)

**Closed issues:**

- Signed releases [\#186](https://github.com/IJHack/QtPass/issues/186)
- Why it's not listed in wikipedia.org/wiki/List_of_password_managers ? [\#164](https://github.com/IJHack/QtPass/issues/164)
- Bitdefender blocks installation and quarantines the .exe and .ink [\#138](https://github.com/IJHack/QtPass/issues/138)

**Merged pull requests:**

- issue 91 bugfix [\#199](https://github.com/IJHack/QtPass/pull/199) ([asalamon74](https://github.com/asalamon74))
- issue 101 bugfix [\#198](https://github.com/IJHack/QtPass/pull/198) ([asalamon74](https://github.com/asalamon74))
- Czech translation [\#195](https://github.com/IJHack/QtPass/pull/195) ([svetlemodry](https://github.com/svetlemodry))

## [v1.1.3](https://github.com/IJHack/QtPass/tree/v1.1.3) (2016-06-10)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.2...v1.1.3)

**Fixed bugs:**

- edit of password broken with active "Automatically push" [\#177](https://github.com/IJHack/QtPass/issues/177)
- Clipboard not cleared when quitting or killing application [\#171](https://github.com/IJHack/QtPass/issues/171)
- Hide content doesn't work when using templates [\#160](https://github.com/IJHack/QtPass/issues/160)

**Closed issues:**

- Add a \(small\) manpage [\#174](https://github.com/IJHack/QtPass/issues/174)

## [v1.1.2](https://github.com/IJHack/QtPass/tree/v1.1.2) (2016-06-10)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.1...v1.1.2)

**Implemented enhancements:**

- qtpass on windows, space in front of URL and Username [\#182](https://github.com/IJHack/QtPass/issues/182)

**Fixed bugs:**

- Deletion of folder doesn't work on Debian/GNU Linux [\#181](https://github.com/IJHack/QtPass/issues/181)

**Closed issues:**

- gpg: decryption failed: No secret key [\#179](https://github.com/IJHack/QtPass/issues/179)
- "gpg-agent: command get_passphrase failed: No such file or directory" [\#156](https://github.com/IJHack/QtPass/issues/156)

**Merged pull requests:**

- add Appdata file and update desktop file [\#178](https://github.com/IJHack/QtPass/pull/178) ([daveol](https://github.com/daveol))
- HTTPS everywhere [\#176](https://github.com/IJHack/QtPass/pull/176) ([da2x](https://github.com/da2x))
- Fix build issues with MSVC2015 on Windows [\#175](https://github.com/IJHack/QtPass/pull/175) ([msvi](https://github.com/msvi))

## [v1.1.1](https://github.com/IJHack/QtPass/tree/v1.1.1) (2016-04-04)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.1.0...v1.1.1)

**Implemented enhancements:**

- Signed binaries [\#149](https://github.com/IJHack/QtPass/issues/149)
- Icon theme and Cinnamon [\#146](https://github.com/IJHack/QtPass/issues/146)
- Bind a key to the clear action [\#142](https://github.com/IJHack/QtPass/issues/142)
- Installation dependencies [\#140](https://github.com/IJHack/QtPass/issues/140)
- All text input fields need example text & edit dialogue changes [\#85](https://github.com/IJHack/QtPass/issues/85)
- OSX: Qt-window closed only reappears when 'active' and using tray incon [\#77](https://github.com/IJHack/QtPass/issues/77)

**Fixed bugs:**

- Spelling bug: German translation of push and pull [\#110](https://github.com/IJHack/QtPass/issues/110)
- gpg: decryption failed: No secret key [\#92](https://github.com/IJHack/QtPass/issues/92)

**Closed issues:**

- Remove outdated Debian packaging [\#165](https://github.com/IJHack/QtPass/issues/165)
- Same name for file and folder [\#159](https://github.com/IJHack/QtPass/issues/159)
- Icons don't work on NixOS [\#157](https://github.com/IJHack/QtPass/issues/157)
- gpg: Sorry, we are in batchmode - can't get input [\#151](https://github.com/IJHack/QtPass/issues/151)

**Merged pull requests:**

- lupdate and Russian translation [\#170](https://github.com/IJHack/QtPass/pull/170) ([ahippo](https://github.com/ahippo))
- Remove path to password store in commit message and a leading space. [\#169](https://github.com/IJHack/QtPass/pull/169) ([ahippo](https://github.com/ahippo))
- Use --secure for pwgen and add more configurable options [\#168](https://github.com/IJHack/QtPass/pull/168) ([ahippo](https://github.com/ahippo))
- Remove Debian packaging [\#166](https://github.com/IJHack/QtPass/pull/166) ([innir](https://github.com/innir))
- update gl_Es [\#162](https://github.com/IJHack/QtPass/pull/162) ([xmgz](https://github.com/xmgz))
- Two UI Tweaks [\#158](https://github.com/IJHack/QtPass/pull/158) ([lftl](https://github.com/lftl))
- configwindow.ui default/start tab set to "settings" [\#154](https://github.com/IJHack/QtPass/pull/154) ([jounathaen](https://github.com/jounathaen))
- FAQ update concerning button-icons on cinnamon [\#153](https://github.com/IJHack/QtPass/pull/153) ([jounathaen](https://github.com/jounathaen))

## [v1.1.0](https://github.com/IJHack/QtPass/tree/v1.1.0) (2016-01-25)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.6...v1.1.0)

**Implemented enhancements:**

- Clear text input: use system icon instead of x [\#84](https://github.com/IJHack/QtPass/issues/84)
- System Icons on Buttons and double-click on treeView [\#124](https://github.com/IJHack/QtPass/pull/124) ([jounathaen](https://github.com/jounathaen))

**Closed issues:**

- \[resolved\] Error in compiling macOS El capitan [\#148](https://github.com/IJHack/QtPass/issues/148)

**Merged pull requests:**

- Pre 1.1 mixing [\#145](https://github.com/IJHack/QtPass/pull/145) ([annejan](https://github.com/annejan))
- RPM Spec file updates [\#137](https://github.com/IJHack/QtPass/pull/137) ([muff1nman](https://github.com/muff1nman))
- swedish translations [\#135](https://github.com/IJHack/QtPass/pull/135) ([ralphtheninja](https://github.com/ralphtheninja))

## [v1.0.6](https://github.com/IJHack/QtPass/tree/v1.0.6) (2016-01-03)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.5...v1.0.6)

**Implemented enhancements:**

- Feature: Always on top [\#118](https://github.com/IJHack/QtPass/issues/118)
- Option to show minimized instance [\#99](https://github.com/IJHack/QtPass/issues/99)

**Fixed bugs:**

- Bug: deleted record stays in memory [\#117](https://github.com/IJHack/QtPass/issues/117)

**Closed issues:**

- SIGSEGV in MainWindow::executeWrapper on clean install [\#122](https://github.com/IJHack/QtPass/issues/122)

**Merged pull requests:**

- improved the German translation [\#134](https://github.com/IJHack/QtPass/pull/134) ([retokromer](https://github.com/retokromer))
- qrand always generating the same sequence of passwords [\#129](https://github.com/IJHack/QtPass/pull/129) ([treat1](https://github.com/treat1))
- some improvements [\#126](https://github.com/IJHack/QtPass/pull/126) ([retokromer](https://github.com/retokromer))
- added one translation [\#125](https://github.com/IJHack/QtPass/pull/125) ([retokromer](https://github.com/retokromer))
- initial attempt to create a RPM spec file [\#121](https://github.com/IJHack/QtPass/pull/121) ([bram-ivs](https://github.com/bram-ivs))
- Cleanup and coding standards [\#120](https://github.com/IJHack/QtPass/pull/120) ([annejan](https://github.com/annejan))
- Modified the clipboard logic to allow for on-demand copy to clipboard. [\#119](https://github.com/IJHack/QtPass/pull/119) ([jonhanks](https://github.com/jonhanks))

## [v1.0.5](https://github.com/IJHack/QtPass/tree/v1.0.5) (2015-11-18)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.4...v1.0.5)

**Fixed bugs:**

- using pwgen adds carriage-return [\#115](https://github.com/IJHack/QtPass/issues/115)
- Enhancement: color code Git results [\#111](https://github.com/IJHack/QtPass/issues/111)

**Merged pull requests:**

- Fix bug that prints "Unknown error" to the terminal [\#113](https://github.com/IJHack/QtPass/pull/113) ([dvaerum](https://github.com/dvaerum))

## [v1.0.4](https://github.com/IJHack/QtPass/tree/v1.0.4) (2015-11-03)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.3...v1.0.4)

**Implemented enhancements:**

- Add support for RightToLeft languages [\#108](https://github.com/IJHack/QtPass/issues/108)

## [v1.0.3](https://github.com/IJHack/QtPass/tree/v1.0.3) (2015-10-25)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.2...v1.0.3)

**Implemented enhancements:**

- Get PREFIX variable from environment [\#106](https://github.com/IJHack/QtPass/issues/106)
- Password file named `git` returns error [\#105](https://github.com/IJHack/QtPass/issues/105)

**Merged pull requests:**

- Get PREFIX variable from environment [\#104](https://github.com/IJHack/QtPass/pull/104) ([jorti](https://github.com/jorti))
- spanish translations added [\#103](https://github.com/IJHack/QtPass/pull/103) ([mrpnkt](https://github.com/mrpnkt))

## [v1.0.2](https://github.com/IJHack/QtPass/tree/v1.0.2) (2015-09-24)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.1...v1.0.2)

**Closed issues:**

- Generate password: Floating point exception \(core dumped\) [\#102](https://github.com/IJHack/QtPass/issues/102)
- A way to indicate the installation prefix is needed [\#100](https://github.com/IJHack/QtPass/issues/100)
- IPv4 URLs are non-clickable [\#97](https://github.com/IJHack/QtPass/issues/97)
- app crashes when "Use pwgen" is unselected, and "Generate" is clicked. [\#95](https://github.com/IJHack/QtPass/issues/95)
- Some minor improvements on the templating part [\#93](https://github.com/IJHack/QtPass/issues/93)
- app crashes with variant of "pwgen" app [\#90](https://github.com/IJHack/QtPass/issues/90)

## [v1.0.1](https://github.com/IJHack/QtPass/tree/v1.0.1) (2015-08-09)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v1.0.0...v1.0.1)

**Implemented enhancements:**

- Users setup - key colours could be improved [\#82](https://github.com/IJHack/QtPass/issues/82)

**Closed issues:**

- When QtPass starts, focus search input box [\#89](https://github.com/IJHack/QtPass/issues/89)
- Clear the password display after some time [\#86](https://github.com/IJHack/QtPass/issues/86)
- Auto push/pull [\#83](https://github.com/IJHack/QtPass/issues/83)
- qtpass doesn't commit deletes to Git [\#81](https://github.com/IJHack/QtPass/issues/81)
- Always crashes while using the quick-search input [\#79](https://github.com/IJHack/QtPass/issues/79)
- Git initialisation [\#72](https://github.com/IJHack/QtPass/issues/72)
- Initialising new repo's doesn't work correctly [\#55](https://github.com/IJHack/QtPass/issues/55)
- gpg: Sorry, no terminal at all requested - can't get input [\#18](https://github.com/IJHack/QtPass/issues/18)

**Merged pull requests:**

- Issue 86 clear panel [\#87](https://github.com/IJHack/QtPass/pull/87) ([karlgrz](https://github.com/karlgrz))
- Update FAQ for Yubikey NEO helper in .bashrc for Ubuntu [\#80](https://github.com/IJHack/QtPass/pull/80) ([karlgrz](https://github.com/karlgrz))
- \[WIP\] Call 'pass Git init' on creation of password-store when useGit [\#78](https://github.com/IJHack/QtPass/pull/78) ([dennisdegreef](https://github.com/dennisdegreef))

## [v1.0.0](https://github.com/IJHack/QtPass/tree/v1.0.0) (2015-08-01)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.2...v1.0.0)

**Closed issues:**

- Yubikey Neo Pin entry not working properly on Ubuntu 15.04 [\#73](https://github.com/IJHack/QtPass/issues/73)

**Merged pull requests:**

- Updating hungarian localisation [\#76](https://github.com/IJHack/QtPass/pull/76) ([damnlie](https://github.com/damnlie))
- added DE translations [\#74](https://github.com/IJHack/QtPass/pull/74) ([Friedy](https://github.com/Friedy))

## [v0.9.2](https://github.com/IJHack/QtPass/tree/v0.9.2) (2015-07-30)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.1...v0.9.2)

**Closed issues:**

- Show expiration date in key setup [\#70](https://github.com/IJHack/QtPass/issues/70)

## [v0.9.1](https://github.com/IJHack/QtPass/tree/v0.9.1) (2015-07-29)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.9.0...v0.9.1)

**Closed issues:**

- Minimize on startup. [\#69](https://github.com/IJHack/QtPass/issues/69)
- tray icon in xfce [\#58](https://github.com/IJHack/QtPass/issues/58)
- Git integration [\#57](https://github.com/IJHack/QtPass/issues/57)
- Weird characters in filenames breaks loading gpg files [\#10](https://github.com/IJHack/QtPass/issues/10)

## [v0.9.0](https://github.com/IJHack/QtPass/tree/v0.9.0) (2015-07-17)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.6...v0.9.0)

**Closed issues:**

- Request: Integrate qtpass with pwgen for generating passwords. [\#68](https://github.com/IJHack/QtPass/issues/68)

## [v0.8.6](https://github.com/IJHack/QtPass/tree/v0.8.6) (2015-07-17)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.5.1...v0.8.6)

**Closed issues:**

- Copy password by Ctrl+C [\#60](https://github.com/IJHack/QtPass/issues/60)
- Remember window size and vertical pane width [\#59](https://github.com/IJHack/QtPass/issues/59)
- Multiline Editing [\#34](https://github.com/IJHack/QtPass/issues/34)

**Merged pull requests:**

- To make building successful wi Desktop Qt 5.4.0 MSVC2012 OpenGL 32bit [\#67](https://github.com/IJHack/QtPass/pull/67) ([annejan](https://github.com/annejan))

## [v0.8.5.1](https://github.com/IJHack/QtPass/tree/v0.8.5.1) (2015-07-08)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.5...v0.8.5.1)

## [v0.8.5](https://github.com/IJHack/QtPass/tree/v0.8.5) (2015-07-08)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.4...v0.8.5)

**Closed issues:**

- Won't compile on Kubuntu 15.10 [\#61](https://github.com/IJHack/QtPass/issues/61)
- Hanging process gives weird effects [\#56](https://github.com/IJHack/QtPass/issues/56)
- Directory separator actually broken by 208171fd09c55ad765fdf4fa1de9a7f0757fa72d [\#53](https://github.com/IJHack/QtPass/issues/53)

**Merged pull requests:**

- Many deadlocks and other nasty bugfixes [\#64](https://github.com/IJHack/QtPass/pull/64) ([annejan](https://github.com/annejan))
- Mention qt5-default package in readme [\#62](https://github.com/IJHack/QtPass/pull/62) ([lorrin](https://github.com/lorrin))
- Some hacks I needed for portable gpg4win release [\#54](https://github.com/IJHack/QtPass/pull/54) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.4](https://github.com/IJHack/QtPass/tree/v0.8.4) (2015-06-11)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.3...v0.8.4)

**Closed issues:**

- QtPass does not detect GPG installation [\#50](https://github.com/IJHack/QtPass/issues/50)
- Cannot create new folders [\#48](https://github.com/IJHack/QtPass/issues/48)
- Better error handling when no pass or gpg found initially [\#13](https://github.com/IJHack/QtPass/issues/13)

**Merged pull requests:**

- Develop [\#52](https://github.com/IJHack/QtPass/pull/52) ([annejan](https://github.com/annejan))
- Minor thingies [\#51](https://github.com/IJHack/QtPass/pull/51) ([beefcurtains](https://github.com/beefcurtains))

## [v0.8.3](https://github.com/IJHack/QtPass/tree/v0.8.3) (2015-06-09)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.2...v0.8.3)

**Merged pull requests:**

- Bugfixes [\#49](https://github.com/IJHack/QtPass/pull/49) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.2](https://github.com/IJHack/QtPass/tree/v0.8.2) (2015-05-27)

[Full Changelog](https://github.com/IJHack/QtPass/compare/v0.8.1...v0.8.2)

**Closed issues:**

- Typo in 37f17f3808c1c97bd72c165a530c67a4bfb82edb? [\#45](https://github.com/IJHack/QtPass/issues/45)
- Signing of keys from user management [\#41](https://github.com/IJHack/QtPass/issues/41)

**Merged pull requests:**

- Provide more information in user list. [\#47](https://github.com/IJHack/QtPass/pull/47) ([rdoeffinger](https://github.com/rdoeffinger))
- Enable C++11 and use it to simplify loops. [\#46](https://github.com/IJHack/QtPass/pull/46) ([rdoeffinger](https://github.com/rdoeffinger))

## [v0.8.1](https://github.com/IJHack/QtPass/tree/v0.8.1) (2015-05-06)

[Full Changelog](https://github.com/IJHack/QtPass/compare/c2eb3dff58e4de577f6c250ad225d42f762b6c26...v0.8.1)

**Fixed bugs:**

- Some items not found on first search [\#8](https://github.com/IJHack/QtPass/issues/8)

**Closed issues:**

- compiling qtpass on Ubuntu 15.04 - fails due to newer qmake version [\#43](https://github.com/IJHack/QtPass/issues/43)
- QProcess::start: Process is already running [\#40](https://github.com/IJHack/QtPass/issues/40)
- Extra line breaks seem to be added to the \(HTML\) output [\#39](https://github.com/IJHack/QtPass/issues/39)
- Missing develop branch and release testing [\#38](https://github.com/IJHack/QtPass/issues/38)
- Windows WebDAV broken by 24f8dec3c203921f765e923e6ae6a4069b8cf50a [\#36](https://github.com/IJHack/QtPass/issues/36)
- .gpg-id file not added to Git [\#35](https://github.com/IJHack/QtPass/issues/35)
- Icon filenames [\#31](https://github.com/IJHack/QtPass/issues/31)
- `GNUPGHOME` environment variable [\#30](https://github.com/IJHack/QtPass/issues/30)
- Feature: webdav alternative to Git [\#28](https://github.com/IJHack/QtPass/issues/28)
- Windows: not working due to pointless use of "sh" [\#16](https://github.com/IJHack/QtPass/issues/16)
- Windows: support static build and enable ASLR and NX [\#15](https://github.com/IJHack/QtPass/issues/15)
- Some paths to executables are printed when starting up [\#11](https://github.com/IJHack/QtPass/issues/11)

**Merged pull requests:**

- SingleApplication per user and leading newline removed from output [\#44](https://github.com/IJHack/QtPass/pull/44) ([annejan](https://github.com/annejan))
- User filtering and many fixes [\#42](https://github.com/IJHack/QtPass/pull/42) ([annejan](https://github.com/annejan))
- Re-enable Windows WebDAV support. [\#37](https://github.com/IJHack/QtPass/pull/37) ([rdoeffinger](https://github.com/rdoeffinger))
- User robustness [\#33](https://github.com/IJHack/QtPass/pull/33) ([rdoeffinger](https://github.com/rdoeffinger))
- Add WebDAV support. [\#29](https://github.com/IJHack/QtPass/pull/29) ([rdoeffinger](https://github.com/rdoeffinger))
- Add nosingleapp config. [\#27](https://github.com/IJHack/QtPass/pull/27) ([rdoeffinger](https://github.com/rdoeffinger))
- Add Makefile with commands to make a binary release ZIP file. [\#25](https://github.com/IJHack/QtPass/pull/25) ([rdoeffinger](https://github.com/rdoeffinger))
- Start process only after we finished disabling UI elements etc. [\#24](https://github.com/IJHack/QtPass/pull/24) ([rdoeffinger](https://github.com/rdoeffinger))
- Support for editing .gpg-id via GUI with public keyring list. [\#23](https://github.com/IJHack/QtPass/pull/23) ([rdoeffinger](https://github.com/rdoeffinger))
- More proper support for subdirectories. [\#22](https://github.com/IJHack/QtPass/pull/22) ([rdoeffinger](https://github.com/rdoeffinger))
- Russian translation \(+typo fixed\) [\#20](https://github.com/IJHack/QtPass/pull/20) ([mexus](https://github.com/mexus))
- Windows-related fixes. [\#17](https://github.com/IJHack/QtPass/pull/17) ([rdoeffinger](https://github.com/rdoeffinger))
- Deal with "special" characters [\#14](https://github.com/IJHack/QtPass/pull/14) ([JiCiT](https://github.com/JiCiT))
- galician and spanish localization files created [\#12](https://github.com/IJHack/QtPass/pull/12) ([xmgz](https://github.com/xmgz))
- Update localization_hu_HU.ts [\#9](https://github.com/IJHack/QtPass/pull/9) ([damnlie](https://github.com/damnlie))
- Replace which invocations with actual path resolution code [\#7](https://github.com/IJHack/QtPass/pull/7) ([shitbangs](https://github.com/shitbangs))
- Added Swedish and Polish localization to resources [\#6](https://github.com/IJHack/QtPass/pull/6) ([iamtew](https://github.com/iamtew))
- Swedish localization [\#5](https://github.com/IJHack/QtPass/pull/5) ([iamtew](https://github.com/iamtew))
- Update localization_hu_HU.ts [\#4](https://github.com/IJHack/QtPass/pull/4) ([reesenemesis](https://github.com/reesenemesis))
- Update localization_hu_HU.ts [\#3](https://github.com/IJHack/QtPass/pull/3) ([reesenemesis](https://github.com/reesenemesis))
- [pass](http://www.passwordstore.org/) [\#2](https://github.com/IJHack/QtPass/pull/2) ([guaka](https://github.com/guaka))
- Beginning of German translation [\#1](https://github.com/IJHack/QtPass/pull/1) ([mwfc](https://github.com/mwfc))
