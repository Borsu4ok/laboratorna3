#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/cparser"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

GREEN=$'\033[32m'
RED=$'\033[31m'
YELLOW=$'\033[33m'
RESET=$'\033[0m'

if [ ! -x "$BIN" ]; then
    echo "build/cparser not found - run 'make' first"
    exit 1
fi

HAVE_NODE=0
command -v node >/dev/null 2>&1 && HAVE_NODE=1
HAVE_GXX=0
command -v g++ >/dev/null 2>&1 && HAVE_GXX=1

passed=0
failed=0

mark() {
    if [ "$1" = "ok" ]; then
        printf "%s%-12s%s" "$GREEN" "OK" "$RESET"
    elif [ "$1" = "skip" ]; then
        printf "%s%-12s%s" "$YELLOW" "skipped" "$RESET"
    else
        printf "%s%-12s%s" "$RED" "FAIL" "$RESET"
    fi
}

echo "== Valid programs: g++ output vs our interpreter vs generated JavaScript =="
printf "%-24s %-12s %-12s %-12s\n" "program" "g++" "interpreter" "js (node)"
for src in "$ROOT"/examples/*.cpp; do
    name="$(basename "$src" .cpp)"
    expected="$TMP/$name.expected"
    gxx_state="skip"
    if [ "$HAVE_GXX" = 1 ]; then
        if g++ -std=c++17 -w -o "$TMP/$name.bin" "$src" && "$TMP/$name.bin" > "$expected"; then
            gxx_state="ok"
        else
            gxx_state="fail"
        fi
    fi

    "$BIN" "$src" --run > "$TMP/$name.interp" 2> "$TMP/$name.interp.err"
    interp_rc=$?
    if [ "$gxx_state" = "ok" ]; then
        if [ "$interp_rc" = 0 ] && cmp -s "$expected" "$TMP/$name.interp"; then interp_state="ok"; else interp_state="fail"; fi
    else
        if [ "$interp_rc" = 0 ]; then interp_state="ok"; else interp_state="fail"; fi
        cp "$TMP/$name.interp" "$expected"
    fi

    js_state="skip"
    if [ "$HAVE_NODE" = 1 ]; then
        if "$BIN" "$src" --js "$TMP/$name.js" 2>/dev/null && node "$TMP/$name.js" > "$TMP/$name.jsout" 2>&1 \
            && cmp -s "$expected" "$TMP/$name.jsout"; then
            js_state="ok"
        else
            js_state="fail"
        fi
    fi

    printf "%-24s " "$name"; mark "$gxx_state"; printf " "; mark "$interp_state"; printf " "; mark "$js_state"; printf "\n"
    for state in "$gxx_state" "$interp_state" "$js_state"; do
        [ "$state" = "fail" ] && failed=$((failed + 1))
        [ "$state" = "ok" ] && passed=$((passed + 1))
    done
    if [ "$interp_state" = "fail" ]; then
        diff "$expected" "$TMP/$name.interp" | head -10
        cat "$TMP/$name.interp.err"
    fi
    if [ "$js_state" = "fail" ]; then
        diff "$expected" "$TMP/$name.jsout" | head -10
    fi
done

echo
echo "== Programs with errors: expected exit code and first diagnostic =="
check_error() {
    local file="$1" expected_rc="$2"
    "$BIN" "$ROOT/examples/errors/$file" --run > "$TMP/err.out" 2> "$TMP/err.txt"
    local rc=$?
    printf "%-24s exit=%d (expected %d) " "$file" "$rc" "$expected_rc"
    if [ "$rc" = "$expected_rc" ]; then mark ok; passed=$((passed + 1)); else mark fail; failed=$((failed + 1)); fi
    printf "\n"
    sed 's/^/    /' "$TMP/err.txt" | head -20
}
check_error syntax_error.cpp 1
check_error semantic_error.cpp 2
check_error runtime_error.cpp 3

echo
echo "passed: $passed, failed: $failed"
[ "$failed" = 0 ]
