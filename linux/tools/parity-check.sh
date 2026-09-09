#!/bin/sh
# parity-check.sh — the web is the reference; this fails when the C side drifts:
#   1. every web component (../web/src/components/<Name>) has src/components/<Name>/<Name>.c
#      and a README.md, and every C component has a web component;
#   2. every web source file under src/lib and src/desktop (tests, CSS and index files
#      aside) is named in PARITY.md, so a new file there gets a row (even a ✗ one).
# Run it from anywhere (`make parity`). Exit status = number of problems.
set -u
here=$(cd "$(dirname "$0")/.." && pwd)
web=$here/../web/src
fail=0
problem() { echo "parity: $*"; fail=$((fail + 1)); }
[ -d "$web" ] || { echo "parity: no web checkout at $web"; exit 1; }

for dir in "$web"/components/*/; do
    name=$(basename "$dir")
    case $name in
    SvgDefs) continue ;; # SVG filter definitions: their numbers live in src/draw and GooGroup (PARITY.md)
    esac
    [ -f "$here/src/components/$name/$name.c" ] || problem "web component $name has no src/components/$name/$name.c"
    [ -f "$here/src/components/$name/README.md" ] || problem "web component $name has no src/components/$name/README.md"
done
for dir in "$here"/src/components/*/; do
    name=$(basename "$dir")
    [ -d "$web/components/$name" ] || problem "C component $name has no web component"
done

for f in $(cd "$web" && find lib desktop -type f \( -name '*.ts' -o -name '*.tsx' \) ! -name '*.test.ts' ! -name 'index.ts' | sort); do
    grep -q -F "$f" "$here/PARITY.md" || problem "PARITY.md does not mention web/src/$f"
done

if [ "$fail" -eq 0 ]; then
    n=$(ls -d "$here"/src/components/*/ | wc -l | tr -d ' ')
    echo "parity: ok — $n components mirrored, PARITY.md covers src/lib and src/desktop"
fi
exit $fail
