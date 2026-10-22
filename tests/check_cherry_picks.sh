#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# OpenMyGlass: every commit on top of the AOSP base is either one of ours that only touches
# .github/ or tests/, or an unchanged cherry-pick: it names its source with
# "(cherry picked from commit <sha>)", the source is in this repository, and both have the same
# patch id. Run from the repository root, with full history:
#   sh tests/check_cherry_picks.sh <base-commit>
set -eu
base=$1
fail=0
for c in $(git rev-list --reverse --no-merges "$base..HEAD"); do
    subject=$(git log -1 --format=%s "$c")
    if ! git diff-tree --no-commit-id --name-only -r "$c" | grep -q -v -e '^\.github/' -e '^tests/'; then
        echo "ok   $c ours (.github/, tests/ only): $subject"
        continue
    fi
    src=$(git log -1 --format=%B "$c" | sed -n 's/^(cherry picked from commit \([0-9a-f]\{40\}\))$/\1/p' | tail -n 1)
    if [ -z "$src" ]; then
        echo "FAIL $c: not a cherry-pick and touches more than .github/ and tests/: $subject"
        fail=1
        continue
    fi
    if ! git cat-file -e "$src^{commit}" 2>/dev/null; then
        echo "FAIL $c: source $src is not in this repository"
        fail=1
        continue
    fi
    a=$(git show "$c" | git patch-id --stable | cut -d' ' -f1)
    b=$(git show "$src" | git patch-id --stable | cut -d' ' -f1)
    if [ "$a" = "$b" ]; then
        echo "ok   $c = $src: $subject"
    else
        echo "FAIL $c: differs from its source $src: $subject"
        fail=1
    fi
done
exit $fail
