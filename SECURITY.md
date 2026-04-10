# Security Policy

## Supported Versions

QtPass is actively maintained and security updates are provided for the latest releases. Older versions are not supported.

| Version | Supported          |
| ------- | ------------------ |
| 1.5.x   | :white_check_mark: |
| < 1.5   | :x:                |

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

## Dependencies

QtPass depends on:

- **Qt6** (primary; use `qmake6`) - GUI framework
- **Qt5** (5.15+, legacy; use `qmake`) - GUI framework
- **GPG** (gpg2) - encryption
- **pass** (optional) - password store CLI
- **Git** (optional) - version control

Ensure your system dependencies are kept up to date for security patches.
