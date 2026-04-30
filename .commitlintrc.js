// Conventional Commits — see https://www.conventionalcommits.org/
// Standard preset is bundled with super-linter; no npm install needed.
module.exports = {
  extends: ["@commitlint/config-conventional"],

  // Skip auto-generated messages that don't follow conventional format.
  // Weblate's translation-sync commits ("Translated using Weblate (X)")
  // are pushed by the weblate bot via PR; they're squash-merged into a
  // proper conventional message at merge time, but commitlint runs on
  // each individual commit in the PR range and would otherwise fail.
  ignores: [
    (commit) => commit.startsWith("Translated using Weblate"),
    (commit) => commit.startsWith("Translation update from Weblate"),
  ],

  rules: {
    // We don't enforce strict line lengths on commit message bodies/footers
    // — diff snippets, long URLs and Co-Authored-By trailers regularly
    // exceed the 100-char default. Subjects/headers stay capped to keep
    // `git log --oneline` readable.
    "body-max-line-length": [0, "always", 0],
    "footer-max-line-length": [0, "always", 0],

    // Add `i18n` to the standard 11 conventional types — already in active
    // use in this repo for translation-related commits and Weblate sync PRs.
    "type-enum": [
      2,
      "always",
      [
        "build",
        "chore",
        "ci",
        "docs",
        "feat",
        "fix",
        "i18n",
        "perf",
        "refactor",
        "revert",
        "style",
        "test",
      ],
    ],
  },
};
