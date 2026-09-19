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

- **A password store you cloned from someone else is not inert data.** With Git enabled, QtPass runs `git` in that repository, and Git runs the repository's hooks (`.git/hooks`) with your rights. Only enable Git for stores you trust as you would trust a script.
- **A signed `.gpg-id` protects the recipient list against someone who can change the store but does not hold the signing key.** QtPass verifies the signature over the exact bytes it then encrypts to, so a file swapped between the check and its use is not a way in.
- **Path checks catch mistakes, not a hostile local writer.** Store-boundary checks (`..`, symlinks, absolute paths) run before a file operation, not atomically with it. Another process that can write to your store while QtPass runs can also replace what is in it; that is outside what QtPass can protect.
- **Nothing is logged that is fed to `gpg` or `pass` on standard input** (passwords, passphrases). With `QT_LOGGING_RULES=qtpass.debug=true` the commands and their arguments are logged; values of options such as `--passphrase` are redacted should they ever appear.

## Dependencies

QtPass depends on:

- **Qt6** (primary; use `qmake6`) - GUI framework
- **Qt5** (5.15+, legacy; use `qmake`) - GUI framework
- **GPG** (gpg2) - encryption
- **pass** (optional) - password store CLI
- **Git** (optional) - version control

Ensure your system dependencies are kept up to date for security patches.
