#!/bin/sh -eu
#
# CI Version String Generator
#
# Output format:
#   Tag (release):  v1.0.0
#   Master branch:  master-<short_sha>
#   Feature branch: <tag>-<commits>-<branch>-<commits>-<short_sha> (same as local)
#
# Environment variables (from GitHub Actions):
#   GITHUB_HEAD_REF  - source branch for PRs
#   GITHUB_REF_NAME  - branch/tag name
#   GITHUB_REF_TYPE  - "tag" or "branch"
#

sanitize_version() {
  echo "$1" | sed -E 's/[^a-zA-Z0-9.+~:-]/-/g'
}

cd "$(dirname "$0")/../.."

SANITIZED=false
[ "$#" -gt 0 ] && [ "$1" = "--sanitized" ] && SANITIZED=true

# For tags — just return tag name
if [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
  RESULT="${GITHUB_REF_NAME}"
  [ "$SANITIZED" = true ] && RESULT=$(sanitize_version "$RESULT")
  printf "%s" "$RESULT"
  exit 0
fi

if [ -x "$(command -v git)" ] && [ -d "$(git rev-parse --git-dir 2>/dev/null)" ]; then
  HEAD=$(git rev-parse --short HEAD)

  # Determine the main branch
  MAIN_BRANCH=$(git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@')
  if [ -z "$MAIN_BRANCH" ]; then
    MAIN_BRANCH=$(git branch -r 2>/dev/null | grep -E 'origin/(main|master)$' | head -1 | sed 's|.*/||')
  fi
  [ -z "$MAIN_BRANCH" ] && MAIN_BRANCH="master"

  # Get branch name: git first, then GitHub env for detached HEAD
  BRANCH=$(git branch --show-current 2>/dev/null)
  if [ -z "$BRANCH" ]; then
    BRANCH="${GITHUB_HEAD_REF:-${GITHUB_REF_NAME:-}}"
  fi

  # For master/main — simple format: master-<sha>
  if [ "$BRANCH" = "$MAIN_BRANCH" ] || [ "$BRANCH" = "main" ] || [ "$BRANCH" = "master" ]; then
    RESULT="${BRANCH}-${HEAD}"
  else
    # For feature branches — full format like local get_version.sh
    COMMON=$(git merge-base HEAD "origin/$MAIN_BRANCH" 2>/dev/null || git merge-base HEAD "$MAIN_BRANCH" 2>/dev/null || echo "$HEAD")

    DESCR=$(git describe --tags --long "$COMMON" 2>/dev/null || echo "")
    [ -z "$DESCR" ] && DESCR="$HEAD-0-g$HEAD"

    TAG_IN_MASTER=$(echo "$DESCR" | sed -E "s/v?(.*)-([0-9]+)-g[a-f0-9]+/\1/")
    TAG_TO_FORK_DISTANCE=$(echo "$DESCR" | sed -E "s/v?(.*)-([0-9]+)-g[a-f0-9]+/\2/")
    FORK_TO_HEAD_DISTANCE=$(git rev-list --count "$COMMON..HEAD" 2>/dev/null || echo "0")

    RESULT=$TAG_IN_MASTER
    [ "$TAG_TO_FORK_DISTANCE" != "0" ] && RESULT="$RESULT-$TAG_TO_FORK_DISTANCE"
    RESULT="$RESULT-$BRANCH-$FORK_TO_HEAD_DISTANCE-$HEAD"
  fi
else
  RESULT="Unknown(no git)"
fi

[ "$SANITIZED" = true ] && RESULT=$(sanitize_version "$RESULT")

printf "%s" "$RESULT"
