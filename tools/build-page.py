#!/usr/bin/env python3
"""Render a Markdown file from the QtPass repository as a page of the site.

    git show origin/1.8:FAQ.md | tools/build-page.py faq > faq.html
    git show origin/main:CONTRIBUTING.md | tools/build-page.py contributing > contributing.html
    npx prettier --write faq.html contributing.html

The Markdown goes through marked (GFM); headings get the same anchor
markup as the hand-written pages and GitHub-style ids, so links into
FAQ.md#some-question keep working on the site. Relative links to files
that have a page here go to that page, the rest to GitHub. A list of the
sections (and, for the FAQ, the questions) is inserted up top.
"""

import html
import re
import subprocess
import sys

REPO_BLOB = "https://github.com/IJHack/QtPass/blob/main/"
VERSION = "1.8.1"
ASSET_VERSION = "?v=1.8.1-5"

# Repository files that are pages on the site.
SITE_PAGES = {
    "FAQ.md": "/faq",
    "CONTRIBUTING.md": "/contributing",
}

PAGES = {
    "faq": {
        "source": "FAQ.md",
        "slug": "faq",
        "title": "QtPass FAQ",
        "heading": "QtPass FAQ",
        "description": (
            "Frequently asked questions about QtPass: GnuPG and pinentry "
            "problems, Git on Windows, one-time passwords, where settings "
            "live, and how to help."
        ),
        "blurb": (
            "Answers to the questions that reach the issue tracker and the "
            "mailing list most often."
        ),
        "crumb": "FAQ",
        "toc_label": "Questions",
        "toc_depth": 3,
    },
    "contributing": {
        "source": "CONTRIBUTING.md",
        "slug": "contributing",
        "title": "Contributing to QtPass",
        "heading": "Contributing",
        "description": (
            "How to contribute to QtPass: the pull request process, the "
            "rules for AI-assisted changes, translations on Weblate, and "
            "building on Windows."
        ),
        "blurb": (
            "Pull requests, translations, bug reports: what helps and how to "
            "send it."
        ),
        "crumb": "Contributing",
        "toc_label": "Sections",
        "toc_depth": 2,
    },
}

HEAD = """<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>{title}</title>
    <meta name="description" content="{description}" />
    <link rel="stylesheet" href="stylesheets/styles.css{v}" />
    <link rel="canonical" href="https://qtpass.org/{slug}" />
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1, user-scalable=yes"
    />
    <meta name="color-scheme" content="light dark" />
    <link
      rel="preload"
      href="fonts/lato-latin-400-normal.woff2"
      as="font"
      type="font/woff2"
      crossorigin
    />
    <link
      rel="preload"
      href="fonts/lato-latin-700-normal.woff2"
      as="font"
      type="font/woff2"
      crossorigin
    />
    <link rel="icon" type="image/svg+xml" href="images/logo.svg{v}" />
    <link rel="icon" type="image/png" sizes="32x32" href="/images/favicon-32x32.png{v}" />
    <link rel="icon" type="image/png" sizes="16x16" href="/images/favicon-16x16.png{v}" />
    <link rel="apple-touch-icon" sizes="180x180" href="/images/apple-icon-180x180.png{v}" />
    <link rel="manifest" href="/manifest.json" />
    <meta
      name="theme-color"
      media="(prefers-color-scheme: light)"
      content="#fafbfc"
    />
    <meta
      name="theme-color"
      media="(prefers-color-scheme: dark)"
      content="#14181d"
    />
    <meta property="og:title" content="{title}" />
    <meta property="og:site_name" content="QtPass" />
    <meta property="og:url" content="https://qtpass.org/{slug}" />
    <meta property="og:image" content="https://qtpass.org/images/og.png{v}" />
    <meta property="og:image:width" content="1280" />
    <meta property="og:image:height" content="640" />
    <meta
      property="og:image:alt"
      content="The QtPass padlocked heart next to the name and the address qtpass.org"
    />
    <meta name="twitter:card" content="summary_large_image" />
    <meta property="og:description" content="{description}" />
    <meta property="og:type" content="website" />
    <script type="application/ld+json">
      {{
        "@context": "https://schema.org",
        "@type": "BreadcrumbList",
        "itemListElement": [
          {{
            "@type": "ListItem",
            "position": 1,
            "name": "QtPass",
            "item": "https://qtpass.org/"
          }},
          {{
            "@type": "ListItem",
            "position": 2,
            "name": "{crumb}",
            "item": "https://qtpass.org/{slug}"
          }}
        ]
      }}
    </script>
  </head>
  <body>
    <a class="skip" href="#main">Skip to content</a>
    <div class="background"></div>
    <div class="wrapper">
      <aside class="sidebar">
        <header>
          <h1>{heading} <small>{version}</small></h1>
          <p>{blurb}</p>

          <p class="view">
            <a href="/">Back to QtPass Home</a>
          </p>
          <p>
            <small
              >Generated from
              <a href="{blob}{source}">{source}</a> in the repository; improve
              it there.</small
            >
          </p>
        </header>
        <footer>
          <p>
            This project is maintained by
            <a title="IJhack on GitHub" href="https://github.com/IJHack"
              >IJHack</a
            >
          </p>
          <p>
            <small
              >Based on a theme by
              <a href="https://github.com/orderedlist">orderedlist</a></small
            >
          </p>
          <p class="fine">
            <small
              ><a title="QtPass Sitemap" href="/sitemap">Sitemap</a> ·
              <a title="QtPass privacy policy" href="/privacy">Privacy</a> ·
              <a title="Latest documentation" href="/docs/">API docs</a></small
            >
          </p>
        </footer>
      </aside>
      <section id="main">
"""

