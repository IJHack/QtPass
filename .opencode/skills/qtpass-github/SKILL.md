---
name: qtpass-github
description: QtPass GitHub interaction - PRs, issues, branches, merging
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: github
---

# QtPass GitHub Interaction

## Checking PR Status

```bash
# Check specific PR
gh pr checks <PR_NUMBER>

# Check all PRs
gh pr view <PR_NUMBER> --json state,mergeable

# View PR comments
gh api repos/IJHack/QtPass/pulls/<PR_NUMBER>/comments
```

## Creating Branches

```bash
# Create and switch to new branch
git checkout -b <branch-name>

# Push and set upstream
git push -u origin <branch-name>
```

## Creating PRs

```bash
# Create PR with title and body
gh pr create --title "Fix description (#issue)" --body "## Summary\n- Fix details\n\nFixes #issue"

# Create PR with specific base branch
gh pr create --base main --title "Fix" --body "Fixes #issue"
```

## Updating Branches

**Before pushing or merging, always update with latest main:**

```bash
# (If not already set) add upstream remote pointing to main repository
git remote add upstream https://github.com/IJHack/QtPass.git
# Fetch and rebase on main
git fetch upstream
git pull upstream main --rebase

# Force push if needed (safer)
git push --force-with-lease <push-remote> <branch>
```

This prevents "branch is out-of-date with base branch" errors.

## Signed Commits

Always sign your commits with `-S` flag:

```bash
git commit -S -m "Fix description"
```

If a PR has unsigned commits (e.g., from bots), recreate the changes on a new branch with signed commits:

```bash
# Fetch original branch
git fetch origin <branch>
git checkout FETCH_HEAD

# Make changes, commit with signing
git add -A
git commit -S -m "chore: description"

# Push and create new PR
git push -u origin new-branch-name
```

## Merging PRs

**Preferred: Squash merge for long PR threads**

Squash merging keeps the main branch history clean and avoids cluttering it with numerous intermediate commits from review iterations.

### Squash merge now via GitHub CLI

When all CI checks pass and you want to merge immediately:

```bash
# Squash merge immediately (all checks passed)
gh pr merge <PR_NUMBER> --squash --delete-branch --subject "fix: description"
```

### Schedule squash merge (waits for CI)

When you want to wait for CI to pass before merging:

```bash
# Squash merge that waits for CI checks to pass
gh pr merge <PR_NUMBER> --squash --auto --delete-branch
```

**Avoid force pushing to shared branches**

Only force push to feature branches when absolutely necessary (e.g., resolving merge conflicts, cleaning up commits). Prefer `--force-with-lease` over `-f` because it fails if someone else pushed to the branch, preventing accidental overwrites of others' work. Never use `-f` or `--force` on main or shared branches:

```bash
# Safe force push (recommended)
git push --force-with-lease <push-remote> <branch>

# Never use on main or shared branches
git push --force origin main  # AVOID THIS
```

Never force push to main or branches that others may be working from.

**Merge strategies:**

| Strategy | Use Case                                             |
| -------- | ---------------------------------------------------- |
| Squash   | Feature PRs with multiple review commits (preferred) |
| Merge    | PRs where individual commits have meaningful history |
| Rebase   | Clean, linear history (rarely used here)             |

**When to use merge (not squash):**

- Hotfixes that need individual commits for rollback
- PRs with distinct, logically separate changes

```bash
# Regular merge (when needed)
gh pr merge <PR_NUMBER> --merge --auto --delete-branch
```

## Commenting on Issues/PRs

```bash
# Comment on issue
gh issue comment <ISSUE_NUMBER> --body "Comment text"

# Comment on PR
gh pr comment <PR_NUMBER> --body "## Summary\n- Details"
```

### Using HEREDOC for Multi-line Comments

For complex comments with Markdown formatting, HEREDOC avoids shell interpretation issues:

