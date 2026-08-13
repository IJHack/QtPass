# QtPass Scripts

This directory contains helper scripts for development and releases.

## Development

| Script                         | Description                                              |
| ------------------------------ | -------------------------------------------------------- |
| `generate-compile-commands.sh` | Generate `compile_commands.json` for IDE/LSP tooling     |
| `build-windows.cmd`            | Set up the MSVC x64 + Qt environment and build (Windows) |

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

### Build on Windows

From any `cmd.exe`; loads the Visual Studio x64 environment and picks an MSVC Qt:

```cmd
scripts\build-windows.cmd
scripts\build-windows.cmd check
```

Set `QT_DIR` to select a specific Qt. See [Windows.md](../Windows.md).

### Release Scripts

See the [qtpass-releasing skill](../.opencode/skills/qtpass-releasing/SKILL.md) for detailed release workflow.