TAIL = """      </section>
    </div>
    <script src="javascripts/main.js{v}" defer></script>
  </body>
</html>
"""


def slugify(text, seen):
    """GitHub's heading id: lowercase, drop punctuation, spaces to hyphens."""
    plain = html.unescape(re.sub(r"<[^>]+>", "", text))
    slug = re.sub(r"[^\w\- ]", "", plain.lower()).strip().replace(" ", "-")
    base, n = slug, 1
    while slug in seen:
        slug = f"{base}-{n}"
        n += 1
    seen.add(slug)
    return slug


def anchor(level, slug, text):
    return (
        f'<h{level}>\n'
        f'  <a id="{slug}" class="anchor" href="#{slug}" aria-hidden="true"'
        f'><span class="octicon octicon-link"></span></a\n'
        f"  >{text}\n"
        f"</h{level}>"
    )


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in PAGES:
        sys.exit(f"usage: build-page.py {{{'|'.join(PAGES)}}} < FILE.md")
    page = PAGES[sys.argv[1]]
    source = sys.stdin.read()
    body = subprocess.run(
        ["npx", "--yes", "marked", "--gfm"],
        input=source,
        capture_output=True,
        text=True,
        check=True,
    ).stdout

    # The title lives in the sidebar.
    body = re.sub(r"^<h1>.*?</h1>\n?", "", body, count=1)

    # Links to sibling files in the repository: to their page here if they
    # have one, otherwise to GitHub.
    def repo_link(m):
        target = m.group(1)
        name, _, fragment = target.partition("#")
        if name in SITE_PAGES:
            return f'href="{SITE_PAGES[name]}{"#" + fragment if fragment else ""}"'
        return f'href="{REPO_BLOB}{target}"'

    body = re.sub(r'href="(?!https?:|mailto:|#|/)([^"]+)"', repo_link, body)

    seen = set()
    outline = []  # (level, slug, text)

    def heading(m):
        level, text = int(m.group(1)), m.group(2)
        slug = slugify(text, seen)
        outline.append((level, slug, text))
        return anchor(level, slug, text)

    body = re.sub(r"<h([23])>(.*?)</h\1>", heading, body)

    toc = [f'<nav class="toc" aria-label="{page["toc_label"]}">']
    if page["toc_depth"] == 2:
        toc.append("<ul>")
        toc += [
            f'<li><a href="#{slug}">{text}</a></li>'
            for level, slug, text in outline
            if level == 2
        ]
        toc.append("</ul>")
    else:
        open_list = False
        for level, slug, text in outline:
            if level == 2:
                if open_list:
                    toc.append("</ul>")
                toc.append(f'<p><a href="#{slug}">{text}</a></p>')
                toc.append("<ul>")
                open_list = True
            else:
                toc.append(f'<li><a href="#{slug}">{text}</a></li>')
        if open_list:
            toc.append("</ul>")
    toc.append("</nav>")

    sys.stdout.write(
        HEAD.format(v=ASSET_VERSION, version=VERSION, blob=REPO_BLOB, **page)
        + "\n".join(toc)
        + "\n"
        + body
        + TAIL.format(v=ASSET_VERSION)
    )


if __name__ == "__main__":
    main()