```bash
# Comment on issue/PR using HEREDOC
gh pr comment <PR_NUMBER> --body "$(cat <<'EOF'
## Changes in this PR

### .gitignore additions
- Added test binaries to prevent accidentally committing test executables
- Added *.bak for test settings backup files

### New skill: qtpass-github
A comprehensive skill for GitHub interaction workflows:
- Reading issues and PRs
- Responding to users
- CI debugging
EOF
)"
```

**Key points:**

- Use `$(cat <<'EOF' ... EOF)` to capture the content
- Quote the `EOF` delimiter (`'EOF'`) to prevent variable expansion
- Use `\n` for newlines in inline strings (less readable)
- Use HEREDOC for complex/long comments (more readable)

## Reading Issues and PRs

```bash
# View issue details
gh issue view <ISSUE_NUMBER>

# View issue with comments
gh issue view <ISSUE_NUMBER> --comments

# View issue body only
gh issue view <ISSUE_NUMBER> --json body

# View PR details
gh pr view <PR_NUMBER>

# View PR with comments
gh pr view <PR_NUMBER> --json comments

# Get all PR comments via API
gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments

# Get all issue comments via API
gh api repos/<owner>/<repo>/issues/<ISSUE_NUMBER>/comments
```

## Answering User Questions

When responding to users on issues or PRs:

1. **Read the full context** - Check previous comments and related issues
2. **Be clear and concise** - Answer directly
3. **Provide next steps** - Let user know what to expect
4. **Use Markdown** - Format for readability

Example response templates:

```markdown
## Investigation

I've looked into this issue. The root cause is...

## Fix

I've implemented a fix that...

## Next Steps

- Review the PR when ready
- Test on your machine
- Let me know if you have questions
```

### Good Practices

- Always acknowledge user's report/feedback
- Explain technical details in simple terms
- Provide actionable next steps
- Follow up on unanswered questions
- Thank contributors for their input

## Common Patterns

### Pre-PR Checklist

```bash
# 1. Format files
npx prettier --write "**/*.md" "**/*.yml"

# 2. Verify formatting
npx prettier --check "**/*.md"

# 3. Update with main
git fetch upstream
git pull upstream main --rebase

# 4. Push
git push
```

### Post-Merge Cleanup

```bash
# Switch to main and update
git checkout main
git pull upstream main

# Delete merged branch
git branch -d <branch-name>
```

## Debugging CI Failures

When CI checks fail on GitHub:

```bash
# Get run details
gh run view <RUN_ID>

# Get full log
gh run view <RUN_ID> --log

# Filter for errors
gh run view <RUN_ID> --log | grep -iE "error|fail"

# Check specific job logs
gh run view <RUN_ID> --job <JOB_NAME> --log
```

### Common CI Failures

| Failure                   | Likely Cause           | Fix                                 |
| ------------------------- | ---------------------- | ----------------------------------- |
| Linting errors            | Formatting issues      | Run `npx prettier --write`          |
| Test failures             | Bug in code or test    | Run tests locally with `make check` |
| Build failures            | Missing deps or syntax | Build locally with `make -j4`       |
| act fails on new branches | `HEAD~0` error         | Skip act, rely on prettier check    |

## Resolving Merge Conflicts

When branch is behind main and has conflicts:

```bash
# Fetch and rebase
git fetch upstream
git checkout <branch>
git rebase upstream/main

# Resolve conflicts in editor, then:
git add <resolved-files>
git rebase --continue

# Force push (since we rewrote history; safer)
git push --force-with-lease <push-remote> <branch>
```

## Fork Workflow

If you don't have push access:

```bash
# Fork repository on GitHub first

# Add your fork as remote
git remote add myfork git@github.com:<your-username>/QtPass.git

# Push to your fork
git push -u myfork <branch-name>

# Create PR from your fork to upstream
gh pr create --base upstream/main --head <your-username>:<branch-name>
```

Note: When working with forks, use `myfork` for pushing and `upstream` for syncing with main repository.

## Troubleshooting

### "Branch is out-of-date"

```bash
git checkout <branch-name>
git pull upstream main --rebase
git push --force-with-lease <push-remote> <branch-name>
```

