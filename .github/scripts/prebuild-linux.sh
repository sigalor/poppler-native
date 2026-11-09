#!/bin/sh
# Builds and tests the Linux prebuilt binary inside a container: a manylinux_2_28 image for glibc (binaries that run on
# glibc 2.28 and newer: RHEL/Alma/Rocky 8+, Debian 10+, Ubuntu 20.04+) or an Alpine image for musl.
#   .github/scripts/prebuild-linux.sh glibc|musl     (run from the repository root)
set -eu
libc=$1
export JOBS=max # node-gyp compiles on every core; prebuildify alone would use one

if [ "$libc" = glibc ]; then
  arch=$(uname -m | sed -e 's/x86_64/x64/' -e 's/aarch64/arm64/')
  file=$(curl -fsSL https://nodejs.org/dist/latest-v24.x/SHASUMS256.txt | grep -oE "node-v[0-9.]+-linux-$arch\.tar\.gz" | head -1)
  curl -fsSL "https://nodejs.org/dist/latest-v24.x/$file" | tar -xz -C /opt
  export PATH="/opt/${file%.tar.gz}/bin:/opt/python/cp312-cp312/bin:$PATH"
else
  apk add --no-cache build-base python3
  # Alpine releases ship different libstdc++ versions; with the C++ runtime linked in, the binary runs on all of them.
  export LDFLAGS="-static-libstdc++ -static-libgcc"
fi
gcc --version | head -1
node --version

npm ci --ignore-scripts --no-audit --no-fund
npx prebuildify --napi --strip --tag-libc
ls -lR prebuilds
binary=$(ls prebuilds/*/*.node)
readelf -d "$binary" | grep NEEDED

if [ "$libc" = glibc ]; then
  # manylinux_2_28's compiler links what a newer libstdc++ has into the binary itself, so it runs on glibc 2.28 with
  # that system's own libstdc++ (GCC 8). Check that no newer symbol version slipped in.
  for limit in GLIBC_2.28 GLIBCXX_3.4.25 CXXABI_1.3.11; do
    newest=$(objdump -T "$binary" | grep -oE "${limit%_*}_[0-9.]+" | sort -uV | tail -1)
    echo "needs ${newest:-no ${limit%_*}} (at most $limit)"
    if [ -n "$newest" ] && [ "$(printf '%s\n%s\n' "$newest" "$limit" | sort -V | tail -1)" != "$limit" ]; then
      echo "prebuild-linux.sh: $binary needs $newest, newer than $limit" >&2
      exit 1
    fi
  done
else
  if readelf -d "$binary" | grep -E "NEEDED.*(libstdc\+\+|libgcc_s)"; then
    echo "prebuild-linux.sh: $binary should not need the shared C++ runtime" >&2
    exit 1
  fi
fi

# the tests run against the prebuilt binary: node-gyp-build prefers build/, so there must not be one
rm -rf build
npm test
