const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

// scripts/sanitize.sh moves the existing build aside and puts it back at the end. A run that was killed can't put it
// back (no trap runs on SIGKILL): the next run must say so instead of replacing that build. It is a bash script for
// Linux and macOS (with glibc: AddressSanitizer doesn't support musl), so this needs bash.
const hasBash = process.platform !== 'win32' && spawnSync('bash', ['-c', 'true']).status === 0;
(hasBash ? test : test.skip)('sanitize.sh refuses to start while an earlier run left a build aside', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-sanitize-'));
  try {
    fs.mkdirSync(path.join(dir, 'scripts'));
    fs.copyFileSync(path.join(__dirname, '..', 'scripts', 'sanitize.sh'), path.join(dir, 'scripts', 'sanitize.sh'));
    const stranded = path.join(dir, '.build-before-sanitize.abc123', 'build');
    fs.mkdirSync(stranded, { recursive: true });
    fs.mkdirSync(path.join(dir, 'build'));
    const result = spawnSync('bash', [path.join(dir, 'scripts', 'sanitize.sh')], { encoding: 'utf8' });
    expect(result.status).toBe(1);
    expect(result.stderr).toContain(`the build from before an interrupted run is still in ${path.dirname(stranded)}`);
    expect(fs.existsSync(stranded)).toBe(true); // nothing moved
    expect(fs.existsSync(path.join(dir, 'build'))).toBe(true);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
