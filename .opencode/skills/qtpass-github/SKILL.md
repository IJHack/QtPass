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
gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments
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
# Fetch and rebase on main
git fetch upstream
git pull upstream main --rebase

# Force push if needed
git push -f
```

This prevents "branch is out-of-date with base branch" errors.

## Merging PRs

```bash
# Merge via GitHub CLI (if you have admin rights)
gh pr merge <PR_NUMBER> --admin --merge

# Or squash merge
gh pr merge <PR_NUMBER> --squash --auto --delete-branch
```

## Commenting on Issues/PRs

```bash
# Comment on issue
gh issue comment <ISSUE_NUMBER> --body "Comment text"

# Comment on PR
gh pr comment <PR_NUMBER> --body "## Summary\n- Details"
```

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

## Troubleshooting

### "Branch is out-of-date"

```bash
git checkout <branch-name>
git pull upstream main --rebase
git push -f
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
