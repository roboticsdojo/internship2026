
# Contributing guidelines

This document defines how all work is proposed, reviewed, and merged in the `internship2026` monorepo. Read it fully. 

------------------

## 1. Jira workflow

**No ticket, no branch.**

Before writing a single line of code, check that a Jira issue exists for the work. If it does not, create one. 

The Jira project key for this programme is `RDI`. Every ticket has a unique key in the format `RDI-XXX` (e.g. `RDI-42`). This key must appear in your branch name, your commit messages, and your PR title. 
- When it does, Jira automatically links your commits, branches, and PRs to the ticket.

**Ticket status transitions (automated):**

| Event | Jira status |
|---|---|
| Branch created with ticket key | In Progress |
| PR opened with ticket key in title | In Review |
| PR merged to `main` | Done |

If a ticket does not transition automatically, check that the key appears correctly in the branch name and PR title.

----------

## 2. Branch naming

Branch names follow this exact format:

```
<type>/<JIRA-KEY>-<short-description>

```

**Types:**

| Type | When to use |
|---|---|
| `feature` | New functionality, a new ROS2 node, or a new capability |
| `fix` | Correcting a bug or a broken behaviour |
| `chore` | Maintenance work — dependencies, configs, CI, docs |
| `experiment` | Work in `playground/` that may not be merged |

**Examples:**

```
feature/DOJO-42-add-slam-config-robot1
feature/DOJO-67-implement-coverage-path-planner
fix/DOJO-87-nav2-costmap-drift-corridor
fix/DOJO-91-tf2-tree-broken-on-startup
chore/DOJO-103-update-nav2-dependencies
chore/DOJO-110-add-colcon-gitignore
experiment/DOJO-55-test-rtabmap-vs-slam-toolbox

```

**Rules:**

-   All lowercase, hyphens only — no underscores, no spaces, no slashes after the first
-   The short description should be readable in isolation: `fix/DOJO-87-nav2-costmap-drift-corridor` has more information than `fix/DOJO-87-bugfix`
-   Never work directly on `main` — branch protection rules enforce this, but the rule exists regardless

----------

## 3. Commit messages

Commit messages follow this format:

```
RDI-XXX: <short summary in imperative mood>

<optional body — explain why, not what>

```

**The first line (summary):**

-   Start with the Jira key, then a colon and a space
-   Use the imperative mood: `add`, `fix`, `remove`, `refactor`, `update` — not `added`, `fixes`, or `adding`
-   Capitalise the first word after the colon
-   Keep it under 72 characters
-   No full stop at the end

**The body (optional but encouraged for non-trivial changes):**

-   Leave a blank line between the summary and the body
-   Explain _why_ the change was made, not _what_ changed — the diff shows what changed
-   Note any trade-offs, known limitations, or follow-up tickets

**What makes a bad commit message:**

```
# Bad — vague, no ticket, past tense
fixed stuff

# Bad — what, not why; no ticket
updated slam params

# Bad — correct ticket but summary tells you nothing
DOJO-87: bug fix

# Bad — too long, wraps in terminal
DOJO-42: Add the slam_toolbox configuration file for robot 1 with all the parameters needed for the competition field size and loop closure settings

```

**Commit granularity:** Each commit should represent one logical change that could stand alone. A commit that both fixes a bug and adds a feature should be two commits. Avoid "work in progress" commits on `main`-bound branches — squash them before opening a PR if needed.

----------

## 4. Opening a pull request

Before you open a PR, run through this checklist locally. Reviewers will check these and will ask you to fix them before reviewing the code itself.

```
[ ] colcon build passes with no errors or warnings
[ ] colcon test passes — all tests green
[ ] No generated files staged (build/, install/, log/, __pycache__/, *.pyc)
[ ] All new parameters are declared in code and documented in config/*.yaml
[ ] No hardcoded values in source files
[ ] Topics follow the /robot_X/... namespace convention
[ ] Package has a README.md describing its responsibility and topics
[ ] Branch name includes the Jira ticket key (RDI-XXX)
[ ] PR title includes the Jira ticket key in square brackets

```

If any item is not checked, fix it before requesting review. A PR that does not build is closed without review.

----------

