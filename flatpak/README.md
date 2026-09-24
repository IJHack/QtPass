<!--
SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
SPDX-License-Identifier: CC-BY-4.0
-->

# QtPass as a Flatpak

`org.qtpass.QtPass.yml` is the manifest used for the Flathub submission (see
[#637](https://github.com/IJHack/QtPass/issues/637)). It builds QtPass from a
release tag on the KDE 6 runtime and bundles `git`, `pass`, `pwgen` and
`qrencode`; GnuPG comes with the runtime.

## Build and run locally

```sh
flatpak install flathub org.flatpak.Builder
flatpak run org.flatpak.Builder --user --install --force-clean \
  --install-deps-from=flathub build-dir flatpak/org.qtpass.QtPass.yml
flatpak run org.qtpass.QtPass
```

Lint the way Flathub does:

```sh
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest flatpak/org.qtpass.QtPass.yml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder appstream org.qtpass.QtPass.metainfo.xml
```

## How GnuPG works inside the sandbox

The sandbox has no access to `~/.gnupg`. The runtime-provided `gpg` (only the
`gpg2` wrapper is ours) therefore keeps its own keyring in
`~/.var/app/org.qtpass.QtPass/.gnupg`, but it connects to the **host**
`gpg-agent` through the read-only exposed socket directory
(`--filesystem=xdg-run/gnupg:ro`). Private keys, pinentry dialogs and
smartcards stay on the host; only the public keys have to be known inside the
sandbox. Import them once:

```sh
gpg --export --armor you@example.org | \
  flatpak run --command=gpg2 org.qtpass.QtPass --import
```

or paste them via **Users → Import key → From clipboard** in QtPass. The
host agent must be running (it is, on any desktop where `gpg` already works).

## The password store

The default store is private to the Flatpak:
`~/.var/app/org.qtpass.QtPass/.password-store` (`--persist=.password-store`).
To use an existing store, for example `~/.password-store` shared with `pass`
on the host, pick it with the folder button in Settings (Profiles page); the
portal gives access to that folder only. Alternatively, grant the folder to
QtPass alone with Flatseal or:

```sh
flatpak override --user --filesystem=~/.password-store org.qtpass.QtPass
```

## Permissions, and what is deliberately left out

Granted: Wayland/X11, network (for `git` over https), the gpg-agent socket
directory (read-only), a persistent private `~/.password-store` and
`~/.gnupg`, and the tray (`org.kde.StatusNotifierWatcher`). Nothing here needs
a Flathub linter exception.

Not granted on purpose: any host directory, `~/.gnupg` itself, `~/.ssh` and
the ssh-agent socket. For `git` over SSH, grant them with Flatseal or:

```sh
flatpak override --user --socket=ssh-auth --filesystem=~/.ssh:ro org.qtpass.QtPass
```

(`~/.ssh` read-only is for `known_hosts`.) Without the application ID,
`flatpak override` changes the defaults of every installed Flatpak.

## Updating

Bump the `tag`/`commit` of the `qtpass` module to the new release. The
`x-checker-data` blocks let Flathub's external-data-checker propose updates of
the bundled tools automatically. The module builds the tag as-is: since
v1.8.1 the tree ships `flatpak/gpg2`, and `make -C main install` puts the
desktop file, metainfo and icons where `rename-desktop-file` /
`rename-appdata-file` / `rename-icon` expect them, so no working-tree files
are overlaid any more. Since 1.8.2 the metainfo is installed under its
component ID (`org.qtpass.QtPass.metainfo.xml`), so the manifest no longer
needs `rename-appdata-file`.

## CI

The `Flatpak` workflow does not build the pinned tag: `flatpak/ci-manifest.sh`
derives `org.qtpass.QtPass.ci.yml`, identical except that the `qtpass` module
is a `dir` source pointing at the checkout under review, so a pull request
that changes the overlaid files together with other sources still builds.
The generated file is ignored by Git; run the script locally to build the
working tree with `flatpak-builder`.
