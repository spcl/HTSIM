# How to (remove this section before creating the PR)

## Checklist 
- PR not too large?
- Should this code go into the repo as is? If not, please consider a "draft PR".
- Run commit checks: htsim/sim/datacenter/commit_check.sh and include the new validate_outputs results in the PR.

## Feedback and discussion
- Please fix/update issues in the PR directly by updating the branch to be merged.


# Reminder: What to put where (remove this header before creating the PR, keep the "Description of changes" heading)
- Title
 - Put summary in headline, include key information here
 - Prepend with "DRAFT: " if this PR is informational only and not for merging

# Description of changes
- Add list of changes here
- How to run a new feature (if applicable)


# PR Impact Assessment (required, needs to be part of the PR description)

## Interaction with CMS specification 
"None", "Update code to match spec", "Needs spec update"

## Impact on default behavior
"None", "Modify existing features or parameters", "Add new feature not enabled by default", "Change default feature configuration"

## Impact on performance
On performance, as evidenced by the simulation results included in the PR: point reviewers to results and summarize differences


# Merging instructions

## PR dependencies
List PRs that need to be merged before this one or write "DRAFT" for an informational PR.

## Should the PR commit history be kept (make sure it's clean)
Yes ("rebase and merge")/no ("squash and merge"), NA if this PR is informational


# This PR needs approval from at least one person per row (creating the PR counts as approval)
AMD: @yfle0707, @jgjl
Broadcom: @mhandley, @craiciu
Microsoft: @aghalayini, @tommasobo