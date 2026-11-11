#!/usr/bin/env bash
#
# mayhem/test.sh — RUNS upstream's own test programs (built by mayhem/build.sh) and asserts on their output.
# Emits a CTRF report (file + `CTRF {...}` stdout marker); exit 0 iff nothing failed. Does not compile.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Runs upstream's three test programs (built by mayhem/build.sh into build/tests/test) and asserts on
# their OUTPUT, not just their exit status: a program neutered to exit(0) prints nothing and fails here.
T="$SRC/build/tests/test"
for b in test_detection test_tag_pose_estimation test_quick_decode; do
  [ -x "$T/$b" ] || { echo "missing $T/$b (mayhem/build.sh should have built it)" >&2; emit_ctrf "apriltag-tests" 0 1 0; exit 1; }
done

passed=0; failed=0
ok()   { passed=$((passed + 1)); echo "PASS $1"; }
bad()  { failed=$((failed + 1)); echo "FAIL $1: $2" >&2; }

cd "$SRC/test"
for img in 33369213973_9d9bb4cc96_c 34085369442_304b6bafd9_c 34139872896_defdb2f8d9_c; do
  # known-answer: every expected corner line must be detected and printed as "Got:" (the program itself
  # compares each against the "Expected:" line and exits non-zero on a mismatch)
  want=$(grep -c . "data/$img.txt")
  out=$("$T/test_detection" "data/$img" 2>&1); rc=$?
  got=$(printf '%s\n' "$out" | grep -c '^Got: ')
  if [ "$rc" -eq 0 ] && [ "$want" -gt 0 ] && [ "$got" -eq "$want" ]; then ok "detection $img ($got tags)"
  else bad "detection $img" "rc=$rc got=$got want=$want"; fi

  out=$("$T/test_tag_pose_estimation" "data/$img.jpg" "data/$img.txt" 2>&1); rc=$?
  if [ "$rc" -eq 0 ] && printf '%s\n' "$out" | grep -q 'All pose estimations successful!' \
     && printf '%s\n' "$out" | grep -q "Successful pose estimations: $want/$want"; then ok "pose $img"
  else bad "pose $img" "rc=$rc"; fi
done

out=$("$T/test_quick_decode" 2>&1); rc=$?
nfam=$(printf '%s\n' "$out" | grep -c '^Family .* passed\.$')
if [ "$rc" -eq 0 ] && [ "$nfam" -eq 26 ] && printf '%s\n' "$out" | grep -q 'All quick_decode tests passed!'; then ok "quick_decode ($nfam families)"
else bad "quick_decode" "rc=$rc families=$nfam"; fi

emit_ctrf "apriltag-tests" "$passed" "$failed" 0
