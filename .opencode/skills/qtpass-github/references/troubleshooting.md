# Troubleshooting PRs and merges

Start with the merge state; it names the blocker:

```bash
gh pr view <PR> --json mergeable,mergeStateStatus,reviewDecision
```

| `mergeStateStatus` | Meaning                                    | Fix                                               |
| ------------------ | ------------------------------------------ | ------------------------------------------------- |
| `BEHIND`           | `main` moved; the ruleset needs up to date | Rebase (Workflow 3 in [../SKILL.md](../SKILL.md)) |
| `DIRTY`            | Merge conflicts                            | Rebase and resolve conflicts                      |
| `BLOCKED`          | Missing approval, open threads, or checks  | See the sections below                            |
| `UNSTABLE`         | A non-required check failed                | Read it; merge is allowed but check why           |
| `CLEAN`            | Ready                                      | `gh pr merge <PR> --squash --delete-branch`       |

## Missing approval

The ruleset needs one approval from someone other than the last pusher, and
every push dismisses earlier approvals. You cannot approve your own PR, and a
`COMMENT` review does not count. Ask a committer to review after your last
push. If you push again (including a rebase), ask again.

## Review threads block the merge

List the threads with their first comment, so you can verify each one:

```bash
gh api graphql -f query='
query($pr: Int!) {
  repository(owner: "IJHack", name: "QtPass") {
    pullRequest(number: $pr) {
      reviewThreads(first: 100) {
        nodes {
          id
          isResolved
          path
          comments(first: 1) { nodes { author { login } body } }
        }
      }
    }
  }
}' -F pr=<PR> --jq '.data.repository.pullRequest.reviewThreads.nodes[]
  | select(.isResolved | not)
  | "\(.id) \(.path) \(.comments.nodes[0].author.login)"'
```

Resolve a thread only after you fixed the finding or replied why it is a false
positive:

```bash
gh api graphql -f query='
mutation($id: ID!) {
  resolveReviewThread(input: {threadId: $id}) { thread { isResolved } }
}' -f id=<PRRT_THREAD_ID>
```

Verify: run the list query again; it prints nothing when all threads are
resolved.

## A bot pushed unsigned commits

CodeRabbit (or another bot) can push a commit to your branch. It has no GPG
signature, so the ruleset blocks the merge (and no sign-off, which
CONTRIBUTING requires). Re-sign and sign off every commit on the branch:

```bash
git checkout <branch>
git pull --rebase origin <branch>
git fetch origin
git rebase --force-rebase --signoff -S origin/main
git log --show-signature origin/main..HEAD   # every commit: good signature + Signed-off-by
git push --force-with-lease origin <branch>
```

Read the bot's change before you sign it off: your sign-off certifies it.

## Push rejected

- **To `main`:** expected. Push a branch and open a PR.
- **`stale info` from `--force-with-lease`:** someone (often a bot) pushed in
  the meantime. Run `git pull --rebase origin <branch>`, check what arrived,
  then push again.
- **Non-fast-forward without force:** you rebased locally. Use
  `--force-with-lease`, never `--force`.

## Checks do not start or stay pending

- Lint, fuzz and CodeQL jobs skip their heavy steps when no relevant files
  changed; the required check still reports success.
- A check that stays "Expected" after its workflow finished usually means the
  job name changed. Compare the names in `.github/workflows/` with the required
  checks listed in [../SKILL.md](../SKILL.md); only a maintainer can change the
  ruleset.
- Re-run a failure only after the log shows it is flaky:
  `gh run rerun <RUN_ID> --failed`.
