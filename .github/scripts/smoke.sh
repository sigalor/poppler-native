#!/bin/sh
# Installs the packed tarball into an empty project, the way a user would, and reads PDFs with it: on the command line,
# from CommonJS and as an ES module. Runs in images without a compiler, so it passes only if a prebuilt binary was
# used. In an image without Node.js (Rocky Linux), NODE_MAJOR names the version to install first.
#   .github/scripts/smoke.sh <repository checkout, with the poppler-native-*.tgz in it>
set -eu
repo=$(cd "$1" && pwd)
tarball=$(ls "$repo"/poppler-native-*.tgz)

if ! command -v node >/dev/null; then
  { command -v curl && command -v tar && command -v gzip; } >/dev/null || dnf install -y -q curl tar gzip
  arch=$(uname -m | sed -e 's/x86_64/x64/' -e 's/aarch64/arm64/')
  file=$(curl -fsSL "https://nodejs.org/dist/latest-v$NODE_MAJOR.x/SHASUMS256.txt" | grep -oE "node-v[0-9.]+-linux-$arch\.tar\.gz" | head -1)
  curl -fsSL "https://nodejs.org/dist/latest-v$NODE_MAJOR.x/$file" | tar -xz -C /opt
  export PATH="/opt/${file%.tar.gz}/bin:$PATH"
fi
echo "smoke: Node.js $(node --version) on $(uname -sm)"

project=$(mktemp -d)
cd "$project"
npm init -y >/dev/null
npm install --no-audit --no-fund "$tarball"
test ! -e node_modules/poppler-native/build || { echo "smoke: something was compiled" >&2; exit 1; }

expected=$(printf 'Page 1\nVisit\nGo to page 3\n\n\fPage 2\n\n\fPage 3\n\n\f')
actual=$(npx poppler-native "$repo/test/fixtures/three-pages.pdf" --text)
[ "$actual" = "$expected" ] || { echo "smoke: unexpected text: $actual" >&2; exit 1; }

node -e "
  const pdf = require('poppler-native');
  pdf.info(process.argv[1]).then(d => {
    if (d.numPages !== 1 || d.pages[0].images.length !== 5) throw new Error('unexpected result: ' + JSON.stringify(d).slice(0, 300));
  });
" "$repo/test/fixtures/images.pdf"

node --input-type=module -e "
  import { info } from 'poppler-native';
  const d = await info(process.argv[1], { images: false });
  if (d.outline[0].title !== 'Chapter 1 – Überblick') throw new Error('unexpected outline: ' + JSON.stringify(d.outline));
" "$repo/test/fixtures/three-pages.pdf"

echo "smoke: ok"
