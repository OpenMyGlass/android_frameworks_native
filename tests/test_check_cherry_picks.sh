#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# OpenMyGlass: tests/check_cherry_picks.sh on a throwaway repository: an unchanged cherry-pick
# passes; a changed patch, another author, a source off the upstream branch and a commit that is
# not a cherry-pick fail.
set -eu
script=$(cd "$(dirname "$0")" && pwd)/check_cherry_picks.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
git init -q -b main .
export GIT_COMMITTER_NAME=ci GIT_COMMITTER_EMAIL=ci@example.org
up() { GIT_AUTHOR_NAME=Up GIT_AUTHOR_EMAIL=up@example.org GIT_AUTHOR_DATE='1500000000 +0000' "$@"; }
echo a > f && git add f && up git commit -q -m base
base=$(git rev-parse HEAD)
git checkout -q -b upstream
echo b >> f && up git commit -q -a -m fix
fix=$(git rev-parse HEAD)
git checkout -q -b other "$base"
echo c > g && git add g && up git commit -q -m elsewhere
other=$(git rev-parse HEAD)
fails=0
expect() { # expect <ok|fail> <what>
    if sh "$script" "$base" upstream > out.txt 2>&1; then got=ok; else got=fail; fi
    if [ "$got" = "$1" ]; then echo "ok   $2"; else echo "FAIL $2 (got $got)"; cat out.txt; fails=1; fi
}
reset() { git checkout -q -B pick "$base"; }

reset; git cherry-pick -x "$fix" >/dev/null; expect ok "unchanged cherry-pick"
reset; mkdir -p tests && echo t > tests/t.sh && git add tests && git commit -q -m tests
expect ok "our tests/ commit"
reset; git cherry-pick -x "$fix" >/dev/null; echo d >> f; git commit -q -a --amend --no-edit
expect fail "changed patch"
reset; git cherry-pick -x "$fix" >/dev/null
git commit -q --amend --no-edit --author='Someone <s@example.org>'
expect fail "other author"
reset; git cherry-pick -x "$fix" >/dev/null
git commit -q --amend --no-edit --date='1600000000 +0000'
expect fail "other author date"
reset; git cherry-pick -x "$other" >/dev/null; expect fail "source not on the upstream branch"
reset; echo e >> f && git commit -q -a -m "not a pick"; expect fail "not a cherry-pick"
exit $fails
