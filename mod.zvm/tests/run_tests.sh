#!/bin/sh
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK=${TMPDIR:-/tmp}/zhirvm-tests-$$
CC=${CC:-gcc}

mkdir -p "$WORK" || exit 1
trap 'rm -rf "$WORK"' EXIT INT TERM

SOURCES="builtins init interpret lexer map parse vector"

if ! "$CC" -m32 -O0 -w -I"$ROOT" -I"$ROOT/../lib" -I"$ROOT/tests" \
    -o "$WORK/runner" \
    "$ROOT/tests/runner.c" \
    "$ROOT/builtins.c" \
    "$ROOT/interpret.c" \
    "$ROOT/lexer.c" \
    "$ROOT/map.c" \
    "$ROOT/parse.c" \
    "$ROOT/vector.c"
then
    echo "failed to build the ZhirScript test runner (is gcc -m32 multilib available?)" >&2
    exit 1
fi

for src in $SOURCES; do
    if ! "$CC" -m32 -O0 -fno-stack-protector -ffreestanding -fPIC \
        -fno-common -w -c "$ROOT/$src.c" -o "$WORK/$src.o"
    then
        echo "failed to build the flat test module" >&2
        exit 1
    fi
done

sed 's/\. = 0x0;/. = 0x10000;/' "$ROOT/../ld/module.ld" > "$WORK/module.ld"

if ! ld -m elf_i386 -T "$WORK/module.ld" \
    "$WORK/builtins.o" \
    "$WORK/init.o" \
    "$WORK/interpret.o" \
    "$WORK/lexer.o" \
    "$WORK/map.o" \
    "$WORK/parse.o" \
    "$WORK/vector.o" \
    -o "$WORK/module.elf"
then
    echo "failed to link the flat test module" >&2
    exit 1
fi

objcopy --set-section-flags .bss=alloc,load,contents \
    -O binary "$WORK/module.elf" "$WORK/module.mod"

if ! "$CC" -m32 -O0 -w \
    -o "$WORK/flat_loader" "$ROOT/tests/flat_loader.c"
then
    echo "failed to build the flat module loader" >&2
    exit 1
fi

failed=0
total=0

run_case() {
    suite=$1
    name=$2
    expected="$ROOT/tests/expected/$name.out"
    actual="$WORK/$suite-$name.out"
    total=$((total + 1))
    shift 3

    "$@" >"$actual" 2>"$WORK/$suite-$name.err"

    if ! diff -u "$expected" "$actual"; then
        echo "FAIL: $suite/$name" >&2
        sed 's/^/    /' "$WORK/$suite-$name.err" >&2
        failed=$((failed + 1))
    else
        echo "PASS: $suite/$name"
    fi
}

for script in "$ROOT"/tests/scripts/*.zo; do
    name=$(basename "$script" .zo)
    run_case host "$name" "$script" "$WORK/runner" "$script"
    run_case flat "$name" "$script" "$WORK/flat_loader" "$script" "$WORK/module.mod"
done

if [ "$failed" -ne 0 ]; then
    echo "$failed of $total test(s) failed" >&2
    exit 1
fi

echo "all $total test(s) passed"
