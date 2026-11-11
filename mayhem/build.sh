#!/usr/bin/env bash
# Builds the sanitized apriltag library + fuzz harnesses (and standalone reproducers), and, with the
# project's NORMAL flags, upstream's own test programs for mayhem/test.sh. Runs in the commit image as
# `mayhem` in /mayhem; re-runnable and network-free.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"

# 1) Sanitized, SanCov-instrumented static library (upstream's CMake, Release = upstream's default).
#    build/ is gitignored upstream, so `git clean -ffdX` removes it.
#    -fno-sanitize=shift: pjpeg.c does `(-1) << ssss` and `1 << (ssss-1)` (ssss == 0) for every DC
#    coefficient (benign on clang/x86), which would halt on any valid JPEG and stall fuzz-pjpeg at the
#    header parser; all other UBSan checks stay on.
rm -rf build/fuzz
cmake -S . -B build/fuzz -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PYTHON_WRAPPER=OFF \
      -DCMAKE_C_COMPILER="$CC" \
      -DCMAKE_C_FLAGS="$SANITIZER_FLAGS -fno-sanitize=shift -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
cmake --build build/fuzz --target apriltag -j"$MAYHEM_JOBS"

# 2) Harnesses: libFuzzer binary + standalone run-once reproducer, LeakSanitizer off at build time.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/standalone_main.o
for h in fuzz_detect fuzz_pjpeg fuzz_pnm fuzz_pam; do
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $LIB_FUZZING_ENGINE -I"$SRC" "mayhem/$h.c" build/lsan_off.o \
      build/fuzz/libapriltag.a -lm -lpthread -o "$SRC/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -I"$SRC" "mayhem/$h.c" build/standalone_main.o build/lsan_off.o \
      build/fuzz/libapriltag.a -lm -lpthread -o "$SRC/$h-standalone"
done

# 3) Upstream's test programs, project's NORMAL flags (no sanitizers, no -gdwarf-3).
rm -rf build/tests
cmake -S . -B build/tests -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PYTHON_WRAPPER=OFF \
      -DCMAKE_C_COMPILER="$CC" -DCMAKE_C_FLAGS="$COVERAGE_FLAGS"
cmake --build build/tests --target test_detection test_tag_pose_estimation test_quick_decode -j"$MAYHEM_JOBS"
