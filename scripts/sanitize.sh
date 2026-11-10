#!/usr/bin/env bash
# Builds the addon with AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer (Linux, GCC) and runs two
# things against it: scripts/sanitize-run.js -- every fixture through every API path, plus any folders of PDFs given
# as arguments -- and a round of scripts/fuzz.mjs, which feeds it mutated fixtures. Fails if a sanitizer reports
# anything in poppler-native's own code or the libraries it bundles, or if either run does not finish. Node itself is
# not instrumented, so leaks and reports that don't involve the addon are filtered out.
#
#   scripts/sanitize.sh [folder of PDFs ...]
#
# The instrumented build is made in build/, where the normal one lives; the build that was there before is put back
# when the script ends, however it ends.
set -euo pipefail
cd "$(dirname "$0")/.."

logs=$(mktemp -d)
saved=""
if [ -d build ]; then
  saved=$(mktemp -d "$PWD/.build-before-sanitize.XXXXXX")
  mv build "$saved/build"
fi
restore() {
  rm -rf build "$logs"
  if [ -n "$saved" ]; then
    mv "$saved/build" build
    rmdir "$saved"
  fi
}
trap restore EXIT

flags="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -g"
CFLAGS="$flags" CXXFLAGS="$flags" LDFLAGS="-fsanitize=address,undefined" npx node-gyp rebuild --jobs max >/dev/null

# Runs a command with the sanitizer runtimes preloaded. Only that command (and the processes it starts) gets them:
# preloaded into this shell, they would instrument every tool the script runs. ASan and LSan write their reports to
# $logs/report.<pid>; UBSan, a separate runtime here, ignores log_path and writes to stderr, so stderr is kept too.
# Leak reports don't change the exit status, because Node's own startup leaks a few bytes in every process. Every stack
# frame names its module (%M), so a report involving the addon always contains "poppler.node", also where the frame
# has a source location instead.
frames="stack_trace_format='    #%n %p %F %L %M'"
sanitized() {
  LD_PRELOAD="$(gcc -print-file-name=libasan.so):$(gcc -print-file-name=libubsan.so)" \
    ASAN_OPTIONS="detect_leaks=1:detect_odr_violation=0:alloc_dealloc_mismatch=0:log_path=$logs/report:$frames" \
    LSAN_OPTIONS="exitcode=0" \
    UBSAN_OPTIONS="print_stacktrace=1:$frames" \
    "$@"
}

failed=""
sanitized node scripts/sanitize-run.js "$@" > "$logs/run.out" 2>&1 || true
cat "$logs/run.out"
grep -q "^sanitize-run: " "$logs/run.out" || failed="$failed scripts/sanitize-run.js stopped early;"
# fuzz.mjs exits 1 when a mutated PDF makes a process die -- which every ASan and UBSan error does -- and keeps the
# start of that process's stderr in its summary.
if ! sanitized node scripts/fuzz.mjs --cases 25 --seed 1 > "$logs/fuzz.out" 2>&1; then
  failed="$failed scripts/fuzz.mjs found PDFs that crash (node scripts/fuzz.mjs --seed 1 --case <case> writes one);"
fi
echo "fuzz: $(tail -1 "$logs/fuzz.out" | cut -c1-3000)"

reports=$(cat "$logs"/report.* "$logs/run.out" 2>/dev/null || true)
# A leak report is a block of frames; keep the blocks that mention the addon.
ours=$(printf '%s\n' "$reports" | awk 'BEGIN{RS="\n\n"} /poppler\.node/' || true)
errors=$(printf '%s\n' "$reports" | grep -E "ERROR: AddressSanitizer|runtime error:" || true)
if [ -n "$errors" ] || [ -n "$ours" ]; then
  printf '%s\n' "$errors"
  printf '%s\n' "$ours" | sed -n '1,200p'
  failed="$failed sanitizer reports;"
fi
if [ -n "$failed" ]; then
  echo "sanitize.sh:$failed see above" >&2
  exit 1
fi
echo "sanitize.sh: no AddressSanitizer, UndefinedBehaviorSanitizer or LeakSanitizer findings in the addon"
