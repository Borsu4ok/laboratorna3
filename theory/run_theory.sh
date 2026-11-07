#!/usr/bin/env bash
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../build/theory"
mkdir -p "$OUT"

CEX=""
if bison -Wcounterexamples --version >/dev/null 2>&1; then
    CEX="-Wcounterexamples"
fi

for grammar in "$HERE"/*.y; do
    name="$(basename "$grammar" .y)"
    echo "================================================================"
    echo "  $name.y"
    echo "================================================================"
    cat "$grammar"
    echo "---------------- bison report ----------------"
    bison $CEX --report=state,itemset,lookahead,solved --graph="$OUT/$name.dot" \
        -o "$OUT/$name.tab.c" "$grammar" 2>&1 | sed "s|$grammar|$name.y|g"
    if ! grep -q "conflict" "$OUT/$name.output"; then
        echo "no unresolved conflicts"
    fi
    grep -E "^State [0-9]+ conflicts|resolved as" "$OUT/$name.output" | head -20
    if command -v dot >/dev/null 2>&1; then
        dot -Tsvg "$OUT/$name.dot" -o "$OUT/$name.svg"
        echo "automaton graph: build/theory/$name.svg"
    fi
    echo
done
echo "Full LALR(1) automaton descriptions: build/theory/*.output"
