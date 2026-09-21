# Security Policy

## Supported Versions

QtPass is actively maintained and security updates are provided for the latest releases. Older versions are not supported.

| Version | Supported          |
| ------- | ------------------ |
| 1.8.x   | :white_check_mark: |
| < 1.8   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability in QtPass, please report it responsibly:

1. **Do NOT** create a public GitHub issue for security vulnerabilities
2. **Do** email the maintainer directly at: `help@qtpass.org`
3. **Alternative**: Open a private security advisory via GitHub
4. **Include** in your report:
   - Description of the vulnerability
   - Steps to reproduce the issue
   - Potential impact assessment
   - Any suggested fixes (optional)

## Response Timeline

- **Acknowledgment**: Within 48 hours
- **Initial assessment**: Within 7 days
- **Fix timeline**: Depends on severity; critical issues are prioritized

## Security Considerations

QtPass is a GUI for [pass](https://www.passwordstore.org/), the standard Unix password manager. Keep in mind:

- QtPass does not encrypt passwords - encryption is handled by GPG
- Passwords are stored in your local password store (typically `~/.password-store`)
- QtPass requires GPG to be installed and configured on your system
- The clipboard is cleared after a configurable timeout (default: 45 seconds)

### What QtPass does and does not protect against

- **A password store's `.git` directory is not inert data.** With Git enabled, QtPass runs `git` in the store, and Git runs the hooks in `.git/hooks` with your rights. A plain `git clone` does not copy a remote's hooks, but a store received as an archive or an existing checkout, or one on storage others can write to, may carry them. Treat such a `.git` as you would treat a script.
- **A signed `.gpg-id` protects the recipient list against someone who can change the store but does not hold the signing key.** QtPass verifies the signature over the exact bytes it then encrypts to, so a file swapped between the check and its use is not a way in.
- **A signature proves authentic, not current, and not placed.** An older list, genuinely signed, that still names a member since removed can be put back by the same someone, or copied into a folder that never had a list of its own.
- A `.gpg-id` QtPass writes for a store with a signing key therefore starts with two comment lines the signature covers: `# QtPass-GpgId-Generation: N` and `# QtPass-GpgId-Folder: <store-relative folder>`.
- QtPass remembers, per list, the highest generation it has accepted or written on this device, with a SHA-256 of the exact bytes, and refuses a signed list with a lower generation, one of the same generation with other bytes, or one written for another folder.
- Without a readable and writable record nothing is accepted or written; the record is one per user, and a second QtPass process waits for it or is refused.
- A holder of the signing key saving the recipients from QtPass writes the next generation; that is the way back after a deliberate revert. The dialog preselects only a list that verifies (an older but authentic one with a warning; one that does not verify not at all), so what is ticked is never an unsigned edit.
- The limits of that: it detects a rollback only on a device that has already accepted or written a higher generation; a device seeing the store for the first time, or one that was offline for the change, has nothing to compare against.
- A folder moved or renamed carries a list bound to its old place; saving its recipients once binds it anew.
- The generation is monotonic per device, not a distributed sequence number: two devices can both produce the same next generation, and Git synchronisation resolves that.
- The device that wrote or accepted one of two such lists then meets the other as the same generation with other bytes: a conflict, refused and not preselected, until a signing-key holder checks the recipients and saves, writing the next generation.
- Compatibility: the lines are comments to `pass` 1.7.4 and later and to QtPass 1.8 and later; `pass` up to 1.7.3 and Android Password Store take a comment for a recipient, so a signed store's clients need to be current. Stores without a signing key get plain lists, as before.
- A list written by `pass` (also through QtPass's `pass` backend) or by QtPass before 2.0 carries no generation and reads as 0: once a device has accepted a higher one, such a list is refused, and the Users dialog preselects nothing from it (without a folder line it may be any folder's old pair), until a signing-key holder selects and saves the recipients from QtPass.
- At generation 0 nothing is pinned: `pass` writes a headerless list for every change (also through QtPass's `pass` backend), so different bytes there are the normal course of a store kept with `pass`, not a conflict. Freshness begins with the first list QtPass writes.
- **Path checks catch mistakes, not a hostile local writer.** Store-boundary checks (`..`, symlinks and junctions, absolute paths) run before a file operation, not atomically with it. Another process that can write to your store while QtPass runs can also replace what is in it; that is outside what QtPass can protect.
- **A link inside the store is not part of it.** A store is often shared (a team's Git repository, a synced folder), and Git carries symbolic links, so a co-writer can make `git pull` create `Bank.gpg -> /elsewhere/secret.gpg` on your machine. QtPass treats a symbolic link or NTFS junction found inside the store as neither an entry, a folder nor metadata.
- Concretely: every walk skips links (re-encryption, search, staging); every operation on a link, or on anything behind one, is refused (show, edit, add, move, copy, re-key); a linked `.gpg-id` or `.gpg-id.sig` is not a recipient list; deleting a link removes the link.
- The configured store root itself may be a link (`~/.password-store` pointing at a synced folder is a normal setup): that is your configuration, not something found inside the store.
- **Nothing is logged that is fed to `gpg` or `pass` on standard input** (passwords, passphrases). With `QT_LOGGING_RULES=qtpass.debug=true` the commands and their arguments are logged; values of options such as `--passphrase` are redacted should they ever appear.

## Dependencies

QtPass depends on:

- **Qt6** (primary; use `qmake6`) - GUI framework
- **Qt5** (5.15+, legacy; use `qmake`) - GUI framework
- **GPG** (gpg2) - encryption
- **pass** (optional) - password store CLI
- **Git** (optional) - version control

Ensure your system dependencies are kept up to date for security patches.
