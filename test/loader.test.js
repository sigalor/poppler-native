const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const root = path.join(__dirname, '..');

// Package managers that skip install scripts (pnpm 10+, Bun) leave a platform without a prebuilt binary with no
// binary at all. Loading the addon must then say what happened and how to compile it, not just that nothing was found.
test('a missing binary explains how to get one', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-loader-'));
  try {
    fs.mkdirSync(path.join(dir, 'lib'));
    fs.copyFileSync(path.join(root, 'lib', 'native.js'), path.join(dir, 'lib', 'native.js'));
    fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({ name: 'poppler-native' }));
    fs.symlinkSync(path.join(root, 'node_modules'), path.join(dir, 'node_modules'), 'junction');
    const result = spawnSync(process.execPath, ['-e', `require(${JSON.stringify(path.join(dir, 'lib', 'native.js'))})`], {
      encoding: 'utf8',
    });
    expect(result.status).not.toBe(0);
    expect(result.stderr).toContain(`poppler-native has no prebuilt binary for ${process.platform}-${process.arch}`);
    expect(result.stderr).toContain('npm rebuild poppler-native');
    expect(result.stderr).toContain('No native build was found'); // node-gyp-build's own details stay
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
