# QtPass Scripts

This directory contains helper scripts for development and releases.

## Development

| Script                         | Description                                          |
| ------------------------------ | ---------------------------------------------------- |
| `generate-compile-commands.sh` | Generate `compile_commands.json` for IDE/LSP tooling |

## Release

| Script                   | Description                     |
| ------------------------ | ------------------------------- |
| `release-linux.sh`       | Build and install on Linux      |
| `release-mac.sh`         | Build and package for macOS     |
| `sign-release-assets.sh` | Sign release artifacts with GPG |

## Usage

### Generate compile_commands.json

For IDE/LSP code completion:

```bash
./scripts/generate-compile-commands.sh
```

### Release Scripts

See the [qtpass-releasing skill](../.opencode/skills/qtpass-releasing/SKILL.md) for detailed release workflow.