### Merge Failed

Check if:

1. Branch is behind main → rebase and push
2. CI still running → wait for checks
3. Conflicts → resolve locally

### Can't Merge PR

- Check branch protection rules
- Ensure all CI checks pass
- You may need admin rights to bypass some checks

### Blocked by Unresolved Review Comments

When PR shows "All comments must be resolved" but you've fixed the issues:

**1. Identify unresolved threads via GraphQL:**

```bash
gh api graphql -f query='{ repository(owner: "<owner>", name: "<repo>") { pullRequest(number: <pr_number>) { id reviewThreads(first: 20) { nodes { id isResolved } } } } }' | jq -r '.data.repository.pullRequest.reviewThreads.nodes[] | "\(.id) \(.isResolved)"'
```

**2. Resolve threads programmatically:**

```bash
# Get thread IDs and resolve them
# Use an unresolved thread ID from step 1 output (format typically starts with PRRT_)
THREAD_ID="PRRT_FROM_STEP_1"
gh api graphql -f query="mutation { resolveReviewThread(input: {threadId: \"$THREAD_ID\"}) { thread { isResolved } } }"
```

**3. Alternative: Submit a review to clear blocking comments:**

```bash
# Submit a COMMENT review (not APPROVE if it's your own PR)
gh api "repos/<owner>/<repo>/pulls/<pr_number>/reviews" -X POST -f "body"="All issues addressed in recent commits" -f "event"="COMMENT"
```

**4. Common causes:**

- Old CodeRabbit review comments not marked resolved
- Reviewer requested changes but didn't re-review after fixes
- Branch protection requires all conversations resolved

## GitHub AI-Powered Bug Detection

GitHub provides AI-generated code quality suggestions under **Security → AI findings**.

### Common AI Findings

1. **Tautology assertions** - Tests that always pass
2. **Ignored return values** - Test setup not verified
3. **Spelling corrections** - Typos in comments/strings
4. **Formatting consistency** - Backticks, spacing
5. **Contractions** - "can't" vs "cannot"
6. **Copyright years** - Use "YYYY" placeholder

### Fixing AI Findings

1. Create a branch for fixes:

   ```bash
   git checkout -b fix/ai-findings
   ```

2. Apply the suggested fixes

3. Test locally if possible

4. Push and create PR:

   ```bash
   git push -u origin fix/ai-findings
   cat <<'EOF' > /tmp/ai-findings-body.md
   ## Summary

   Found by GitHub AI-powered bug detection.

   - Fixed tautology assertions in tests
   - Added return value verification
   - Corrected spelling typos
   EOF
   gh pr create --title "fix: resolve AI findings" --body-file /tmp/ai-findings-body.md
   ```

### Checking AI Findings

```bash
# View repo security settings (requires admin)
# Go to: https://github.com/<owner>/<repo>/security/ai-findings

# Or check via API (if enabled)
gh api repos/<owner>/<repo>/code-scanning/alerts
```

## Checking PR Status Before Merging

Before merging, always verify:

```bash
# 1. Check if PR is mergeable
gh pr view <PR_NUMBER> --json state,mergeable

# 2. Check CI status
gh pr checks <PR_NUMBER>

# 3. Check for approvals
gh pr view <PR_NUMBER> --json reviews

# 4. Check if branch is up to date
gh pr view <PR_NUMBER> --json baseRefName,headRefName
```

## Pre-Merge Checklist

Before merging a PR:

- [ ] CI checks pass (`gh pr checks`)
- [ ] At least one approval (for non-trivial changes)
- [ ] Branch is up to date with main
- [ ] No unresolved conversations
- [ ] Tests pass locally (`make check`)
- [ ] Linter passes (`act push -W .github/workflows/linter.yml`)

```bash
# Run local CI checks before pushing
act push -W .github/workflows/linter.yml -j build

# Update with latest main before merging
git fetch upstream
git checkout <branch>
git pull upstream main --rebase
git push --force-with-lease <push-remote> <branch>
```
