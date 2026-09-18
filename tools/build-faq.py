#!/usr/bin/env python3
"""Render FAQ.md from the QtPass repository as /faq on the site.

    git show origin/1.8:FAQ.md | tools/build-faq.py > faq.html
    npx prettier --write faq.html

The Markdown goes through marked (GFM); headings get the same anchor
markup as the hand-written pages and GitHub-style ids, so links into
FAQ.md#some-question keep working on the site. Relative links to other
files in the repository are pointed at GitHub. A list of the questions,
grouped by section, is inserted before the first section.
"""

import html
import re
import subprocess
import sys

REPO_BLOB = "https://github.com/IJHack/QtPass/blob/main/"
VERSION = "1.8.1"
ASSET_VERSION = "?v=1.8.1-4"

HEAD = """<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge" />
    <title>QtPass FAQ</title>
    <meta
      name="description"
      content="Frequently asked questions about QtPass: GnuPG and pinentry problems, Git on Windows, one-time passwords, where settings live, and how to help."
    />
    <link rel="stylesheet" href="stylesheets/styles.css{v}" />
    <link rel="canonical" href="https://qtpass.org/faq" />
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1, user-scalable=yes"
    />
    <meta name="color-scheme" content="light dark" />
    <link rel="icon" type="image/svg+xml" href="images/logo.svg{v}" />
    <link rel="icon" type="image/png" sizes="32x32" href="/images/favicon-32x32.png{v}" />
    <link rel="icon" type="image/png" sizes="16x16" href="/images/favicon-16x16.png{v}" />
    <link rel="apple-touch-icon" sizes="180x180" href="/images/apple-icon-180x180.png{v}" />
    <link rel="manifest" href="/manifest.json" />
    <meta name="theme-color" content="#ffffff" />
    <meta property="og:title" content="QtPass FAQ" />
    <meta property="og:site_name" content="QtPass" />
    <meta property="og:url" content="https://qtpass.org/faq" />
    <meta property="og:image" content="https://qtpass.org/images/og.png" />
    <meta property="og:image:width" content="1280" />
    <meta property="og:image:height" content="640" />
    <meta
      property="og:description"
      content="Frequently asked questions about QtPass: GnuPG and pinentry problems, Git on Windows, one-time passwords, where settings live, and how to help."
    />
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
            "name": "FAQ",
            "item": "https://qtpass.org/faq"
          }}
        ]
      }}
    </script>
  </head>
  <body>
    <div class="background"></div>
    <div class="wrapper">
      <aside class="sidebar">
        <header>
          <h1>QtPass FAQ <small>{version}</small></h1>
          <p>
            Answers to the questions that reach the issue tracker and the
            mailing list most often.
          </p>

          <p class="view">
            <a href="/">Back to QtPass Home</a>
          </p>
          <p>
            <small
              >Generated from
              <a href="{blob}FAQ.md">FAQ.md</a> in the repository; improve it
              there.</small
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
          <p>
            <small><a title="QtPass Sitemap" href="/sitemap">Sitemap</a></small>
          </p>
          <p>
            <small
              ><a title="QtPass privacy policy" href="/privacy"
                >Privacy</a
              ></small
            >
          </p>
        </footer>
      </aside>
      <section>
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

    # Links to sibling files in the repository.
    body = re.sub(
        r'href="(?!https?:|mailto:|#|/)([^"]+)"',
        lambda m: f'href="{REPO_BLOB}{m.group(1)}"',
        body,
    )

    seen = set()
    outline = []  # (level, slug, text)

    def heading(m):
        level, text = int(m.group(1)), m.group(2)
        slug = slugify(text, seen)
        outline.append((level, slug, text))
        return anchor(level, slug, text)

    body = re.sub(r"<h([23])>(.*?)</h\1>", heading, body)

    toc = ['<nav class="toc" aria-label="Questions">']
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
        HEAD.format(v=ASSET_VERSION, version=VERSION, blob=REPO_BLOB)
        + "\n".join(toc)
        + "\n"
        + body
        + TAIL.format(v=ASSET_VERSION)
    )


if __name__ == "__main__":
    main()