## 5. PR title and description

**Title format:**

```
[RDI-XXX] Short description of what this PR does

```

**Examples:**

```
[RDI-42] Add SLAM configuration for robot_1 competition field
[RDI-87] Fix Nav2 costmap drift on featureless corridor
[RDI-103] Update rosdep sources to include vision_msgs
[RDI-67] Implement boustrophedon coverage path planner for robot_2

```

**Description template** — use this for every PR:

```markdown
## What this PR does
<!-- One paragraph. What is the change and why was it needed? -->

## How to test it
<!-- Exact commands or steps a reviewer can follow to verify this works. -->
<!-- If it requires physical hardware, say so and describe what to observe. -->

## Screenshots / logs (if applicable)
<!-- rviz screenshots, ros2 topic echo output, test run scores — anything that shows it working. -->

## Related tickets
<!-- RDI-XXX — closes / related to / follow-up for -->

## Checklist
- [ ] `colcon build` passes
- [ ] `colcon test` passes
- [ ] No generated files committed
- [ ] Follows naming conventions in `robot_software/README.md`
- [ ] Package README updated if behaviour changed

```

The "How to test it" section is the most important part of a PR description. If a reviewer cannot reproduce your change, they cannot review it properly. A PR without test instructions will be sent back.

----------

## 6. Review process

**Who reviews what:**

-   All PRs into `main` require at least **one approval** from another team member
-   PRs touching `robot_1_ws/` should be reviewed by the robot_1 lead
-   PRs touching `robot_2_ws/` should be reviewed by the robot_2 lead
-   PRs touching root-level files (`.gitignore`, `CONTRIBUTING.md`, CI configs) require programme lead approval

**Reviewer responsibilities:**

When you are asked to review a PR, you are responsible for:

-   Checking that the build and tests pass (do not approve if CI is red)
-   Verifying the naming conventions are followed
-   Running the "How to test it" steps if you have access to the hardware or simulation
-   Leaving specific, actionable comments — not "this looks wrong" but "this parameter should be declared with `declare_parameter()` rather than read directly, see `robot_software/README.md`"

**Author responsibilities when receiving review comments:**

-   Address every comment with either a code change or a written explanation of why you disagree
-   Do not resolve comment threads yourself — the reviewer resolves them after confirming the change
-   Do not push unrelated changes to a PR that is under review — open a new branch

**Review turnaround:** Reviews should be completed within **24 hours** of being requested. If you cannot review within 24 hours, say so in the PR thread so the author is not blocked.

----------

## 7. Merging

-   Only merge after at least one approval and all CI checks pass
-   Use **squash and merge** for feature and fix branches — this keeps `main`'s history clean and readable
-   Use **merge commit** only for long-running branches where preserving individual commit history matters (rare)
-   The PR author merges their own PR after approval — not the reviewer
-   Delete the source branch after merging (GitHub will offer this automatically)

**Never force-push to `main`.** If `main` has a bad commit, revert it with a new commit — do not rewrite history.

----------

## 8. After merging

-   Confirm the Jira ticket has transitioned to Done automatically. If it has not, move it manually and check that the PR title contained the correct key
-   Delete your local branch: `git branch -d feature/RDI-42-add-slam-config-robot1`
-   Update your local `main`: `git checkout main && git pull`
-   If your change affects how another team's robot is built or run, message them directly — do not assume they will notice the PR

----------

## 9. What not to commit

The root `.gitignore` covers most of these automatically, but understand why each is excluded.

If you find yourself wanting to commit something not covered by the `.gitignore`, ask in the team WhatsApp group before adding it

----------

## Quick reference card

```
1.  Check Jira ticket exists            → DOJO-XXX
2.  Create branch                       → feature/DOJO-XXX-short-description
3.  Commit with key                     → DOJO-XXX: describe the change
4.  Run build + tests locally           → colcon build && colcon test
5.  Open PR with key in title           → [DOJO-XXX] Short description
6.  Fill in PR description template     → what, how to test, related tickets
7.  Wait for review + CI green          → respond to all comments
8.  Squash and merge after approval     → delete branch
9.  Confirm Jira ticket → Done          → move manually if auto-transition failed

```