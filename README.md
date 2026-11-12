# QtPass website

This branch is the deployed site for [qtpass.org](https://qtpass.org/),
served directly by GitHub Pages.

Everything here is authored on this branch:

- the pages (`*.html`), `stylesheets/`, `javascripts/`, `images/` and
  `fonts/`
- PWA and serving boilerplate (`sw.js`, `manifest.json`, `sitemap.xml`,
  `.htaccess`)

The `docs/` folder is **not** edited here. It is built from `main` (the
QtPass application repository) by `.github/workflows/docs.yml` and pushed
to this branch automatically; merge in a build the usual way and do not
hand-edit its files.

## Workflow

- Open pull requests against this branch.
- `.github/workflows/website-lint.yml` runs `htmlhint` on every push and
  pull request; run it locally too:

      npx htmlhint "*.html"

- The homepage stamps its assets with a version query string (for example
  `stylesheets/styles.css?v=1.8.1-8`). Bump it when a release changes the
  page.

## Deploy

Merging to `gh-pages` publishes immediately. For the secondary host that
also serves qtpass.org, `sync.sh` (rsync) with `scripts/precompress.sh`
(gzip twins for the `.htaccess` rewrite) handle the transfer.

The film on `/history` is not in the branch: its three files
(`video/history/qtpass-world-{480,720,1080}.mp4`, 113 MB together) are
ignored by Git and so skipped by `sync.sh`, also with `--delete`. They
come from `renders/web/` in
[qtpass-world](https://github.com/annejan/qtpass-world); upload them apart:

```bash
rsync -avh --checksum video/history/ qtpass.org:www/qtpass/video/history/
```
