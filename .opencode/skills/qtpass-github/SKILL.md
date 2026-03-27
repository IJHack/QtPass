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

## Fork Workflow

If you don't have push access:

```bash
# Fork repository on GitHub first

# Add your fork as remote
git remote add myfork git@github.com:<your-username>/QtPass.git

# Push to your fork
git push -u myfork <branch-name>

# Create PR from your fork to upstream
gh pr create --base upstream/main --head your-fork:branch
```

Note: When working with forks, use `myfork` for pushing and `upstream` for syncing with main repository.

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
   gh pr create --title "fix: resolve AI findings" --body "## Summary

   Found by GitHub AI-powered bug detection.

   - Fixed tautology assertions in tests
   - Added return value verification
   - Corrected spelling typos"
   ```

### Checking AI Findings

```bash
# View repo security settings (requires admin)
# Go to: https://github.com/<owner>/<repo>/security/ai-findings

# Or check via API (if enabled)
gh api repos/<owner>/<repo>/code-scanning/alerts
```
