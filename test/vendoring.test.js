const { execFileSync, spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const scripts = path.join(__dirname, '..', 'scripts');

// The vendoring scripts work in a temporary directory, which must be gone also when they stop with an error.
// (Maintainer tools: they need tar, run on Linux and macOS.)
const describeOnPosix = process.platform === 'win32' ? describe.skip : describe;

describeOnPosix('the vendoring scripts, when they fail', () => {
  let tmp;
  let work;
  beforeEach(() => {
    tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-vendoring-'));
    work = path.join(tmp, 'tmpdir');
    fs.mkdirSync(work);
  });
  afterEach(() => fs.rmSync(tmp, { recursive: true, force: true }));
  const run = (script, args) =>
    spawnSync(process.execPath, [path.join(scripts, script), ...args], { encoding: 'utf8', env: { ...process.env, TMPDIR: work } });

  test('update-poppler says why and leaves no temporary directory behind', () => {
    const notPoppler = path.join(tmp, 'not-poppler.tar.xz');
    execFileSync('tar', ['-cJf', notPoppler, '-C', __dirname, 'fixtures/README.md']);
    const result = run('update-poppler.mjs', ['26.11.0', '--tarball', notPoppler, '--skip-signature']);
    expect(result.status).toBe(1);
    expect(result.stderr).toContain(`update-poppler: ${notPoppler} is not poppler-26.11.0`);
    expect(fs.readdirSync(work)).toEqual([]);
  });

  test('vendor-deps says why and leaves no temporary directory behind', () => {
    const archives = path.join(tmp, 'archives');
    fs.mkdirSync(archives);
    fs.writeFileSync(path.join(archives, 'libpng-1.6.59.tar.xz'), 'not the release');
    const result = run('vendor-deps.mjs', ['libpng', '--dir', archives]);
    expect(result.status).toBe(1);
    expect(result.stderr).toMatch(/^vendor-deps: libpng: sha256 \w+ does not match the pinned \w+$/m);
    expect(result.stderr).not.toContain('vendor-deps: vendor-deps:');
    expect(fs.readdirSync(work)).toEqual([]);
  });
});
